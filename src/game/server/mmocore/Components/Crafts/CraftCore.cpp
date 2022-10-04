/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "CraftCore.h"

#include <game/server/gamecontext.h>
#include <teeother/system/string.h>

void CCraftCore::OnInit()
{
	ResultPtr pRes = Sqlpool.Execute<DB::SELECT>("*", "tw_crafts_list");
	while(pRes->next())
	{
		const int ID = pRes->getInt("ID");
		CCraftData::ms_aCraft[ID].m_ItemID = pRes->getInt("ItemID");
		CCraftData::ms_aCraft[ID].m_ItemValue = pRes->getInt("ItemValue");
		CCraftData::ms_aCraft[ID].m_Price = pRes->getInt("Price");
		CCraftData::ms_aCraft[ID].m_WorldID = pRes->getInt("WorldID");

		char aBuf[32];
		for(int i = 0; i < 3; i ++)
		{
			str_format(aBuf, sizeof(aBuf), "RequiredItemID%d", i);
			CCraftData::ms_aCraft[ID].m_aRequiredItemID[i] = pRes->getInt(aBuf);
		}
		str_copy(aBuf, pRes->getString("RequiredItemsValues").c_str(), sizeof(aBuf));
		if (!sscanf(aBuf, "%d %d %d", &CCraftData::ms_aCraft[ID].m_aRequiredItemsValues[0],
		            &CCraftData::ms_aCraft[ID].m_aRequiredItemsValues[1], &CCraftData::ms_aCraft[ID].m_aRequiredItemsValues[2]))
			dbg_msg("Error", "Error on scanf in Crafting");
	}

	Job()->ShowLoadingProgress("Crafts", CCraftData::ms_aCraft.size());
}

bool CCraftCore::OnHandleTile(CCharacter* pChr, int IndexCollision)
{
	CPlayer* pPlayer = pChr->GetPlayer();
	const int ClientID = pPlayer->GetCID();

	if (pChr->GetHelper()->TileEnter(IndexCollision, TILE_CRAFT_ZONE))
	{
		GS()->Chat(ClientID, "You can see menu in the votes!");
		pChr->m_Core.m_HookHitDisabled = pChr->m_SkipDamage = true;
		GS()->ResetVotes(ClientID, pPlayer->m_OpenVoteMenu);
		return true;
	}
	else if (pChr->GetHelper()->TileExit(IndexCollision, TILE_CRAFT_ZONE))
	{
		GS()->Chat(ClientID, "You left the active zone, menu is restored!");
		pChr->m_Core.m_HookHitDisabled = pChr->m_SkipDamage = false;
		GS()->ResetVotes(ClientID, pPlayer->m_OpenVoteMenu);
		return true;
	}
	return false;
}

int CCraftCore::GetFinalPrice(CPlayer* pPlayer, int CraftID) const
{
	if(!pPlayer)
		return CCraftData::ms_aCraft[CraftID].m_Price;

	int Discount = translate_to_percent_rest(CCraftData::ms_aCraft[CraftID].m_Price, pPlayer->GetSkill(SkillCraftDiscount)->GetLevel());
	if(pPlayer->GetItem(itTicketDiscountCraft)->IsEquipped())
		Discount += translate_to_percent_rest(CCraftData::ms_aCraft[CraftID].m_Price, 20);

	return max(CCraftData::ms_aCraft[CraftID].m_Price - Discount, 0);
}

void CCraftCore::ShowCraftList(CPlayer* pPlayer, const char* TypeName, ItemType Type) const
{
	bool IsNotEmpty = false;
	const int ClientID = pPlayer->GetCID();

	for(const auto& [CraftID, CraftData] : CCraftData::ms_aCraft)
	{
		CItemDescription* pCraftItemInfo = GS()->GetItemInfo(CraftData.m_ItemID);
		if(pCraftItemInfo->GetType() != Type || CraftData.m_WorldID != GS()->GetWorldID())
			continue;

		if(!IsNotEmpty)
		{
			GS()->AVL(ClientID, "null", "{STR}", TypeName);
			IsNotEmpty = true;
		}

		const int Price = GetFinalPrice(pPlayer, CraftID);
		const int HideID = NUM_TAB_MENU + CItemDescription::Data().size() + CraftID;
		if (pCraftItemInfo->IsEnchantable())
		{
			GS()->AVH(ClientID, HideID, "{STR}{STR} - {VAL} gold", (pPlayer->GetItem(CraftData.m_ItemID)->GetValue() ? "✔ " : "\0"), pCraftItemInfo->GetName(), Price);

			char aAttributes[128];
			pCraftItemInfo->StrFormatAttributes(pPlayer, aAttributes, sizeof(aAttributes), 0);
			GS()->AVM(ClientID, "null", NOPE, HideID, "{STR}", aAttributes);
		}
		else
		{
			GS()->AVH(ClientID, HideID, "{STR}x{VAL} ({VAL}) :: {VAL} gold", pCraftItemInfo->GetName(), CraftData.m_ItemValue, pPlayer->GetItem(CraftData.m_ItemID)->GetValue(), Price);
		}
		GS()->AVM(ClientID, "null", NOPE, HideID, "{STR}", pCraftItemInfo->GetDesc());

		for(int i = 0; i < 3; i++)
		{
			const int RequiredItemID = CraftData.m_aRequiredItemID[i];
			const int RequiredValue = CraftData.m_aRequiredItemsValues[i];
			if(RequiredItemID <= 0 || RequiredValue <= 0)
				continue;

			CPlayerItem* pPlayerItem = pPlayer->GetItem(RequiredItemID);
			GS()->AVM(ClientID, "null", NOPE, HideID, "{STR} {VAL}({VAL})", pPlayerItem->Info()->GetName(), RequiredValue, pPlayerItem->GetValue());
		}
		GS()->AVM(ClientID, "CRAFT", CraftID, HideID, "Craft {STR}", pCraftItemInfo->GetName());
	}

	if(IsNotEmpty)
		GS()->AV(ClientID, "null");
}

void CCraftCore::CraftItem(CPlayer *pPlayer, int CraftID) const
{
	const int ClientID = pPlayer->GetCID();
	CPlayerItem* pPlayerCraftItem = pPlayer->GetItem(CCraftData::ms_aCraft[CraftID].m_ItemID);
	if (pPlayerCraftItem->Info()->IsEnchantable() && pPlayerCraftItem->GetValue() > 0)
	{
		GS()->Chat(ClientID, "Enchant item maximal count x1 in a backpack!");
		return;
	}

	// first podding set what is available and required for removal
	dynamic_string Buffer;
	for(int i = 0; i < 3; i++)
	{
		const int SearchItemID = CCraftData::ms_aCraft[CraftID].m_aRequiredItemID[i];
		const int SearchValue = CCraftData::ms_aCraft[CraftID].m_aRequiredItemsValues[i];
		if(SearchItemID <= 0 || SearchValue <= 0 || pPlayer->GetItem(SearchItemID)->GetValue() >= SearchValue)
			continue;

		const int ItemLeft = (SearchValue - pPlayer->GetItem(SearchItemID)->GetValue());
		GS()->Server()->Localization()->Format(Buffer, pPlayer->GetLanguage(), "{STR}x{VAL} ", GS()->GetItemInfo(SearchItemID)->GetName(), ItemLeft);
	}
	if(Buffer.length() > 0)
	{
		GS()->Chat(ClientID, "Item left: {STR}", Buffer.buffer());
		Buffer.clear();
		return;
	}

	// we are already organizing the crafting
	const int Price = GetFinalPrice(pPlayer, CraftID);
	if(!pPlayer->SpendCurrency(Price))
		return;

	// delete ticket if equipped
	const bool TickedDiscountCraft = pPlayer->GetItem(itTicketDiscountCraft)->IsEquipped();
	if(TickedDiscountCraft)
	{
		pPlayer->GetItem(itTicketDiscountCraft)->Remove(1);
		GS()->Chat(ClientID, "You used item {STR} and get discount 25%.", GS()->GetItemInfo(itTicketDiscountCraft)->GetName());
	}

	for(int i = 0; i < 3; i++)
	{
		const int SearchItemID = CCraftData::ms_aCraft[CraftID].m_aRequiredItemID[i];
		const int SearchValue = CCraftData::ms_aCraft[CraftID].m_aRequiredItemsValues[i];
		if(SearchItemID <= 0 || SearchValue <= 0)
			continue;

		pPlayer->GetItem(SearchItemID)->Remove(SearchValue);
	}

	const int CraftGetValue = CCraftData::ms_aCraft[CraftID].m_ItemValue;
	pPlayerCraftItem->Add(CraftGetValue);
	if(pPlayerCraftItem->Info()->IsEnchantable())
	{
		GS()->Chat(-1, "{STR} crafted [{STR}x{VAL}].", Server()->ClientName(ClientID), pPlayerCraftItem->Info()->GetName(), CraftGetValue);
		return;
	}

	GS()->Chat(ClientID, "You crafted [{STR}x{VAL}].", pPlayerCraftItem->Info()->GetName(), CraftGetValue);
	GS()->ResetVotes(ClientID, pPlayer->m_OpenVoteMenu);
}

bool CCraftCore::OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, const int VoteID, const int VoteID2, int Get, const char* GetText)
{
	if(PPSTR(CMD, "CRAFT") == 0)
	{
		CraftItem(pPlayer, VoteID);
		return true;
	}

	return false;
}

bool CCraftCore::OnHandleMenulist(CPlayer* pPlayer, int Menulist, bool ReplaceMenu)
{
	const int ClientID = pPlayer->GetCID();
	if (ReplaceMenu)
	{
		CCharacter* pChr = pPlayer->GetCharacter();
		if (!pChr || !pChr->IsAlive())
			return false;

		if (pChr->GetHelper()->BoolIndex(TILE_CRAFT_ZONE))
		{
			GS()->AVH(ClientID, TAB_INFO_CRAFT, "Crafting Information");
			GS()->AVM(ClientID, "null", NOPE, TAB_INFO_CRAFT, "If you will not have enough items for crafting");
			GS()->AVM(ClientID, "null", NOPE, TAB_INFO_CRAFT, "You will write those and the amount that is still required");
			GS()->AV(ClientID, "null");
			GS()->ShowVotesItemValueInformation(pPlayer);
			GS()->AV(ClientID, "null");

			ShowCraftList(pPlayer, "Craft | Can be used's", ItemType::TYPE_USED);
			ShowCraftList(pPlayer, "Craft | Potion's", ItemType::TYPE_POTION);
			ShowCraftList(pPlayer, "Craft | Equipment's", ItemType::TYPE_EQUIP);
			ShowCraftList(pPlayer, "Craft | Module's", ItemType::TYPE_MODULE);
			ShowCraftList(pPlayer, "Craft | Decoration's", ItemType::TYPE_DECORATION);
			ShowCraftList(pPlayer, "Craft | Other's", ItemType::TYPE_OTHER);
			return true;
		}
		return false;
	}

	return false;
}
