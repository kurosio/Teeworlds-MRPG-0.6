/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "auction_manager.h"

#include <game/server/gamecontext.h>

#include <game/server/core/components/Inventory/InventoryManager.h>

#include <game/server/core/components/mails/mail_wrapper.h>

void CAuctionManager::OnPreInit()
{
	// init auction slots
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_AUCTION_SLOTS_TABLE);
	while(pRes->next())
	{
		int ID = pRes->getInt("ID");
		int ItemID = pRes->getInt("ItemID");
		int Value = pRes->getInt("Value");
		int Enchant = pRes->getInt("Enchant");
		int Price = pRes->getInt("Price");
		int OwnerID = pRes->getInt("OwnerID");

		// init by server
		CAuctionSlot::CreateElement(ID)->Init({ItemID, Value, Enchant}, Price, OwnerID);
	}
}

void CAuctionManager::OnCharacterTile(CCharacter* pChr)
{
	CPlayer* pPlayer = pChr->GetPlayer();

	HANDLE_TILE_VOTE_MENU(pPlayer, pChr, TILE_AUCTION, MENU_AUCTION_LIST, {}, {});
}

bool CAuctionManager::OnSendMenuVotes(CPlayer* pPlayer, int Menulist)
{
	const int ClientID = pPlayer->GetCID();

	if(Menulist == MENU_AUCTION_LIST)
	{
		ShowAuction(pPlayer);
		return true;
	}

	if(Menulist == MENU_AUCTION_SLOT_SELECT)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_AUCTION_LIST);

		if(const auto SlotID = pPlayer->m_VotesData.GetExtraID())
		{
			ShowAuctionSlot(pPlayer, SlotID.value());
		}

		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	if(Menulist == MENU_AUCTION_CREATE_SLOT)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_INVENTORY);
		ShowCreateSlot(pPlayer);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	return false;
}

void CAuctionManager::ShowAuction(CPlayer* pPlayer) const
{
	// initialize variables
	const int ClientID = pPlayer->GetCID();
	const int UsedsSlots = GetSlotsCountByAccountID(pPlayer->Account()->GetID());

	// information
	VoteWrapper VInfo(ClientID, VWF_SEPARATE | VWF_STYLE_STRICT_BOLD | VWF_ALIGN_TITLE, "Auction Information");
	VInfo.Add("To create a slot, see inventory item interact.");
	VoteWrapper::AddEmptyline(ClientID);

	// show your auction list
	VoteWrapper VSelf(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_SIMPLE, "Your auction slots {} of {}", UsedsSlots, g_Config.m_SvMaxPlayerAuctionSlots);
	for(auto& pSlot : CAuctionSlot::Data())
	{
		// self slots
		if(pSlot->GetOwnerID() == pPlayer->Account()->GetID())
		{
			const CItem* pItem = pSlot->GetItem();
			VSelf.AddOption("AUCTION_BUY", pSlot->GetID(), "{}. Cancel slot ({} x{})", VSelf.NextPos(), pItem->Info()->GetName(), pItem->GetValue());
		}
	}
	VoteWrapper::AddEmptyline(ClientID);

	// show other auction list
	VoteWrapper VList(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_SIMPLE, "Other slots");
	for(auto& pSlot : CAuctionSlot::Data())
	{
		// show other slots
		if(pSlot->GetOwnerID() != pPlayer->Account()->GetID())
		{
			const CItem* pItem = pSlot->GetItem();
			VList.AddMenu(MENU_AUCTION_SLOT_SELECT, pSlot->GetID(), "{}. {} x{} Seller: {}",
				VList.NextPos(), pItem->Info()->GetName(), pItem->GetValue(), Server()->GetAccountNickname(pSlot->GetOwnerID()));
		}
	}
}

void CAuctionManager::ShowCreateSlot(CPlayer* pPlayer) const
{
	// initialize variables
	const int ClientID = pPlayer->GetCID();
	CAuctionSlot* pAuctionData = &pPlayer->GetTempData().m_TempAuctionSlot;
	const CItem* pItem = pAuctionData->GetItem();
	const int Value = pItem->GetValue();
	const int Enchant = pItem->GetEnchant();
	const ItemIdentifier ItemID = pItem->GetID();
	const int SlotPrice = pAuctionData->GetPrice();

	// information
	VoteWrapper(ClientID).Add("The reason for write the number for each row");
	VoteWrapper::AddEmptyline(ClientID);

	VoteWrapper VInfo(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_SIMPLE, "Auction slot for {}", pItem->Info()->GetName());
	VInfo.MarkList().Add("Description:");
	{
		VInfo.BeginDepth();
		VInfo.Add("Tax for creating a slot: {$}gold", pAuctionData->GetTaxPrice());
		if(Enchant > 0)
		{
			VInfo.Add("Warning selling enchanted: +{}", Enchant);
		}
		VInfo.EndDepth();
	}
	VoteWrapper::AddEmptyline(ClientID);

	// interaction
	VoteWrapper VInteract(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_SIMPLE, "Interaction:");
	VInteract.AddOption("AUCTION_NUMBER", ItemID, "Select the number of items: {}.", Value);
	VInteract.AddOption("AUCTION_PRICE", ItemID, "Set the price: {$}.", SlotPrice);
	VInteract.AddOption("AUCTION_ACCEPT", ItemID, "Accept the offer.");
}

void CAuctionManager::ShowAuctionSlot(CPlayer* pPlayer, int ID) const
{
	// initialize variables
	const int ClientID = pPlayer->GetCID();
	const CAuctionSlot* pSlot = GetSlot(ID);
	if(!pSlot)
		return;

	// show slot information
	VoteWrapper VInfo(ClientID, VWF_ALIGN_TITLE|VWF_STYLE_STRICT_BOLD|VWF_SEPARATE, "Auction slot");
	VInfo.Add("Item: {} x{}", pSlot->GetItem()->Info()->GetName(), pSlot->GetItem()->GetValue());
	VInfo.Add("Price: {$} gold", pSlot->GetPrice());
	VInfo.Add("Seller: {}", Server()->GetAccountNickname(pSlot->GetOwnerID()));
	VoteWrapper::AddEmptyline(ClientID);

	// interaction
	VoteWrapper VInteract(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_SIMPLE, "Interaction");
	VInteract.AddOption("AUCTION_BUY", ID, "Buy this slot.");
}

bool CAuctionManager::OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, const int Extra1, const int Extra2, int ReasonNumber, const char* pReason)
{
	const int ClientID = pPlayer->GetCID();

	if(PPSTR(pCmd, "AUCTION_BUY") == 0)
	{
		// try to buy slot
		const int SlotID = Extra1;
		if(BuySlot(pPlayer, SlotID))
			pPlayer->m_VotesData.UpdateVotes(MENU_AUCTION_LIST);
		return true;
	}

	if(PPSTR(pCmd, "AUCTION_NUMBER") == 0)
	{
		// initialize variables
		const int ItemID = Extra1;
		const CPlayerItem* pPlayerItem = pPlayer->GetItem(ItemID);
		CAuctionSlot* pAuctionData = &pPlayer->GetTempData().m_TempAuctionSlot;
		ReasonNumber = minimum(ReasonNumber, pPlayerItem->GetValue());
		const int MinimalPrice = (ReasonNumber * pPlayerItem->Info()->GetInitialPrice());

		// update value
		pAuctionData->SetPrice(maximum(MinimalPrice, pAuctionData->GetPrice()));
		pAuctionData->GetItem()->SetValue(ReasonNumber);
		pPlayer->m_VotesData.UpdateVotesIf(MENU_AUCTION_CREATE_SLOT);
		return true;
	}

	if(PPSTR(pCmd, "AUCTION_PRICE") == 0)
	{
		// initialize variables
		CAuctionSlot* pAuctionData = &pPlayer->GetTempData().m_TempAuctionSlot;
		const int MinimalPrice = (pAuctionData->GetItem()->GetValue() * pAuctionData->GetItem()->Info()->GetInitialPrice());

		// update slot price
		ReasonNumber = maximum(ReasonNumber, MinimalPrice);
		pAuctionData->SetPrice(ReasonNumber);
		pPlayer->m_VotesData.UpdateVotesIf(MENU_AUCTION_CREATE_SLOT);
		return true;
	}

	if(PPSTR(pCmd, "AUCTION_CREATE") == 0)
	{
		// initialize variables
		int ItemID = Extra1;
		int AvailableValue = Core()->InventoryManager()->GetUnfrozenItemValue(pPlayer, Extra1);

		// check available value
		if(AvailableValue <= 0)
		{
			GS()->Chat(ClientID, "This item is frozen!");
			return true;
		}

		// start creating new slot
		CAuctionSlot* pAuctionData = &pPlayer->GetTempData().m_TempAuctionSlot;
		pAuctionData->SetItem({ ItemID, 1, pPlayer->GetItem(ItemID)->GetEnchant(), 0, 0});
		pAuctionData->SetPrice(maximum(1, pAuctionData->GetItem()->Info()->GetInitialPrice()));
		pPlayer->m_VotesData.UpdateVotes(MENU_AUCTION_CREATE_SLOT);
		return true;
	}

	if(PPSTR(pCmd, "AUCTION_ACCEPT") == 0)
	{
		// initialize variables
		const int ItemID = Extra1;
		const CPlayerItem* pPlayerItem = pPlayer->GetItem(ItemID);
		CAuctionSlot* pTempSlot = &pPlayer->GetTempData().m_TempAuctionSlot;

		// try to create new slot
		if(pPlayerItem->GetValue() >= pTempSlot->GetItem()->GetValue())
		{
			CreateSlot(pPlayer, pTempSlot);
			pPlayer->m_VotesData.UpdateVotes(MENU_INVENTORY);
			return true;
		}

		pPlayer->m_VotesData.UpdateVotesIf(MENU_AUCTION_CREATE_SLOT);
		return true;
	}

	return false;
}

void CAuctionManager::CreateSlot(CPlayer* pPlayer, CAuctionSlot* pAuctionData) const
{
	// check valid player
	if(!pPlayer)
		return;

	// initialize variables
	const int ClientID = pPlayer->GetCID();
	int AccountID = pPlayer->Account()->GetID();

	// check player slots
	if(GetSlotsCountByAccountID(AccountID) >= g_Config.m_SvMaxPlayerAuctionSlots)
	{
		GS()->Chat(ClientID, "You have reached the maximum number of slots in the auction!");
		return;
	}

	// check total slots
	if(GetTotalSlotsCount() > g_Config.m_SvMaxAuctionSlots)
	{
		GS()->Chat(ClientID, "The auction has reached the maximum number of slots!");
		return;
	}

	// check tax price
	if(pPlayer->Account()->GetTotalGold() < pAuctionData->GetTaxPrice())
	{
		GS()->Chat(ClientID, "You do not have enough gold to create a slot!");
		return;
	}

	// spend tax price
	if(pPlayer->Account()->SpendCurrency(pAuctionData->GetTaxPrice()))
	{
		// initialize variables
		const CItem* pItem = pAuctionData->GetItem();
		CPlayerItem* pPlayerItem = pPlayer->GetItem(pItem->GetID());

		// try to spend selling auction item
		if(pPlayer->Account()->SpendCurrency(pItem->GetValue(), pItem->GetID()))
		{
			// get the highest group ID from the database
			ResultPtr pResID = Database->Execute<DB::SELECT>("ID", TW_AUCTION_SLOTS_TABLE, "ORDER BY ID DESC LIMIT 1");
			const int InitID = pResID->next() ? pResID->getInt("ID") + 1 : 1; // Increment the highest group ID by 1, or set to 1 if no previous group exists

			// insert new slot
			Database->Execute<DB::INSERT>(TW_AUCTION_SLOTS_TABLE, "(ID, ItemID, Value, Price, Enchant, OwnerID) VALUES ('{}', '{}', '{}', '{}', '{}', '{}')",
				InitID, pItem->GetID(), pItem->GetValue(), pAuctionData->GetPrice(), pItem->GetEnchant(), pPlayer->Account()->GetID());
			CAuctionSlot::CreateElement(InitID)->Init(*pItem, pAuctionData->GetPrice(), AccountID);

			// send messages
			const int AvailableSlots = g_Config.m_SvMaxPlayerAuctionSlots - GetSlotsCountByAccountID(AccountID);
			GS()->Chat(-1, "'{}' created a 'slot [{} x{}]' auction.", Server()->ClientName(ClientID), pPlayerItem->Info()->GetName(), pItem->GetValue());
			GS()->Chat(ClientID, "Still available '{} slots'!", AvailableSlots);
		}
	}
}

bool CAuctionManager::BuySlot(CPlayer* pPlayer, int ID) const
{
	// check valid slot
	auto pSlot = GetSlot(ID);
	if(!pSlot)
		return false;

	// initialize variables
	const int ClientID = pPlayer->GetCID();
	const int AccountID = pPlayer->Account()->GetID();
	const CItem* pItem = pSlot->GetItem();

	// check for self slot
	if(pSlot->GetOwnerID() == AccountID)
	{
		// send mail for owner
		MailWrapper Mail("Auctionist", AccountID, "Auction alert.");
		Mail.AddDescLine("You canceled your slot");
		Mail.AttachItem(*pItem);
		Mail.Send();

		// update and messages
		GS()->Chat(ClientID, "You canceled your slot!");
		RemoveSlotByID(ID);
		return true;
	}

	// player purchasing
	if(pPlayer->Account()->SpendCurrency(pSlot->GetPrice()))
	{
		// send mail for seller
		MailWrapper MailSeller("Auctionist", pSlot->GetOwnerID(), "Auction alert.");
		MailSeller.AddDescLine("Buyer: {}", Server()->ClientName(ClientID));
		MailSeller.AddDescLine("Item: {} x{}", pItem->Info()->GetName(), pItem->GetValue());
		MailSeller.AttachItem({ itGold, pSlot->GetPrice() });
		MailSeller.Send();

		// send mail for buyer
		MailWrapper MailBuyer("Auctionist", AccountID, "Auction alert.");
		MailBuyer.AddDescLine("Seller: {}", Server()->GetAccountNickname(pSlot->GetOwnerID()));
		MailBuyer.AddDescLine("Item: {} x{}", pItem->Info()->GetName(), pItem->GetValue());
		MailBuyer.AttachItem(*pItem);
		MailBuyer.Send();

		// update and messages
		GS()->ChatAccount(AccountID, "You bought {} x{}.", pItem->Info()->GetName(), pItem->GetValue());
		GS()->ChatAccount(pSlot->GetOwnerID(), "Your slot was sold!");
		RemoveSlotByID(ID);
		return true;
	}

	return false;
}

int CAuctionManager::GetSlotsCountByAccountID(int AccountID) const
{
	return (int)std::ranges::count_if(CAuctionSlot::Data(), [AccountID](const CAuctionSlot* pSlot)
	{
		return pSlot->GetOwnerID() == AccountID;
	});
}

int CAuctionManager::GetTotalSlotsCount() const
{
	return static_cast<int>(CAuctionSlot::Data().size());
}

CAuctionSlot* CAuctionManager::GetSlot(int ID) const
{
	auto it = std::ranges::find_if(CAuctionSlot::Data(), [ID](const CAuctionSlot* pSlot)
	{
		return pSlot->GetID() == ID;
	});
	return it != CAuctionSlot::Data().end() ? *it : nullptr;
}

void CAuctionManager::RemoveSlotByID(int ID) const
{
	if(const auto pSlot = GetSlot(ID))
	{
		Database->Execute<DB::REMOVE>(TW_AUCTION_SLOTS_TABLE, "WHERE ID = '{}'", ID);
		std::erase(CAuctionSlot::Data(), pSlot);
	}
}
