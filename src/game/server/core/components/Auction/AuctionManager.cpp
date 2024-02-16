/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "AuctionManager.h"

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>

#include <game/server/core/components/Inventory/InventoryManager.h>

constexpr auto TW_AUCTION_TABLE = "tw_auction_items";

void CAuctionManager::OnTick()
{
}

bool CAuctionManager::OnHandleTile(CCharacter* pChr, int IndexCollision)
{
	CPlayer* pPlayer = pChr->GetPlayer();
	const int ClientID = pPlayer->GetCID();
	if (pChr->GetHelper()->TileEnter(IndexCollision, TILE_AUCTION))
	{
		_DEF_TILE_ENTER_ZONE_IMPL(pPlayer, MENU_AUCTION_LIST);
		return true;
	}

	if (pChr->GetHelper()->TileExit(IndexCollision, TILE_AUCTION))
	{
		_DEF_TILE_EXIT_ZONE_IMPL(pPlayer);
		return true;
	}

	return false;
}

bool CAuctionManager::OnHandleMenulist(CPlayer* pPlayer, int Menulist)
{
	const int ClientID = pPlayer->GetCID();

	if(Menulist == MENU_AUCTION_LIST)
	{
		ShowAuction(pPlayer);
		return true;
	}

	if(Menulist == MENU_AUCTION_CREATE_SLOT)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_INVENTORY);

		CAuctionSlot* pAuctionData = &pPlayer->GetTempData().m_AuctionData;
		CItem* pAuctionItem = pAuctionData->GetItem();

		const int SlotValue = pAuctionItem->GetValue();
		const int SlotEnchant = pAuctionItem->GetEnchant();
		const ItemIdentifier SlotItemID = pAuctionItem->GetID();
		const int SlotPrice = pAuctionData->GetPrice();

		CVoteWrapper(ClientID).Add("The reason for write the number for each row");
		CVoteWrapper::AddEmptyline(ClientID);

		CVoteWrapper VSlot(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_SIMPLE, "Auction slot for {STR}", pAuctionItem->Info()->GetName());
		VSlot.Add("Description:");
		{
			VSlot.BeginDepthList();
			VSlot.Add("Tax for creating a slot: {VAL}gold", pAuctionData->GetTaxPrice());
			VSlot.AddIf(SlotEnchant > 0, "Warning selling enchanted: +{INT}", SlotEnchant);
			VSlot.EndDepthList();
		}
		VSlot.AddLine();
		VSlot.Add("Interaction:");
		{
			VSlot.BeginDepthList();
			VSlot.AddOption("AUCTION_COUNT", SlotItemID, "Select the number of items: {VAL}.", SlotValue);
			VSlot.AddOption("AUCTION_PRICE", SlotItemID, "Set the price: {VAL}.", SlotPrice);
			VSlot.AddOption("AUCTION_ACCEPT", SlotItemID, "Accept the offer.");
			VSlot.EndDepthList();
		}

		CVoteWrapper::AddBackpage(ClientID);
		return true;
	}
	return false;
}

bool CAuctionManager::OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, const int VoteID, const int VoteID2, int Get, const char* GetText)
{
	const int ClientID = pPlayer->GetCID();

	if(PPSTR(CMD, "AUCTION_BUY") == 0)
	{
		if(BuyItem(pPlayer, VoteID))
			pPlayer->m_VotesData.UpdateVotes(MenuList::MENU_MAIN);
		return true;
	}

	if(PPSTR(CMD, "AUCTION_COUNT") == 0)
	{
		// if there are fewer items installed, we set the number of items.
		CPlayerItem* pPlayerItem = pPlayer->GetItem(VoteID);
		if(Get > pPlayerItem->GetValue())
			Get = pPlayerItem->GetValue();

		// if it is possible to number
		if(pPlayerItem->Info()->IsEnchantable())
			Get = 1;

		CAuctionSlot* pAuctionData = &pPlayer->GetTempData().m_AuctionData;
		const int MinimalPrice = (Get * pPlayerItem->Info()->GetInitialPrice());
		if(pAuctionData->GetPrice() < MinimalPrice)
			pAuctionData->SetPrice(MinimalPrice);

		pAuctionData->GetItem()->SetValue(Get);
		pPlayer->m_VotesData.UpdateVotesIf(MENU_AUCTION_CREATE_SLOT);
		return true;
	}

	if(PPSTR(CMD, "AUCTION_PRICE") == 0)
	{
		CAuctionSlot* pAuctionData = &pPlayer->GetTempData().m_AuctionData;
		const int MinimalPrice = (pAuctionData->GetItem()->GetValue() * pAuctionData->GetItem()->Info()->GetInitialPrice());
		if(Get < MinimalPrice)
			Get = MinimalPrice;

		pAuctionData->SetPrice(Get);
		pPlayer->m_VotesData.UpdateVotesIf(MENU_AUCTION_CREATE_SLOT);
		return true;
	}

	if(PPSTR(CMD, "AUCTION_CREATE") == 0)
	{
		int AvailableValue = Core()->InventoryManager()->GetUnfrozenItemValue(pPlayer, VoteID);
		if(AvailableValue <= 0)
			return true;
		
		CAuctionSlot* pAuctionData = &pPlayer->GetTempData().m_AuctionData;
		pAuctionData->SetItem({ VoteID, 0, pPlayer->GetItem(VoteID)->GetEnchant(), 0, 0});
		pPlayer->m_VotesData.UpdateVotes(MENU_AUCTION_CREATE_SLOT);
		return true;
	}

	if(PPSTR(CMD, "AUCTION_ACCEPT") == 0)
	{
		CPlayerItem* pPlayerItem = pPlayer->GetItem(VoteID);
		CAuctionSlot* pAuctionData = &pPlayer->GetTempData().m_AuctionData;
		if(pPlayerItem->GetValue() >= pAuctionData->GetItem()->GetValue() && pAuctionData->GetPrice() >= 10)
		{
			CreateAuctionSlot(pPlayer, pAuctionData);
			pPlayer->m_VotesData.UpdateVotes(MenuList::MENU_INVENTORY);
			return true;
		}
		pPlayer->m_VotesData.UpdateVotesIf(MENU_AUCTION_CREATE_SLOT);
		return true;
	}

	return false;
}

void CAuctionManager::CreateAuctionSlot(CPlayer* pPlayer, CAuctionSlot* pAuctionData)
{
	const int ClientID = pPlayer->GetCID();

	// check the number of slots whether everything is occupied or not
	ResultPtr pResCheck = Database->Execute<DB::SELECT>("ID", "tw_auction_items", "WHERE UserID > '0' LIMIT %d", g_Config.m_SvMaxAuctionSlots);
	if((int)pResCheck->rowsCount() >= g_Config.m_SvMaxAuctionSlots)
	{
		GS()->Chat(ClientID, "Auction has run out of slots, wait for the release of slots!");
		return;
	}

	// check your slots
	ResultPtr pResCheck2 = Database->Execute<DB::SELECT>("ID", "tw_auction_items", "WHERE UserID = '%d' LIMIT %d", pPlayer->Account()->GetID(), g_Config.m_SvMaxAuctionPlayerSlots);
	const int ValueSlot = (int)pResCheck2->rowsCount();
	if(ValueSlot >= g_Config.m_SvMaxAuctionPlayerSlots)
	{
		GS()->Chat(ClientID, "You use all open the slots in your auction!");
		return;
	}

	// take a tax from the player for the slot
	if(!pPlayer->Account()->SpendCurrency(pAuctionData->GetTaxPrice()))
		return;

	// pick up the item and add a slot
	CItem* pAuctionItem = pAuctionData->GetItem();
	CPlayerItem* pPlayerItem = pPlayer->GetItem(pAuctionItem->GetID());
	if(pPlayerItem->GetValue() >= pAuctionItem->GetValue() && pPlayerItem->Remove(pAuctionItem->GetValue()))
	{
		Database->Execute<DB::INSERT>(TW_AUCTION_TABLE, "(ItemID, Price, ItemValue, UserID, Enchant) VALUES ('%d', '%d', '%d', '%d', '%d')",
			pAuctionItem->GetID(), pAuctionData->GetPrice(), pAuctionItem->GetValue(), pPlayer->Account()->GetID(), pAuctionItem->GetEnchant());

		const int AvailableSlot = (g_Config.m_SvMaxAuctionPlayerSlots - ValueSlot) - 1;
		GS()->Chat(-1, "{STR} created a slot [{STR}x{VAL}] auction.", Server()->ClientName(ClientID), pPlayerItem->Info()->GetName(), pAuctionItem->GetValue());
		GS()->Chat(ClientID, "Still available {INT} slots!", AvailableSlot);
	}
}

bool CAuctionManager::BuyItem(CPlayer* pPlayer, int ID)
{
	const int ClientID = pPlayer->GetCID();
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_AUCTION_TABLE, "WHERE ID = '%d'", ID);
	if(!pRes->next())
		return false;

	// checking for enchanted items
	const ItemIdentifier ItemID = pRes->getInt("ItemID");
	CPlayerItem* pPlayerItem = pPlayer->GetItem(ItemID);

	const int UserID = pRes->getInt("UserID");
	const int Price = pRes->getInt("Price");
	const int Value = pRes->getInt("ItemValue");
	const int Enchant = pRes->getInt("Enchant");

	// if it is a player slot then close the slot
	if(UserID == pPlayer->Account()->GetID())
	{
		GS()->Chat(ClientID, "You closed auction slot!");
		GS()->SendInbox("Auctionist", pPlayer, "Auction Alert", "You have bought a item, or canceled your slot", ItemID, Value, Enchant);
		Database->Execute<DB::REMOVE>(TW_AUCTION_TABLE, "WHERE ItemID = '%d' AND UserID = '%d'", ItemID, UserID);
		return true;
	}

	// checking for enchanted items
	if(pPlayerItem->HasItem() && pPlayerItem->Info()->IsEnchantable())
	{
		GS()->Chat(ClientID, "Enchant item maximal count x1 in a backpack!");
		return false;
	}

	// player purchasing
	if(!pPlayer->Account()->SpendCurrency(Price))
		return false;

	// information & exchange item
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "Your [Slot %sx%d] was sold!", pPlayerItem->Info()->GetName(), Value);
	GS()->SendInbox("Auctionist", UserID, "Auction Sell", aBuf, itGold, Price, 0);
	Database->Execute<DB::REMOVE>(TW_AUCTION_TABLE, "WHERE ItemID = '%d' AND UserID = '%d'", ItemID, UserID);

	pPlayerItem->Add(Value, 0, Enchant);
	GS()->Chat(ClientID, "You buy {STR}x{VAL}.", pPlayerItem->Info()->GetName(), Value);
	return true;
}

void CAuctionManager::ShowAuction(CPlayer* pPlayer)
{
	const int ClientID = pPlayer->GetCID();

	CVoteWrapper VInfo(ClientID, VWF_SEPARATE_CLOSED, "Auction Information");
	VInfo.Add("To create a slot, see inventory item interact.");
	VInfo.AddLine();

	bool Found = false;
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_AUCTION_TABLE, "WHERE UserID > 0 ORDER BY Price");
	while(pRes->next())
	{
		const int ID = pRes->getInt("ID");
		const ItemIdentifier ItemID = pRes->getInt("ItemID");
		const int Price = pRes->getInt("Price");
		const int Enchant = pRes->getInt("Enchant");
		const int ItemValue = pRes->getInt("ItemValue");
		const int UserID = pRes->getInt("UserID");
		CItemDescription* pItemInfo = GS()->GetItemInfo(ItemID);

		CVoteWrapper VItem(ClientID, VWF_UNIQUE | VWF_STYLE_SIMPLE);
		if(pItemInfo->IsEnchantable())
		{
			VItem.SetTitle("{STR}{STR} {STR} - {VAL} gold",
				(pPlayer->GetItem(ItemID)->GetValue() > 0 ? "✔ " : "\0"), pItemInfo->GetName(), pItemInfo->StringEnchantLevel(Enchant).c_str(), Price);

			char aAttributes[128];
			pItemInfo->StrFormatAttributes(pPlayer, aAttributes, sizeof(aAttributes), Enchant);
			VItem.Add(aAttributes);
		}
		else
		{
			VItem.SetTitle("{STR}x{VAL} ({VAL}) - {VAL} gold", pItemInfo->GetName(), ItemValue, pPlayer->GetItem(ItemID)->GetValue(), Price);
		}

		VItem.Add("Seller: {STR}", Server()->GetAccountNickname(UserID));
		VItem.AddOption("AUCTION_BUY", ID, "Buy this item ({VAL} gold).", Price);
		Found = true;
	}

	if(!Found)
	{
		CVoteWrapper(ClientID).Add("Currently there are no products.");
	}
}
