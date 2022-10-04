/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "ShopCore.h"

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>

#include <game/server/mmocore/Components/Inventory/InventoryCore.h>
#include <game/server/mmocore/Components/Storages/StorageCore.h>

using namespace sqlstr;
void CShopCore::OnInit()
{
	ResultPtr pRes = Sqlpool.Execute<DB::SELECT>("ID, StorageID", "tw_store_items");
	while(pRes->next())
	{
		int ID = pRes->getInt("ID");
		CShop::ms_aShopList[ID].m_StorageID = pRes->getInt("StorageID");
	}
}

void CShopCore::OnTick()
{
	if(GS()->GetWorldID() == MAIN_WORLD_ID)
	{
		if(Server()->Tick() % (1 * Server()->TickSpeed() * (g_Config.m_SvTimeCheckAuction * 60)) == 0)
			CheckAuctionTime();
	}
}

bool CShopCore::OnHandleTile(CCharacter* pChr, int IndexCollision)
{
	CPlayer* pPlayer = pChr->GetPlayer();
	const int ClientID = pPlayer->GetCID();
	if (pChr->GetHelper()->TileEnter(IndexCollision, TILE_AUCTION))
	{
		GS()->Chat(ClientID, "You can see menu in the votes!");
		pChr->m_Core.m_HookHitDisabled = pChr->m_SkipDamage = true;
		GS()->ResetVotes(ClientID, pPlayer->m_OpenVoteMenu);
		return true;
	}

	if (pChr->GetHelper()->TileExit(IndexCollision, TILE_AUCTION))
	{
		GS()->Chat(ClientID, "You left the active zone, menu is restored!");
		pChr->m_Core.m_HookHitDisabled = pChr->m_SkipDamage = false;
		GS()->ResetVotes(ClientID, pPlayer->m_OpenVoteMenu);
		return true;
	}

	return false;
}

bool CShopCore::OnHandleMenulist(CPlayer* pPlayer, int Menulist, bool ReplaceMenu)
{
	const int ClientID = pPlayer->GetCID();
	if(ReplaceMenu)
	{
		CCharacter* pChr = pPlayer->GetCharacter();
		if(!pChr || !pChr->IsAlive())
			return false;

		if(pChr->GetHelper()->BoolIndex(TILE_AUCTION))
		{
			ShowAuction(pPlayer);
			return true;
		}

		if(pChr->GetHelper()->BoolIndex(TILE_SHOP_ZONE))
		{
			const int StorageID = Job()->Storage()->GetStorageID(pChr->m_Core.m_Pos);
			Job()->Storage()->ShowStorageMenu(pChr->GetPlayer(), StorageID);
			ShowMailShop(pPlayer, StorageID);
			return true;
		}
		return false;
	}


	if(Menulist == MenuList::MENU_AUCTION_CREATE_SLOT)
	{
		pPlayer->m_LastVoteMenu = MenuList::MENU_INVENTORY;
		const int ItemID = pPlayer->GetTempData().m_SellItem.m_ItemID;
		CItemDescription* pAuctionItemInfo = GS()->GetItemInfo(ItemID);

		const int SlotValue = pPlayer->GetTempData().m_SellItem.m_Value;
		const int MinimalPrice = SlotValue * pAuctionItemInfo->GetInitialPrice();
		const int SlotPrice = pPlayer->GetTempData().m_SellItem.m_Price;
		const int SlotEnchant = pPlayer->GetTempData().m_SellItem.m_Enchant;

		GS()->AVH(ClientID, TAB_INFO_AUCTION_BIND, "Information Auction Slot");
		GS()->AVM(ClientID, "null", NOPE, TAB_INFO_AUCTION_BIND, "The reason for write the number for each row");
		GS()->AVM(ClientID, "null", NOPE, NOPE, "Item x{VAL} Minimal Price: {VAL}gold", SlotValue, MinimalPrice);
		GS()->AVM(ClientID, "null", NOPE, NOPE, "Auction Slot Price: {VAL}gold", g_Config.m_SvAuctionPriceSlot);
		if(SlotEnchant > 0)
			GS()->AVM(ClientID, "null", NOPE, NOPE, "Warning selling enchanted: +{INT}", SlotEnchant);

		GS()->AVM(ClientID, "AUCTIONCOUNT", ItemID, NOPE, "Item Value: {VAL}", SlotValue);
		GS()->AVM(ClientID, "AUCTIONPRICE", ItemID, NOPE, "Item Price: {VAL}", SlotPrice);
		GS()->AVM(ClientID, "AUCTIONACCEPT", ItemID, NOPE, "Add {STR}x{VAL} {VAL}gold", pAuctionItemInfo->GetName(), SlotValue, SlotPrice);
		GS()->AddVotesBackpage(ClientID);
		return true;
	}
	return false;
}

bool CShopCore::OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, const int VoteID, const int VoteID2, int Get, const char* GetText)
{
	const int ClientID = pPlayer->GetCID();

	if(PPSTR(CMD, "SHOP") == 0)
	{
		if(BuyShopItem(pPlayer, VoteID))
			GS()->ResetVotes(ClientID, MenuList::MAIN_MENU);
		return true;
	}

	if(PPSTR(CMD, "AUCTIONCOUNT") == 0)
	{
		// if there are fewer items installed, we set the number of items.
		CPlayerItem* pPlayerItem = pPlayer->GetItem(VoteID);
		if(Get > pPlayerItem->GetValue())
			Get = pPlayerItem->GetValue();

		// if it is possible to number
		if(pPlayerItem->Info()->IsEnchantable())
			Get = 1;

		const int c_minimalprice = (Get * pPlayerItem->Info()->GetInitialPrice());
		if(pPlayer->GetTempData().m_SellItem.m_Price < c_minimalprice)
			pPlayer->GetTempData().m_SellItem.m_Price = c_minimalprice;

		pPlayer->GetTempData().m_SellItem.m_Value = Get;
		GS()->StrongUpdateVotes(ClientID, MenuList::MENU_AUCTION_CREATE_SLOT);
		return true;
	}

	if(PPSTR(CMD, "AUCTIONPRICE") == 0)
	{
		const int c_minimalprice = (pPlayer->GetTempData().m_SellItem.m_Value * GS()->GetItemInfo(VoteID)->GetInitialPrice());
		if(Get < c_minimalprice)
			Get = c_minimalprice;

		pPlayer->GetTempData().m_SellItem.m_Price = Get;
		GS()->StrongUpdateVotes(ClientID, MenuList::MENU_AUCTION_CREATE_SLOT);
		return true;
	}

	if(PPSTR(CMD, "AUCTIONSLOT") == 0)
	{
		int AvailableValue = Job()->Item()->GetUnfrozenItemValue(pPlayer, VoteID);
		if(AvailableValue <= 0)
			return true;

		pPlayer->GetTempData().m_SellItem.m_ItemID = VoteID;
		pPlayer->GetTempData().m_SellItem.m_Enchant = pPlayer->GetItem(VoteID)->GetEnchant();
		GS()->ResetVotes(ClientID, MenuList::MENU_AUCTION_CREATE_SLOT);
		return true;
	}

	if(PPSTR(CMD, "AUCTIONACCEPT") == 0)
	{
		CPlayerItem* pPlayerItem = pPlayer->GetItem(VoteID);
		if(pPlayerItem->GetValue() >= pPlayer->GetTempData().m_SellItem.m_Value && pPlayer->GetTempData().m_SellItem.m_Price >= 10)
		{
			CreateAuctionSlot(pPlayer, pPlayer->GetTempData().m_SellItem);
			GS()->ResetVotes(ClientID, MenuList::MENU_INVENTORY);
			return true;
		}
		GS()->StrongUpdateVotes(ClientID, MenuList::MENU_AUCTION_CREATE_SLOT);
		return true;
	}

	return false;
}

void CShopCore::CreateAuctionSlot(CPlayer* pPlayer, CAuctionItem& pAuctionItem)
{
	const int ItemID = pAuctionItem.m_ItemID;
	const int ClientID = pPlayer->GetCID();

	// check the number of slots whether everything is occupied or not
	ResultPtr pResCheck = Sqlpool.Execute<DB::SELECT>("ID", "tw_store_items", "WHERE UserID > '0' LIMIT %d", g_Config.m_SvMaxMasiveAuctionSlots);
	if((int)pResCheck->rowsCount() >= g_Config.m_SvMaxMasiveAuctionSlots)
		return GS()->Chat(ClientID, "Auction has run out of slots, wait for the release of slots!");

	// check your slots
	ResultPtr pResCheck2 = Sqlpool.Execute<DB::SELECT>("ID", "tw_store_items", "WHERE UserID = '%d' LIMIT %d", pPlayer->Acc().m_UserID, g_Config.m_SvMaxAuctionSlots);
	const int ValueSlot = pResCheck2->rowsCount();
	if(ValueSlot >= g_Config.m_SvMaxAuctionSlots)
		return GS()->Chat(ClientID, "You use all open the slots in your auction!");

	// we check if the item is in the auction
	ResultPtr pResCheck3 = Sqlpool.Execute<DB::SELECT>("ID", "tw_store_items", "WHERE ItemID = '%d' AND UserID = '%d'", ItemID, pPlayer->Acc().m_UserID);
	if(pResCheck3->next())
		return GS()->Chat(ClientID, "Your same item found in the database, need reopen the slot!");

	// if the money for the slot auction is withdrawn
	if(!pPlayer->SpendCurrency(g_Config.m_SvAuctionPriceSlot))
		return;

	// pick up the item and add a slot
	CPlayerItem* pPlayerItem = pPlayer->GetItem(ItemID);
	if(pPlayerItem->GetValue() >= pAuctionItem.m_Value && pPlayerItem->Remove(pAuctionItem.m_Value))
	{
		Sqlpool.Execute<DB::INSERT>("tw_store_items", "(ItemID, Price, ItemValue, UserID, Enchant) VALUES ('%d', '%d', '%d', '%d', '%d')",
			ItemID, pAuctionItem.m_Price, pAuctionItem.m_Value, pPlayer->Acc().m_UserID, pAuctionItem.m_Enchant);

		const int AvailableSlot = (g_Config.m_SvMaxAuctionSlots - ValueSlot) - 1;
		GS()->Chat(-1, "{STR} created a slot [{STR}x{VAL}] auction.",
			Server()->ClientName(ClientID), pPlayerItem->Info()->GetName(), pAuctionItem.m_Value);
		GS()->Chat(ClientID, "Still available {INT} slots!", AvailableSlot);
	}
}

void CShopCore::CheckAuctionTime()
{
	ResultPtr pRes = Sqlpool.Execute<DB::SELECT>("*", "tw_store_items", "WHERE UserID > 0 AND DATE_SUB(NOW(),INTERVAL %d MINUTE) > Time", g_Config.m_SvTimeAuctionSlot);
	int ReleaseSlots = (int)pRes->rowsCount();
	while(pRes->next())
	{
		const int ID = pRes->getInt("ID");
		const int ItemID = pRes->getInt("ItemID");
		const int Value = pRes->getInt("ItemValue");
		const int Enchant = pRes->getInt("Enchant");
		const int UserID = pRes->getInt("UserID");
		GS()->SendInbox("Auctionist", UserID, "Auction expired", "Your slot has expired", ItemID, Value, Enchant);
		Sqlpool.Execute<DB::REMOVE>("tw_store_items", "WHERE ID = '%d'", ID);
	}
	if(ReleaseSlots)
		GS()->Chat(-1, "Auction {INT} slots has been released!", ReleaseSlots);
}

bool CShopCore::BuyShopItem(CPlayer* pPlayer, int ID)
{
	const int ClientID = pPlayer->GetCID();
	ResultPtr pRes = Sqlpool.Execute<DB::SELECT>("*", "tw_store_items", "WHERE ID = '%d'", ID);
	if(!pRes->next())
		return false;

	const int ItemID = pRes->getInt("ItemID");
	CPlayerItem* pPlayerItem = pPlayer->GetItem(ItemID);
	if(pPlayerItem->HasItem() && pPlayerItem->Info()->IsEnchantable())
	{
		GS()->Chat(ClientID, "Enchant item maximal count x1 in a backpack!");
		return false;
	}

	// - - - - - - - - - - AUCTION - - - - - - - - - - - - -
	const int Price = pRes->getInt("Price");
	const int UserID = pRes->getInt("UserID");
	const int Value = pRes->getInt("ItemValue");
	const int Enchant = pRes->getInt("Enchant");
	if(UserID > 0)
	{
		// take out your slot
		if(UserID == pPlayer->Acc().m_UserID)
		{
			GS()->Chat(ClientID, "You closed auction slot!");
			GS()->SendInbox("Auctionist", pPlayer, "Auction Alert", "You have bought a item, or canceled your slot", ItemID, Value, Enchant);
			Sqlpool.Execute<DB::REMOVE>("tw_store_items", "WHERE ItemID = '%d' AND UserID = '%d'", ItemID, UserID);
			return true;
		}

		const int RequiredItemID = pRes->getInt("RequiredItemID");
		if(!pPlayer->SpendCurrency(Price, RequiredItemID))
			return false;

		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "Your [Slot %sx%d] was sold!", pPlayerItem->Info()->GetName(), Value);
		GS()->SendInbox("Auctionist", UserID, "Auction Sell", aBuf, itGold, Price, 0);
		Sqlpool.Execute<DB::REMOVE>("tw_store_items", "WHERE ItemID = '%d' AND UserID = '%d'", ItemID, UserID);

		pPlayerItem->Add(Value, 0, Enchant);
		GS()->Chat(ClientID, "You buy {STR}x{VAL}.", pPlayerItem->Info()->GetName(), Value);
		return true;
	}

	// - - - - - - - - - - - -SHOP - - - - - - - - - - - - -
	const int RequiredItemID = pRes->getInt("RequiredItemID");
	if(!pPlayer->SpendCurrency(Price, RequiredItemID))
		return false;

	pPlayerItem->Add(Value, 0, Enchant);
	GS()->Chat(ClientID, "You exchange {STR}x{VAL} to {STR}x{VAL}.", pPlayerItem->Info()->GetName(), Value, GS()->GetItemInfo(RequiredItemID)->GetName(), Price);
	return true;
}

void CShopCore::ShowAuction(CPlayer* pPlayer)
{
	const int ClientID = pPlayer->GetCID();
	GS()->AVH(ClientID, TAB_INFO_AUCTION, "Auction Information");
	GS()->AVM(ClientID, "null", NOPE, TAB_INFO_AUCTION, "To create a slot, see inventory item interact.");
	GS()->AV(ClientID, "null");
	GS()->ShowVotesItemValueInformation(pPlayer);
	GS()->AV(ClientID, "null");

	bool FoundItems = false;
	int HideID = (int)(NUM_TAB_MENU + CItemDescription::Data().size() + 400);
	ResultPtr pRes = Sqlpool.Execute<DB::SELECT>("*", "tw_store_items", "WHERE UserID > 0 ORDER BY Price");
	while(pRes->next())
	{
		const int ID = pRes->getInt("ID");
		const int ItemID = pRes->getInt("ItemID");
		const int Price = pRes->getInt("Price");
		const int Enchant = pRes->getInt("Enchant");
		const int ItemValue = pRes->getInt("ItemValue");
		const int UserID = pRes->getInt("UserID");
		CItemDescription* pItemInfo = GS()->GetItemInfo(ItemID);

		if(pItemInfo->IsEnchantable())
		{
			char aEnchantBuf[16];
			pItemInfo->StrFormatEnchantLevel(aEnchantBuf, sizeof(aEnchantBuf), Enchant);
			GS()->AVH(ClientID, HideID, "{STR}{STR} {STR} - {VAL} gold",
				(pPlayer->GetItem(ItemID)->GetValue() > 0 ? "✔ " : "\0"), pItemInfo->GetName(), (Enchant > 0 ? aEnchantBuf : "\0"), Price);

			char aAttributes[128];
			pItemInfo->StrFormatAttributes(pPlayer, aAttributes, sizeof(aAttributes), Enchant);
			GS()->AVM(ClientID, "null", NOPE, HideID, "{STR}", aAttributes);
		}
		else
		{
			GS()->AVH(ClientID, HideID, "{STR}x{VAL} ({VAL}) - {VAL} gold",
				pItemInfo->GetName(), ItemValue, pPlayer->GetItem(ItemID)->GetValue(), Price);
		}

		GS()->AVM(ClientID, "null", NOPE, HideID, "{STR}", pItemInfo->GetDesc());
		GS()->AVM(ClientID, "null", NOPE, HideID, "Seller {STR}", Job()->PlayerName(UserID));
		GS()->AVM(ClientID, "SHOP", ID, HideID, "Buy Price {VAL} gold", Price);
		FoundItems = true;
		++HideID;
	}
	if(!FoundItems)
		GS()->AVL(ClientID, "null", "Currently there are no products.");

	GS()->AV(ClientID, "null");
}

void CShopCore::ShowMailShop(CPlayer *pPlayer, int StorageID)
{
	const int ClientID = pPlayer->GetCID();
	int HideID = NUM_TAB_MENU + CItemDescription::Data().size() + 300;
	ResultPtr pRes = Sqlpool.Execute<DB::SELECT>("*", "tw_store_items", "WHERE StorageID = '%d' ORDER BY Price", StorageID);
	while(pRes->next())
	{
		const int ID = pRes->getInt("ID");
		const int ItemID = pRes->getInt("ItemID");
		const int ItemValue = pRes->getInt("ItemValue");
		const int Price = pRes->getInt("Price");
		const int Enchant = pRes->getInt("Enchant");
		const int RequiredItemID = pRes->getInt("RequiredItemID");
		CItemDescription* pBuyightItemInfo = GS()->GetItemInfo(ItemID);
		CItemDescription* pRequiredItemInfo = GS()->GetItemInfo(RequiredItemID);

		if (pBuyightItemInfo->IsEnchantable())
		{
			char aEnchantBuf[16];
			pBuyightItemInfo->StrFormatEnchantLevel(aEnchantBuf, sizeof(aEnchantBuf), Enchant);
			GS()->AVH(ClientID, HideID, "{STR}{STR} {STR} - {VAL} {STR}",
				(pPlayer->GetItem(ItemID)->GetValue() > 0 ? "✔ " : "\0"), pBuyightItemInfo->GetName(), (Enchant > 0 ? aEnchantBuf : "\0"), Price, pRequiredItemInfo->GetName());

			char aAttributes[128];
			pBuyightItemInfo->StrFormatAttributes(pPlayer, aAttributes, sizeof(aAttributes), Enchant);
			GS()->AVM(ClientID, "null", NOPE, HideID, "{STR}", aAttributes);
		}
		else
		{
			GS()->AVH(ClientID, HideID, "{STR}x{VAL} ({VAL}) - {VAL} {STR}",
				pBuyightItemInfo->GetName(), ItemValue, pPlayer->GetItem(ItemID)->GetValue(), Price, pRequiredItemInfo->GetName());
		}

		GS()->AVM(ClientID, "null", NOPE, HideID, "{STR}", pBuyightItemInfo->GetDesc());
		GS()->AVM(ClientID, "SHOP", ID, HideID, "Exchange {STR}x{VAL} to {STR}x{VAL}", pRequiredItemInfo->GetName(), Price, pBuyightItemInfo->GetName(), ItemValue);
		HideID++;
	}
	GS()->AV(ClientID, "null");
}