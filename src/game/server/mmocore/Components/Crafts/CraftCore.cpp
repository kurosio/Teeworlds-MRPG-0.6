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
		pChr->m_Core.m_NoHookable = pChr->m_SkipDamage = true;
		GS()->ResetVotes(ClientID, pPlayer->m_OpenVoteMenu);
		return true;
	}
	else if (pChr->GetHelper()->TileExit(IndexCollision, TILE_CRAFT_ZONE))
	{
		GS()->Chat(ClientID, "You left the active zone, menu is restored!");
		pChr->m_Core.m_NoHookable = pChr->m_SkipDamage = false;
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
	if(pPlayer->GetItem(itTicketDiscountCraft).IsEquipped())
		Discount += translate_to_percent_rest(CCraftData::ms_aCraft[CraftID].m_Price, 20);

	return max(CCraftData::ms_aCraft[CraftID].m_Price - Discount, 0);
}

void CCraftCore::ShowCraftList(CPlayer* pPlayer, const char* TypeName, int SelectType) const
{
	bool IsNotEmpty = false;
	const int ClientID = pPlayer->GetCID();

	for(const auto& cr : CCraftData::ms_aCraft)
	{
		CItemDataInfo& InfoGetItem = GS()->GetItemInfo(cr.second.m_ItemID);
		if(InfoGetItem.m_Type != SelectType || cr.second.m_WorldID != GS()->GetWorldID())
			continue;

		if(!IsNotEmpty)
		{
			GS()->AVL(ClientID, "null", "{STR}", TypeName);
			IsNotEmpty = true;
		}

		const int Price = GetFinalPrice(pPlayer, cr.first);
		const int HideID = NUM_TAB_MENU + CItemDataInfo::ms_aItemsInfo.size() + cr.first;
		if (InfoGetItem.IsEnchantable())
		{
			GS()->AVH(ClientID, HideID, "{STR}{STR} - {VAL} gold", (pPlayer->GetItem(cr.second.m_ItemID).m_Value ? "✔ " : "\0"), InfoGetItem.GetName(), Price);

			char aAttributes[128];
			InfoGetItem.FormatAttributes(pPlayer, aAttributes, sizeof(aAttributes), 0);
			GS()->AVM(ClientID, "null", NOPE, HideID, "{STR}", aAttributes);
		}
		else
		{
			GS()->AVH(ClientID, HideID, "{STR}x{VAL} ({VAL}) :: {VAL} gold", InfoGetItem.GetName(), cr.second.m_ItemValue, pPlayer->GetItem(cr.second.m_ItemID).m_Value, Price);
		}
		GS()->AVM(ClientID, "null", NOPE, HideID, "{STR}", InfoGetItem.GetDesc());

		for(int i = 0; i < 3; i++)
		{
			const int SearchItemID = cr.second.m_aRequiredItemID[i];
			const int SearchValue = cr.second.m_aRequiredItemsValues[i];
			if(SearchItemID <= 0 || SearchValue <= 0)
				continue;

			CItemData &PlSearchItem = pPlayer->GetItem(SearchItemID);
			GS()->AVM(ClientID, "null", NOPE, HideID, "{STR} {VAL}({VAL})", PlSearchItem.Info().GetName(), SearchValue, PlSearchItem.m_Value);
		}
		GS()->AVM(ClientID, "CRAFT", cr.first, HideID, "Craft {STR}", InfoGetItem.GetName());
	}

	if(IsNotEmpty)
		GS()->AV(ClientID, "null");
}

void CCraftCore::CraftItem(CPlayer *pPlayer, int CraftID) const
{
	const int ClientID = pPlayer->GetCID();
	CItemData& PlayerCraftItem = pPlayer->GetItem(CCraftData::ms_aCraft[CraftID].m_ItemID);
	if (PlayerCraftItem.Info().IsEnchantable() && PlayerCraftItem.m_Value > 0)
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
		if(SearchItemID <= 0 || SearchValue <= 0 || pPlayer->GetItem(SearchItemID).m_Value >= SearchValue)
			continue;

		const int ItemLeft = (SearchValue - pPlayer->GetItem(SearchItemID).m_Value);
		GS()->Server()->Localization()->Format(Buffer, pPlayer->GetLanguage(), "{STR}x{VAL} ", GS()->GetItemInfo(SearchItemID).GetName(), ItemLeft);
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
	const bool TickedDiscountCraft = pPlayer->GetItem(itTicketDiscountCraft).IsEquipped();
	if(TickedDiscountCraft)
	{
		pPlayer->GetItem(itTicketDiscountCraft).Remove(1);
		GS()->Chat(ClientID, "You used item {STR} and get discount 25%.", GS()->GetItemInfo(itTicketDiscountCraft).GetName());
	}

	for(int i = 0; i < 3; i++)
	{
		const int SearchItemID = CCraftData::ms_aCraft[CraftID].m_aRequiredItemID[i];
		const int SearchValue = CCraftData::ms_aCraft[CraftID].m_aRequiredItemsValues[i];
		if(SearchItemID <= 0 || SearchValue <= 0)
			continue;

		pPlayer->GetItem(SearchItemID).Remove(SearchValue);
	}

	const int CraftGetValue = CCraftData::ms_aCraft[CraftID].m_ItemValue;
	PlayerCraftItem.Add(CraftGetValue);
	if(PlayerCraftItem.Info().IsEnchantable())
	{
		GS()->Chat(-1, "{STR} crafted [{STR}x{VAL}].", Server()->ClientName(ClientID), PlayerCraftItem.Info().GetName(), CraftGetValue);
		return;
	}

	GS()->CreatePlayerSound(ClientID, SOUND_ITEM_SELL_BUY);
	GS()->Chat(ClientID, "You crafted [{STR}x{VAL}].", PlayerCraftItem.Info().GetName(), CraftGetValue);
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

			ShowCraftList(pPlayer, "Craft | Can be used's", TYPE_USED);
			ShowCraftList(pPlayer, "Craft | Potion's", TYPE_POTION);
			ShowCraftList(pPlayer, "Craft | Equipment's", TYPE_EQUIP);
			ShowCraftList(pPlayer, "Craft | Module's", TYPE_MODULE);
			ShowCraftList(pPlayer, "Craft | Decoration's", TYPE_DECORATION);
			ShowCraftList(pPlayer, "Craft | Other's", TYPE_OTHER);
			return true;
		}
		return false;
	}

	return false;
}
