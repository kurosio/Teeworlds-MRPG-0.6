/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "HouseManager.h"

#include <game/server/gamecontext.h>

#include <game/server/core/components/Inventory/InventoryManager.h>

void CHouseManager::OnInitWorld(const char* pWhereLocalWorld)
{
	// load house
	const auto InitHouses = Database->Prepare<DB::SELECT>("*", TW_HOUSES_TABLE, pWhereLocalWorld);
	InitHouses->AtExecute([this](ResultPtr pRes)
	{
		while(pRes->next())
		{
			HouseIdentifier ID = pRes->getInt("ID");
			int AccountID = pRes->getInt("UserID");
			std::string ClassName = pRes->getString("Class").c_str();
			int Price = pRes->getInt("Price");
			int Bank = pRes->getInt("HouseBank");
			vec2 Pos(pRes->getInt("PosX"), pRes->getInt("PosY"));
			vec2 TextPos(pRes->getInt("TextX"), pRes->getInt("TextY"));
			vec2 PlantPos(pRes->getInt("PlantX"), pRes->getInt("PlantY"));
			int PlantItemID = pRes->getInt("PlantID");
			int WorldID = pRes->getInt("WorldID");
			std::string AccessData = pRes->getString("AccessData").c_str();
			std::string JsonDoorData = pRes->getString("JsonDoorsData").c_str();

			CHouseData::CreateElement(ID)->Init(AccountID, ClassName, Price, Bank, Pos, TextPos, PlantPos, CItem(PlantItemID, 1), WorldID, std::move(AccessData), std::move(JsonDoorData));
		}

		Core()->ShowLoadingProgress("Houses", CHouseData::Data().size());
	});
}

void CHouseManager::OnTick()
{
	// Check if the current world ID is not equal to the main world (once use House get instance object self world id) ID and current tick
	if(GS()->GetWorldID() != MAIN_WORLD_ID || (Server()->Tick() % Server()->TickSpeed() != 0))
		return;

	// Calculate the remaining lifetime of a text update
	int LifeTime = (Server()->TickSpeed() * 10);

	// Get the house data
	const auto& HouseData = CHouseData::Data();
	for(const auto& p : HouseData)
	{
		// Update the text with the remaining lifetime
		p->TextUpdate(LifeTime);
	}
}

bool CHouseManager::OnHandleTile(CCharacter* pChr, int IndexCollision)
{
	CPlayer* pPlayer = pChr->GetPlayer();
	const int ClientID = pPlayer->GetCID();

	if(pChr->GetHelper()->TileEnter(IndexCollision, TILE_PLAYER_HOUSE))
	{
		_DEF_TILE_ENTER_ZONE_IMPL(pPlayer, MENU_HOUSE_BUY);
		return true;
	}
	if(pChr->GetHelper()->TileExit(IndexCollision, TILE_PLAYER_HOUSE))
	{
		_DEF_TILE_EXIT_ZONE_IMPL(pPlayer);
		return true;
	}

	return false;
}

bool CHouseManager::OnHandleMenulist(CPlayer* pPlayer, int Menulist)
{
	const int ClientID = pPlayer->GetCID();

	if(Menulist == MENU_HOUSE)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);

		CHouseData* pHouse = pPlayer->Account()->GetHouse();
		if(!pHouse)
		{
			CVoteWrapper::AddBackpage(ClientID);
			return true;
		}

		// information
		HouseIdentifier ID = pHouse->GetID();
		CVoteWrapper VInfo(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_SIMPLE, "House stats {} Class {}", ID, pHouse->GetClassName());
		VInfo.Add("/hdoor - interactive with door.");
		VInfo.Add("/hsell - sell house.");
		VInfo.AddLine();

		// House deposit
		CVoteWrapper VDeposit(ClientID, VWF_SEPARATE_OPEN, "\u2727 Your: {} | Safe: {} golds", pPlayer->GetItem(itGold)->GetValue(), pHouse->GetBank()->Get());
		VDeposit.AddOption("HOUSE_BANK_ADD", "Add. (Amount in a reason)");
		VDeposit.AddOption("HOUSE_BANK_TAKE", "Take. (Amount in a reason)");
		VDeposit.AddLine();

		// House doors
		if(!pHouse->GetDoorsController()->GetDoors().empty())
		{
			CVoteWrapper VDoors(ClientID, VWF_SEPARATE_OPEN, "\u2743 House has {} controlled door's", (int)pHouse->GetDoorsController()->GetDoors().size());
			for(auto& [Number, DoorData] : pHouse->GetDoorsController()->GetDoors())
			{
				bool StateDoor = DoorData->IsClosed();
				VDoors.AddOption("HOUSE_DOOR", Number, "Door {} {}", StateDoor ? "Open" : "Close", DoorData->GetName());
			}
			VDoors.AddLine();
		}

		// House invited list
		CVoteWrapper VManagement(ClientID, VWF_SEPARATE_OPEN, "\u2697 Managing your home");
		VManagement.AddOption("HOUSE_SPAWN", "Teleport to your house");
		VManagement.AddOption("HOUSE_SELL", "Sell your house (in reason 777)");
		VManagement.AddOption("HOUSE_DECORATION", "Decoration editor");

		if(GS()->IsPlayerEqualWorld(ClientID, pHouse->GetWorldID()))
		{
			VManagement.AddMenu(MENU_HOUSE_ACCESS_TO_DOOR, "Settings up accesses to your door's");
			VManagement.AddMenu(MENU_HOUSE_PLANTS, "Settings your plants");
		}
		else
		{
			VManagement.Add("null", "More settings allow, only on house zone!");
		}

		CVoteWrapper::AddBackpage(ClientID);
		return true;
	}

	if(Menulist == MENU_HOUSE_PLANTS)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_HOUSE);

		CHouseData* pHouse = pPlayer->Account()->GetHouse();
		if(!pHouse)
		{
			CVoteWrapper(ClientID).Add("You not owner home!");
			return true;
		}

		// information
		CVoteWrapper VPlantInfo(ClientID, VWF_SEPARATE_CLOSED, "\u2741 Plant zones information");
		VPlantInfo.Add("You can plant some kind of plantation.");
		CVoteWrapper::AddLine(ClientID);

		// settings
		CItemDescription* pItem = GS()->GetItemInfo(pHouse->GetPlantedItem()->GetID());
		CVoteWrapper VPlantSettings(ClientID, VWF_STYLE_STRICT_BOLD);
		VPlantSettings.Add("\u2741 Plant zone: default");
		VPlantSettings.Add("Planted: {}", pItem->GetName());
		VPlantSettings.AddLine();

		// items
		CVoteWrapper VPlantItems(ClientID, VWF_SEPARATE_OPEN, "\u2741 Possible items for planting");
		std::vector<ItemIdentifier> vItems = Core()->InventoryManager()->GetItemIDsCollectionByFunction(ItemFunctional::FUNCTION_PLANT);
		for(auto& ID : vItems)
		{
			CPlayerItem* pPlayerItem = pPlayer->GetItem(ID);
			if(pPlayerItem->HasItem())
				VPlantItems.AddOption("HOUSE_PLANT_ZONE_TRY", ID, "Try plant {} (has {})", pPlayerItem->Info()->GetName(), pPlayerItem->GetValue());
		}
		if(VPlantItems.IsEmpty())
			VPlantItems.Add("You have no plants for planting");

		// Add the votes to the player's back page
		CVoteWrapper::AddBackpage(ClientID);
		return true;
	}

	if(Menulist == MENU_HOUSE_ACCESS_TO_DOOR)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_HOUSE);

		CHouseData* pHouse = pPlayer->Account()->GetHouse();
		if(!pHouse)
		{
			CVoteWrapper(ClientID).Add("You not owner home!");
			return true;
		}

		// information
		CVoteWrapper VInfo(ClientID, VWF_SEPARATE_CLOSED, "\u2697 Access to door's");
		VInfo.Add("You can manage access to your door's.");
		VInfo.Add("Add a limited number of players who can");
		VInfo.Add("enter the house regardless of door status");
		VInfo.AddLine();

		// show active access players to house
		CHouseDoorsController* pHouseDoor = pHouse->GetDoorsController();
		CVoteWrapper VAccess(ClientID, VWF_SEPARATE_OPEN|VWF_STYLE_SIMPLE);
		VAccess.MarkList().Add("Permits have been granted:");
		{
			VAccess.BeginDepth();
			VAccess.Add("You and your eidolon have full access");
			for(auto& p : pHouseDoor->GetAccesses())
				VAccess.AddOption("HOUSE_INVITED_LIST_REMOVE", p, "Remove access from {}", Server()->GetAccountNickname(p));
			VAccess.EndDepth();
		}

		// search result
		CVoteWrapper VSearch(ClientID, VWF_SEPARATE_OPEN|VWF_STYLE_SIMPLE, "Search result by [{}]", pPlayer->GetTempData().m_aPlayerSearchBuf);
		VSearch.MarkList().Add("You can add {} player's:", pHouseDoor->GetAvailableAccessSlots());
		{
			VSearch.BeginDepth();
			VSearch.Add("Use reason. The entered value can be a partial.");
			VSearch.AddOption("HOUSE_INVITED_LIST_FIND", "Find player: [{}]", pPlayer->GetTempData().m_aPlayerSearchBuf);
			VSearch.EndDepth();
		}
		VSearch.AddLine();
		VSearch.MarkList().Add("Search result: [{}]", pPlayer->GetTempData().m_aPlayerSearchBuf);
		{
			VSearch.BeginDepth();
			if(pPlayer->GetTempData().m_aPlayerSearchBuf[0] != '\0')
			{
				bool Found = false;
				CSqlString<64> cPlayerName = CSqlString<64>(pPlayer->GetTempData().m_aPlayerSearchBuf);
				ResultPtr pRes = Database->Execute<DB::SELECT>("ID, Nick", "tw_accounts_data", "WHERE Nick LIKE '%%%s%%' LIMIT 20", cPlayerName.cstr());
				while(pRes->next())
				{
					const int UserID = pRes->getInt("ID");
					if(pHouseDoor->HasAccess(UserID) || (UserID == pPlayer->Account()->GetID()))
						continue;

					cPlayerName = pRes->getString("Nick").c_str();
					VSearch.AddOption("HOUSE_INVITED_LIST_ADD", UserID, "Give access for {}", cPlayerName.cstr());
					Found = true;
				}

				if(!Found)
					VSearch.Add("null", "Players for the request {} not found!", pPlayer->GetTempData().m_aPlayerSearchBuf);
			}
			else
				VSearch.Add("Set the reason for the search field");
			VSearch.EndDepth();
		}

		// back page
		CVoteWrapper::AddBackpage(ClientID);
		return true;
	}

	if(Menulist == MENU_HOUSE_BUY)
	{
		CCharacter* pChr = pPlayer->GetCharacter();
		if(CHouseData* pHouse = GetHouseByPos(pChr->m_Core.m_Pos))
			ShowBuyHouse(pPlayer, pHouse);

		return true;
	}

	return false;
}

bool CHouseManager::OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, const int VoteID, const int VoteID2, int Get, const char* GetText)
{
	const int ClientID = pPlayer->GetCID();
	if(PPSTR(CMD, "HOUSE_BUY") == 0)
	{
		const int HouseID = VoteID;
		if(CHouseData* pHouse = GetHouse(HouseID))
		{
			pHouse->Buy(pPlayer);
		}

		return true;
	}

	if(PPSTR(CMD, "HOUSE_DECORATION") == 0)
	{
		// check player house
		CHouseData* pHouse = pPlayer->Account()->GetHouse();
		if(!pHouse)
		{
			GS()->Chat(ClientID, "You do not have your own home!");
			return true;
		}

		bool Result = pHouse->StartDrawing();
		if(Result)
		{
			GS()->Chat(ClientID, "You can now draw decorations.");
		}
		else
		{
			GS()->Chat(ClientID, "You can't draw decorations.");
		}
		return true;
	}

	if(PPSTR(CMD, "HOUSE_PLANT_ZONE_TRY") == 0)
	{
		// check player house
		CHouseData* pHouse = pPlayer->Account()->GetHouse();
		if(!pHouse)
		{
			GS()->Chat(ClientID, "You do not have your own home!");
			return true;
		}

		const int& Useds = maximum(1, Get);
		const int& PlantzoneID = VoteID;
		const ItemIdentifier& ItemID = VoteID2;

		// Check if the ItemID of the plant matches the ItemID of the plant zone
		if(ItemID == pHouse->GetPlantedItem()->GetID())
		{
			GS()->Chat(ClientID, "This plant is already planted.");
			return true;
		}

		// Check if the player has enough currency to spend
		if(pPlayer->Account()->SpendCurrency(Useds, ItemID))
		{
			// Check if the chance result is successful
			bool Success = false;
			Chance result(0.025f);
			for(int i = 0; i < Useds && !Success; i++)
			{
				if(result())
					Success = true;

				// Update the chance result for the next attempt
				result.Update();
			}

			// Check if the planting was successful
			if(!Success)
			{
				// If not successful, inform the client that they failed to plant the plant
				GS()->Chat(ClientID, "You failed to plant the plant.");
			}
			else
			{
				// Change the item in the plant zone to the planted item
				GS()->Chat(ClientID, "You have successfully planted the plant.");
				pHouse->SetPlantItemID(ItemID);
			}

			pPlayer->m_VotesData.UpdateVotesIf(MENU_HOUSE_PLANTS);
		}

		return true;
	}

	if(PPSTR(CMD, "HOUSE_SPAWN") == 0)
	{
		// check alive player
		if(!pPlayer->GetCharacter())
		{
			return true;
		}

		// check player house
		CHouseData* pHouse = pPlayer->Account()->GetHouse();
		if(!pHouse)
		{
			GS()->Chat(ClientID, "You do not have your own home!");
			return true;
		}

		// change world in case the house is in another world
		if(!GS()->IsPlayerEqualWorld(ClientID, pHouse->GetWorldID()))
		{
			pPlayer->GetTempData().SetTeleportPosition(pHouse->GetPos());
			pPlayer->ChangeWorld(pHouse->GetWorldID());
			return true;
		}

		// set new position
		pPlayer->GetCharacter()->ChangePosition(pHouse->GetPos());
		return true;
	}

	if(PPSTR(CMD, "HOUSE_SELL") == 0)
	{
		// check player house
		CHouseData* pHouse = pPlayer->Account()->GetHouse();
		if(!pHouse)
		{
			GS()->Chat(ClientID, "You do not have your own home!");
			return true;
		}

		// check captcha accidental press
		if(Get != 777)
		{
			GS()->Chat(ClientID, "A verification number was entered incorrectly!");
			return true;
		}

		// sell house
		pHouse->Sell();
		pPlayer->m_VotesData.UpdateVotes(MENU_MAIN);
		return true;
	}

	if(PPSTR(CMD, "HOUSE_BANK_ADD") == 0)
	{
		// check player house
		CHouseData* pHouse = pPlayer->Account()->GetHouse();
		if(!pHouse)
		{
			GS()->Chat(ClientID, "You do not have your own home!");
			return true;
		}

		// minial 
		if(Get < 100)
		{
			GS()->Chat(ClientID, "The minimum interaction cannot be below 100 gold!");
			return true;
		}

		// add gold to house bank
		pHouse->GetBank()->Add(Get);
		pPlayer->m_VotesData.UpdateVotesIf(MENU_HOUSE);
		return true;
	}

	if(PPSTR(CMD, "HOUSE_BANK_TAKE") == 0)
	{
		// check player house
		CHouseData* pHouse = pPlayer->Account()->GetHouse();
		if(!pHouse)
		{
			GS()->Chat(ClientID, "You do not have your own home!");
			return true;
		}

		// minial 
		if(Get < 100)
		{
			GS()->Chat(ClientID, "The minimum interaction cannot be below 100 gold!");
			return true;
		}

		// take gold from house bank
		pHouse->GetBank()->Take(Get);
		pPlayer->m_VotesData.UpdateVotesIf(MENU_HOUSE);
		return true;
	}

	if(PPSTR(CMD, "HOUSE_DOOR") == 0)
	{
		// check player house
		CHouseData* pHouse = pPlayer->Account()->GetHouse();
		if(!pHouse)
		{
			GS()->Chat(ClientID, "You do not have your own home!");
			return true;
		}

		// reverse door house
		int UniqueDoorID = VoteID;
		pHouse->GetDoorsController()->Reverse(UniqueDoorID);
		pPlayer->m_VotesData.UpdateVotesIf(MENU_HOUSE);
		return true;
	}

	/*
	if(PPSTR(CMD, "DECORATION_HOUSE_ADD") == 0)
	{
		// check player house
		CHouseData* pHouse = pPlayer->AccountManager()->GetHouse();
		if(!pHouse)
		{
			GS()->Chat(ClientID, "You do not have your own home!");
			return true;
		}

		// check distance between player and house
		if(distance(pHouse->GetPos(), pPlayer->GetCharacter()->m_Core.m_Pos) > 600)
		{
			GS()->Chat(ClientID, "You have a lot of distance between you and the house, try to get closer!");
			return true;
		}

		// start custom vote
		GS()->StartCustomVotes(ClientID, MENU_HOUSE_DECORATION);
		GS()->AV(ClientID, "null", "Please close vote and press Left Mouse,");
		GS()->AV(ClientID, "null", "on position where add decoration!");
		GS()->AddVotesBackpage(ClientID);
		GS()->EndCustomVotes(ClientID);
		// end custom vote

		// set temp data
		pPlayer->GetTempData().m_TempDecoractionID = VoteID;
		pPlayer->GetTempData().m_TempDecorationType = DECORATIONS_HOUSE;
		return true;
	}

	if(PPSTR(CMD, "DECORATION_HOUSE_DELETE") == 0)
	{
		// check player house
		CHouseData* pHouse = pPlayer->AccountManager()->GetHouse();
		if(!pHouse)
		{
			GS()->Chat(ClientID, "You do not have your own home!");
			return true;
		}

		// remove decoration
		if(pHouse->RemoveDecoration(VoteID))
		{
			CPlayerItem* pPlayerItem = pPlayer->GetItem(VoteID2);
			GS()->Chat(ClientID, "You back to the backpack {}!", pPlayerItem->Info()->GetName());
			pPlayerItem->Add(1);
			GS()->StrongUpdateVotes(ClientID, MENU_HOUSE_DECORATION);
		}

		return true;
	}
	*/

	if(PPSTR(CMD, "PLANTING_HOUSE_SET") == 0)
	{
		// check player house
		CHouseData* pHouse = pPlayer->Account()->GetHouse();
		if(!pHouse)
		{
			GS()->Chat(ClientID, "You do not have your own home!");
			return true;
		}

		// check support for plant item's
		if(!pHouse->GetPlantedItem()->IsValid())
		{
			GS()->Chat(ClientID, "Your home does not support plants!");
			return true;
		}

		// try set new plant item
		ItemIdentifier TryItemID = VoteID;
		if(pPlayer->Account()->SpendCurrency(1, TryItemID))
		{
			const int ChanceSuccesful = VoteID2;
			if(ChanceSuccesful != 0)
			{
				GS()->Chat(ClientID, "Unfortunately plant did not take root!");
				pPlayer->m_VotesData.UpdateCurrentVotes();
				return true;
			}

			GS()->Chat(-1, "Congratulations {}, planted at home {}!", Server()->ClientName(ClientID), GS()->GetItemInfo(TryItemID)->GetName());
			pHouse->SetPlantItemID(TryItemID);
			pPlayer->m_VotesData.UpdateCurrentVotes();
		}

		return true;
	}

	// house invited list
	if(PPSTR(CMD, "HOUSE_INVITED_LIST_FIND") == 0)
	{
		if(PPSTR(GetText, "NULL") == 0)
		{
			GS()->Chat(ClientID, "Use please another name.");
			return true;
		}

		str_copy(pPlayer->GetTempData().m_aPlayerSearchBuf, GetText, sizeof(pPlayer->GetTempData().m_aPlayerSearchBuf));
		pPlayer->m_VotesData.UpdateVotesIf(MENU_HOUSE_ACCESS_TO_DOOR);
		return true;
	}

	if(PPSTR(CMD, "HOUSE_INVITED_LIST_ADD") == 0)
	{
		const int UserID = VoteID;
		if(CHouseData* pHouse = pPlayer->Account()->GetHouse())
			pHouse->GetDoorsController()->AddAccess(UserID);

		pPlayer->m_VotesData.UpdateVotesIf(MENU_HOUSE_ACCESS_TO_DOOR);
		return true;
	}

	if(PPSTR(CMD, "HOUSE_INVITED_LIST_REMOVE") == 0)
	{
		const int UserID = VoteID;
		if(CHouseData* pHouse = pPlayer->Account()->GetHouse())
			pHouse->GetDoorsController()->RemoveAccess(UserID);

		pPlayer->m_VotesData.UpdateVotesIf(MENU_HOUSE_ACCESS_TO_DOOR);
		return true;
	}

	return false;
}

/* #########################################################################
	MENUS HOUSES
######################################################################### */
void CHouseManager::ShowBuyHouse(CPlayer* pPlayer, CHouseData* pHouse)
{
	HouseIdentifier ID = pHouse->GetID();
	const int ClientID = pPlayer->GetCID();
	const char* pStrHouseOwner = pHouse->HasOwner() ? Instance::Server()->GetAccountNickname(pHouse->GetAccountID()) : "No owner";

	CVoteWrapper VInfo(ClientID, VWF_SEPARATE_CLOSED, "House information");
	VInfo.Add("You can buy this house for {} gold.", pHouse->GetPrice());
	VInfo.AddLine();

	// detail information
	CVoteWrapper VDetail(ClientID, VWF_SEPARATE_OPEN, "Detail information about house.", ID, pHouse->GetClassName());
	VDetail.Add("House owned by: {}", pStrHouseOwner);
	VDetail.Add("House price: {} gold", pHouse->GetPrice());
	VDetail.Add("House class: {}", pHouse->GetClassName());
	VDetail.AddLine();

	CVoteWrapper VBuy(ClientID, VWF_SEPARATE_OPEN, "You have {} golds.", pPlayer->GetItem(itGold)->GetValue());
	if(pHouse->GetAccountID() <= 0)
		VBuy.AddOption("HOUSE_BUY", ID, "Buy this house. Price {}gold", pHouse->GetPrice());
	else
		VBuy.Add("This house has already been purchased!");
	VBuy.AddLine();
}

CHouseData* CHouseManager::GetHouseByAccountID(int AccountID)
{
	CHouseData* pData = nullptr;

	for(auto& pHouse : CHouseData::Data())
	{
		if(pHouse->GetAccountID() == AccountID)
		{
			pData = pHouse.get();
			break;
		}
	}

	return pData;
}

CHouseData* CHouseManager::GetHouse(HouseIdentifier ID)
{
	auto pHouse = std::find_if(CHouseData::Data().begin(), CHouseData::Data().end(), [ID](auto& p) { return p->GetID() == ID; });
	return pHouse != CHouseData::Data().end() ? (*pHouse).get() : nullptr;
}

CHouseData* CHouseManager::GetHouseByPos(vec2 Pos)
{
	auto pHouse = std::find_if(CHouseData::Data().begin(), CHouseData::Data().end(), [Pos](auto& p) { return distance(Pos, p->GetPos()) < 128.0f; });
	return pHouse != CHouseData::Data().end() ? (*pHouse).get() : nullptr;
}

CHouseData* CHouseManager::GetHouseByPlantPos(vec2 Pos)
{
	auto pHouse = std::find_if(CHouseData::Data().begin(), CHouseData::Data().end(), [Pos](auto& p) { return distance(Pos, p->GetPlantPos()) < 300.0f; });
	return pHouse != CHouseData::Data().end() ? (*pHouse).get() : nullptr;
}