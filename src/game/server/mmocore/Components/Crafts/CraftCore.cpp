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
		int ItemID = pRes->getInt("ItemID");
		int ItemValue = pRes->getInt("ItemValue");
		int Price = pRes->getInt("Price");
		int WorldID = pRes->getInt("WorldID");

		CraftIdentifier ID = pRes->getInt("ID");

		std::string JsonRequiredData = pRes->getString("RequiredItems").c_str();
		CCraftItem(ID).Init(CItem::FromArrayJSON(JsonRequiredData), CItem(ItemID, ItemValue), Price, WorldID);
	}

	Job()->ShowLoadingProgress("Crafts", (int)CCraftItem::Data().size());
}

bool CCraftCore::OnHandleTile(CCharacter* pChr, int IndexCollision)
{
	CPlayer* pPlayer = pChr->GetPlayer();
	const int ClientID = pPlayer->GetCID();

	if (pChr->GetHelper()->TileEnter(IndexCollision, TILE_CRAFT_ZONE))
	{
		GS()->Chat(ClientID, "You can see menu in the votes!");
		GS()->UpdateVotes(ClientID, pPlayer->m_OpenVoteMenu);
		return true;
	}
	else if (pChr->GetHelper()->TileExit(IndexCollision, TILE_CRAFT_ZONE))
	{
		GS()->Chat(ClientID, "You left the active zone, menu is restored!");
		GS()->UpdateVotes(ClientID, pPlayer->m_OpenVoteMenu);
		return true;
	}
	return false;
}

void CCraftCore::ShowCraftList(CPlayer* pPlayer, const char* TypeName, ItemType Type) const
{
	const int ClientID = pPlayer->GetCID();
	std::function pFuncNewTab = [&](){ GS()->AVL(ClientID, "null", "{STR}", TypeName); };

	for(const auto& [ID, Craft] : CCraftItem::Data())
	{
		CItemDescription* pCraftItemInfo = Craft.GetItem()->Info();
		if(pCraftItemInfo->GetType() != Type || Craft.GetWorldID() != GS()->GetWorldID())
			continue;

		pFuncNewTab();
		pFuncNewTab = nullptr;

		ItemIdentifier ItemID = Craft.GetItem()->GetID();
		const int Price = Craft.GetPrice(pPlayer);
		const int HideID = NUM_TAB_MENU + CItemDescription::Data().size() + ID;

		if(pCraftItemInfo->IsEnchantable())
		{
			GS()->AVH(ClientID, HideID, "{STR}{STR} - {VAL} gold", (pPlayer->GetItem(ItemID)->GetValue() ? "✔ " : "\0"), pCraftItemInfo->GetName(), Price);

			char aAttributes[128];
			pCraftItemInfo->StrFormatAttributes(pPlayer, aAttributes, sizeof(aAttributes), 0);
			GS()->AVM(ClientID, "null", NOPE, HideID, "{STR}", aAttributes);
		}
		else
		{
			GS()->AVH(ClientID, HideID, "{STR}x{VAL} ({VAL}) :: {VAL} gold", pCraftItemInfo->GetName(), Craft.GetItem()->GetValue(), pPlayer->GetItem(ItemID)->GetValue(), Price);
		}
		GS()->AVM(ClientID, "null", NOPE, HideID, "{STR}", pCraftItemInfo->GetDescription());

		for(auto& RequiredItem: Craft.m_RequiredItem)
		{
			CPlayerItem* pPlayerItem = pPlayer->GetItem(RequiredItem.GetID());
			GS()->AVM(ClientID, "null", NOPE, HideID, "# {STR} {VAL}({VAL})", pPlayerItem->Info()->GetName(), RequiredItem.GetValue(), pPlayerItem->GetValue());
		}

		GS()->AVM(ClientID, "CRAFT", ID, HideID, "Craft {STR}", pCraftItemInfo->GetName());
	}

	if(!pFuncNewTab)
		GS()->AV(ClientID, "null");
}

void CCraftCore::CraftItem(CPlayer *pPlayer, int CraftID) const
{
	const int ClientID = pPlayer->GetCID();
	CCraftItem* pCraft = &CCraftItem::Data()[CraftID];
	CPlayerItem* pPlayerCraftItem = pPlayer->GetItem(*pCraft->GetItem());

	// check enchant
	if (pPlayerCraftItem->Info()->IsEnchantable() && pPlayerCraftItem->GetValue() > 0)
	{
		GS()->Chat(ClientID, "Enchant item maximal count x1 in a backpack!");
		return;
	}

	// first podding set what is available and required for removal
	dynamic_string Buffer;
	for(auto& RequiredItem : pCraft->m_RequiredItem)
	{
		if(pPlayer->GetItem(RequiredItem)->GetValue() < RequiredItem.GetValue())
		{
			const int ItemLeft = (RequiredItem.GetValue() - pPlayer->GetItem(RequiredItem)->GetValue());
			GS()->Server()->Localization()->Format(Buffer, pPlayer->GetLanguage(), "{STR}x{VAL} ", RequiredItem.Info()->GetName(), ItemLeft);
		}
	}
	if(Buffer.length() > 0)
	{
		GS()->Chat(ClientID, "Item left: {STR}", Buffer.buffer());
		Buffer.clear();
		return;
	}

	// we are already organizing the crafting
	const int Price = pCraft->GetPrice(pPlayer);
	if(!pPlayer->SpendCurrency(Price))
		return;

	// delete ticket if equipped
	if(pPlayer->GetItem(itTicketDiscountCraft)->IsEquipped())
	{
		pPlayer->GetItem(itTicketDiscountCraft)->Remove(1);
		GS()->Chat(ClientID, "You used item {STR} and get discount 25%.", GS()->GetItemInfo(itTicketDiscountCraft)->GetName());
	}

	// action get and remove
	for(auto& RequiredItem : pCraft->m_RequiredItem)
	{
		pPlayer->GetItem(RequiredItem)->Remove(RequiredItem.GetValue());
	}

	const int CraftGetValue = pCraft->GetItem()->GetValue();
	pPlayerCraftItem->Add(CraftGetValue);
	if(pPlayerCraftItem->Info()->IsEnchantable())
	{
		GS()->Chat(-1, "{STR} crafted [{STR}x{VAL}].", Server()->ClientName(ClientID), pPlayerCraftItem->Info()->GetName(), CraftGetValue);
	}
	else
	{
		GS()->Chat(ClientID, "You crafted [{STR}x{VAL}].", pPlayerCraftItem->Info()->GetName(), CraftGetValue);
	}

	GS()->UpdateVotes(ClientID, pPlayer->m_OpenVoteMenu);
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
