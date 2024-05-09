/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "house_manager.h"

#include "entities/house_door.h"

#include <game/server/core/components/Inventory/InventoryManager.h>
#include <game/server/gamecontext.h>


void CHouseManager::OnInitWorld(const char* pWhereLocalWorld)
{
	// initialize houses
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_HOUSES_TABLE, pWhereLocalWorld);
	while(pRes->next())
	{
		HouseIdentifier ID = pRes->getInt("ID");
		int AccountID = pRes->getInt("UserID");
		std::string ClassName = pRes->getString("Class").c_str();
		int Price = pRes->getInt("Price");
		int Bank = pRes->getInt("Bank");
		int WorldID = pRes->getInt("WorldID");
		std::string AccessList = pRes->getString("AccessList").c_str();
		std::string JsonDoors = pRes->getString("Doors").c_str();
		std::string JsonPlantzones = pRes->getString("Plantzones").c_str();
		std::string JsonProperties = pRes->getString("Properties").c_str();

		CHouseData::CreateElement(ID)->Init(AccountID, ClassName, Price, Bank, WorldID, std::move(AccessList), std::move(JsonDoors), std::move(JsonPlantzones), std::move(JsonProperties));
	}

	Core()->ShowLoadingProgress("Houses", CHouseData::Data().size());
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

void CHouseManager::ShowSell(CPlayer* pPlayer)
{
	auto* pHouse = pPlayer->Account()->GetHouse();
	if(!pHouse)
		return;

	// information
	int ClientID = pPlayer->GetCID();
	VoteWrapper VInfo(ClientID, VWF_STYLE_STRICT_BOLD | VWF_SEPARATE, "\u2324 Selling a house (Information)");
	VInfo.Add("The gold will be returned by inbox.");
	VoteWrapper::AddEmptyline(ClientID);

	// sell house
	VoteWrapper(ClientID).AddOption("HOUSE_SELL", "Sell. (in reason send 777)");
	VoteWrapper::AddEmptyline(ClientID);
}

void CHouseManager::ShowDoorsController(CPlayer* pPlayer)
{
	auto* pHouse = pPlayer->Account()->GetHouse();
	if(!pHouse)
		return;

	// initialize variables
	int ClientID = pPlayer->GetCID();
	int DoorsNum = pHouse->GetDoorManager()->GetContainer().size();

	// information
	VoteWrapper VInfo(ClientID, VWF_STYLE_STRICT_BOLD | VWF_SEPARATE, "\u2324 House doors information");
	VInfo.Add("You can control your doors in the house");
	VInfo.Add("Your home has: {} doors.", DoorsNum);
	VoteWrapper::AddEmptyline(ClientID);

	// doors control
	VoteWrapper VDoors(ClientID, VWF_OPEN | VWF_STYLE_SIMPLE, "\u2743 Door's control");
	for(auto& [Number, DoorData] : pHouse->GetDoorManager()->GetContainer())
	{
		bool StateDoor = DoorData->IsClosed();
		VDoors.AddOption("HOUSE_DOOR", Number, "[{}] {} door", StateDoor ? "Closed" : "Open", DoorData->GetName());
	}
	VoteWrapper::AddEmptyline(ClientID);

	// show active access players to house
	auto* pHouseDoor = pHouse->GetDoorManager();
	VoteWrapper VAccess(ClientID, VWF_OPEN | VWF_STYLE_SIMPLE, "Permits have been granted:");
	{
		VAccess.BeginDepth();
		VAccess.Add("You and your eidolon have full access");
		for(auto& p : pHouseDoor->GetAccesses())
			VAccess.AddOption("HOUSE_INVITED_LIST_REMOVE", p, "Remove access from {}", Server()->GetAccountNickname(p));
		VAccess.EndDepth();
	}
	VoteWrapper::AddEmptyline(ClientID);

	// search result
	const char* pField = pPlayer->GetTempData().m_aPlayerSearchBuf[0] == '\0' ? "by reason field" : pPlayer->GetTempData().m_aPlayerSearchBuf;
	VoteWrapper VSearch(ClientID, VWF_SEPARATE_CLOSED | VWF_STYLE_SIMPLE, "You can add {} player's", pHouseDoor->GetAvailableAccessSlots());
	VSearch.AddOption("HOUSE_INVITED_LIST_FIND", "Field: [{}]", pField);
	VSearch.AddLine();
	VSearch.Add("Result by: {}", pPlayer->GetTempData().m_aPlayerSearchBuf);
	{
		VSearch.BeginDepth();
		if(pPlayer->GetTempData().m_aPlayerSearchBuf[0] != '\0')
		{
			bool Found = false;
			CSqlString<64> cPlayerName = CSqlString<64>(pPlayer->GetTempData().m_aPlayerSearchBuf);
			ResultPtr pRes = Database->Execute<DB::SELECT>("ID, Nick", "tw_accounts_data", "WHERE Nick LIKE '%%%s%%' LIMIT 5", cPlayerName.cstr());
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
			{
				VSearch.Add("Not found!", pPlayer->GetTempData().m_aPlayerSearchBuf);
			}
		}
		else
		{
			VSearch.Add("Set the reason for the search field");
		}
		VSearch.EndDepth();
	}
}

void CHouseManager::ShowPlantzonesControl(CPlayer* pPlayer) const
{
	auto* pHouse = pPlayer->Account()->GetHouse();
	if(!pHouse)
		return;

	int ClientID = pPlayer->GetCID();
	int PlantzonesNum = (int)pHouse->GetPlantzonesManager()->GetContainer().size();

	// information
	VoteWrapper VInfo(ClientID, VWF_STYLE_STRICT_BOLD | VWF_SEPARATE, "\u2324 Plant zones information");
	VInfo.Add("You can control your plant zones in the house");
	VInfo.Add("Your home has: {} plant zones.", PlantzonesNum);
	VoteWrapper::AddEmptyline(ClientID);

	// plant zones control
	VoteWrapper VPlantzones(ClientID, VWF_OPEN | VWF_STYLE_SIMPLE, "\u2743 Plant zone's control");
	for(auto& [ID, Plantzone] : pHouse->GetPlantzonesManager()->GetContainer())
		VPlantzones.AddMenu(MENU_HOUSE_PLANTZONE_SELECTED, ID, "Plant {} zone / {}", Plantzone.GetName(), GS()->GetItemInfo(Plantzone.GetItemID())->GetName());

	VoteWrapper::AddEmptyline(ClientID);
}

void CHouseManager::ShowPlantzoneEdit(CPlayer* pPlayer, int PlantzoneID) const
{
	auto* pHouse = pPlayer->Account()->GetHouse();
	if(!pHouse)
		return;

	auto* pPlantzone = pHouse->GetPlantzonesManager()->GetPlantzoneByID(PlantzoneID);
	if(!pPlantzone)
		return;

	int ClientID = pPlayer->GetCID();
	CItemDescription* pItem = GS()->GetItemInfo(pPlantzone->GetItemID());

	// information
	VoteWrapper VInfo(ClientID, VWF_SEPARATE | VWF_STYLE_STRICT_BOLD, "\u2741 Plant {} zone", pPlantzone->GetName());
	VInfo.Add("You can grow a plant on the property");
	VInfo.Add("Chance: {}%", s_GuildChancePlanting);
	VInfo.Add("Planted: {}", pItem->GetName());
	VoteWrapper::AddEmptyline(ClientID);

	// items list availables can be planted
	VoteWrapper VPlantItems(ClientID, VWF_OPEN | VWF_STYLE_SIMPLE, "\u2741 Possible items for planting");
	std::vector<ItemIdentifier> vItems = Core()->InventoryManager()->GetItemIDsCollectionByFunction(FUNCTION_PLANT);
	for(auto& ID : vItems)
	{
		CPlayerItem* pPlayerItem = pPlayer->GetItem(ID);
		if(pPlayerItem->HasItem() && ID != pPlantzone->GetItemID())
		{
			VPlantItems.AddOption("HOUSE_PLANT_ZONE_TRY", PlantzoneID, ID, "Try plant {} (has {})", pPlayerItem->Info()->GetName(), pPlayerItem->GetValue());
		}
	}
	VoteWrapper::AddEmptyline(ClientID);
}

bool CHouseManager::OnHandleMenulist(CPlayer* pPlayer, int Menulist)
{
	const int ClientID = pPlayer->GetCID();

	// buy house menu
	if(Menulist == MENU_HOUSE_BUY)
	{
		ShowBuyHouse(pPlayer, GetHouseByPos(pPlayer->GetCharacter()->m_Core.m_Pos));
		return true;
	}

	// menu house
	if(Menulist == MENU_HOUSE)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);
		ShowMenu(pPlayer);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// menu house sell
	if(Menulist == MENU_HOUSE_SELL)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_HOUSE);
		ShowSell(pPlayer);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// menu house doors
	if(Menulist == MENU_HOUSE_DOOR_LIST)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_HOUSE);
		ShowDoorsController(pPlayer);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// menu house plantzones
	if(Menulist == MENU_HOUSE_PLANTZONE_LIST)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_HOUSE);
		ShowPlantzonesControl(pPlayer);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// menu house plantzone selected
	if(Menulist == MENU_HOUSE_PLANTZONE_SELECTED)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_HOUSE_PLANTZONE_LIST);
		ShowPlantzoneEdit(pPlayer, pPlayer->m_VotesData.GetMenuTemporaryInteger());
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	if(Menulist == MENU_HOUSE_ACCESS_TO_DOOR)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_HOUSE);

		CHouseData* pHouse = pPlayer->Account()->GetHouse();
		if(!pHouse)
		{
			VoteWrapper(ClientID).Add("You not owner home!");
			return true;
		}

		// information
		VoteWrapper VInfo(ClientID, VWF_SEPARATE_CLOSED, "\u2697 Access to door's");
		VInfo.Add("You can manage access to your door's.");
		VInfo.Add("Add a limited number of players who can");
		VInfo.Add("enter the house regardless of door status");
		VInfo.AddLine();


		// back page
		VoteWrapper::AddBackpage(ClientID);
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

		bool Result = pHouse->GetDecorationManager()->StartDrawing(pPlayer);
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
		// check valid house
		auto *pHouse = pPlayer->Account()->GetHouse();
		if(!pHouse)
		{
			GS()->Chat(ClientID, "You do not have your own home!");
			return true;
		}

		// initialize variables
		const int& Useds = maximum(1, Get);
		const int& PlantzoneID = VoteID;
		const ItemIdentifier& ItemID = VoteID2;

		// check plantzone valid
		auto pPlantzone = pHouse->GetPlantzonesManager()->GetPlantzoneByID(PlantzoneID);
		if(!pPlantzone)
		{
			GS()->Chat(ClientID, "Plant zone not found.");
			return true;
		}

		// check is same plant item with current
		if(ItemID == pPlantzone->GetItemID())
		{
			GS()->Chat(ClientID, "This plant is already planted.");
			return true;
		}

		// check spend currency (planting item)
		if(pPlayer->Account()->SpendCurrency(Useds, ItemID))
		{
			// Check if the chance result is successful
			bool Success = false;
			Chance result(s_GuildChancePlanting);
			for(int i = 0; i < Useds && !Success; i++)
			{
				Success = result() ? true : Success;
				result.Update();
			}

			// result
			if(Success)
			{
				// Change the item in the plant zone to the planted item
				GS()->Chat(ClientID, "You have successfully planted the plant.");
				pPlantzone->ChangeItem(ItemID);
			}
			else
			{
				GS()->Chat(ClientID, "You failed to plant the plant.");
			}
			pPlayer->m_VotesData.UpdateVotesIf(MENU_HOUSE_PLANTZONE_SELECTED);
		}

		return true;
	}

	// move to house
	if(PPSTR(CMD, "HOUSE_SPAWN") == 0)
	{
		// check valid house
		auto* pHouse = pPlayer->Account()->GetHouse();
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

	// sell house
	if(PPSTR(CMD, "HOUSE_SELL") == 0)
	{
		// check player house
		auto *pHouse = pPlayer->Account()->GetHouse();
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

	// add gold to house bank
	if(PPSTR(CMD, "HOUSE_BANK_ADD") == 0)
	{
		// check valid house
		auto* pHouse = pPlayer->Account()->GetHouse();
		if(!pHouse)
		{
			GS()->Chat(ClientID, "You do not have your own home!");
			return true;
		}

		// check minimal
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

	// take gold from house bank
	if(PPSTR(CMD, "HOUSE_BANK_TAKE") == 0)
	{
		// check valid house
		auto* pHouse = pPlayer->Account()->GetHouse();
		if(!pHouse)
		{
			GS()->Chat(ClientID, "You do not have your own home!");
			return true;
		}

		// check minimal
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

	// interact with house door
	if(PPSTR(CMD, "HOUSE_DOOR") == 0)
	{
		// check valid house
		auto* pHouse = pPlayer->Account()->GetHouse();
		if(!pHouse)
		{
			GS()->Chat(ClientID, "You do not have your own home!");
			return true;
		}

		// reverse door house
		int UniqueDoorID = VoteID;
		pHouse->GetDoorManager()->Reverse(UniqueDoorID);
		pPlayer->m_VotesData.UpdateVotesIf(MENU_HOUSE_DOOR_LIST);
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
			//pHouse->SetPlantItemID(TryItemID);
			pPlayer->m_VotesData.UpdateCurrentVotes();
		}

		return true;
	}

	// house invited list
	if(PPSTR(CMD, "HOUSE_INVITED_LIST_FIND") == 0)
	{
		// check valid name
		if(PPSTR(GetText, "NULL") == 0)
		{
			GS()->Chat(ClientID, "Use please another name.");
			return true;
		}

		str_copy(pPlayer->GetTempData().m_aPlayerSearchBuf, GetText, sizeof(pPlayer->GetTempData().m_aPlayerSearchBuf));
		pPlayer->m_VotesData.UpdateVotesIf(MENU_HOUSE_DOOR_LIST);
		return true;
	}

	if(PPSTR(CMD, "HOUSE_INVITED_LIST_ADD") == 0)
	{
		const int UserID = VoteID;
		if(auto* pHouse = pPlayer->Account()->GetHouse())
			pHouse->GetDoorManager()->AddAccess(UserID);

		pPlayer->m_VotesData.UpdateVotesIf(MENU_HOUSE_DOOR_LIST);
		return true;
	}

	if(PPSTR(CMD, "HOUSE_INVITED_LIST_REMOVE") == 0)
	{
		const int UserID = VoteID;
		if(CHouseData* pHouse = pPlayer->Account()->GetHouse())
			pHouse->GetDoorManager()->RemoveAccess(UserID);

		pPlayer->m_VotesData.UpdateVotesIf(MENU_HOUSE_DOOR_LIST);
		return true;
	}

	return false;
}

void CHouseManager::ShowBuyHouse(CPlayer* pPlayer, CHouseData* pHouse)
{
	// check valid player and house
	if(!pHouse || !pPlayer)
		return;

	// initialize variables
	HouseIdentifier ID = pHouse->GetID();
	const int ClientID = pPlayer->GetCID();
	const char* pOwnerNickname = pHouse->HasOwner() ? Instance::Server()->GetAccountNickname(pHouse->GetAccountID()) : "No owner";

	// information
	VoteWrapper VInfo(ClientID, VWF_SEPARATE | VWF_STYLE_STRICT_BOLD, "\u2732 House information");
	VInfo.Add("Every player has the right to rent houses.", pHouse->GetPrice());
	VInfo.Add("An admission price is required for rentals.", pHouse->GetPrice());
	VoteWrapper::AddEmptyline(ClientID);

	// detail information
	VoteWrapper VDetail(ClientID, VWF_STYLE_SIMPLE, "Detail information about house.", ID, pHouse->GetClassName());
	VDetail.Add("Admission price: {} gold", pHouse->GetPrice());
	VDetail.Add("Owned by: {}", pOwnerNickname);
	VDetail.Add("Class: {}", pHouse->GetClassName());
	VoteWrapper::AddEmptyline(ClientID);

	// buy tab
	VoteWrapper VBuy(ClientID, VWF_OPEN|VWF_STYLE_SIMPLE, "Buying a house", pPlayer->GetItem(itGold)->GetValue());
	if(!pHouse->HasOwner())
		VBuy.AddOption("HOUSE_BUY", "Buy for {} golds", pHouse->GetPrice());
	else
		VBuy.Add("This house has already been purchased!");
	VoteWrapper::AddEmptyline(ClientID);
}

void CHouseManager::ShowMenu(CPlayer* pPlayer) const
{
	// check valid player
	if(!pPlayer)
		return;

	// check valid house
	auto* pHouse = pPlayer->Account()->GetHouse();
	if(!pHouse)
		return;

	// initialize variables
	int ClientID = pPlayer->GetCID();

	// information
	VoteWrapper VInfo(ClientID, VWF_SEPARATE | VWF_STYLE_STRICT_BOLD, "\u2747 Information about house");
	VInfo.Add("Class: {}", pHouse->GetClassName());
	VInfo.Add("Bank: {} golds", pHouse->GetBank()->Get());
	VoteWrapper::AddEmptyline(ClientID);

	// house bank
	VoteWrapper VBank(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_SIMPLE, "\u2727 Bank Management");
	VBank.Add("Your: {} | Bank: {} golds", pPlayer->GetItem(itGold)->GetValue(), pHouse->GetBank()->Get());
	VBank.AddOption("HOUSE_BANK_ADD", "Add. (Amount in a reason)");
	VBank.AddOption("HOUSE_BANK_TAKE", "Take. (Amount in a reason)");
	VoteWrapper::AddEmptyline(ClientID);

	// house management
	VoteWrapper VManagement(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_SIMPLE, "\u262B House Management");
	VManagement.AddOption("HOUSE_DECORATION", "Decoration editor");
	VManagement.AddMenu(MENU_HOUSE_PLANTZONE_LIST, "Plants");
	VManagement.AddMenu(MENU_HOUSE_DOOR_LIST, "Doors");
	VManagement.AddOption("HOUSE_SPAWN", "Move to house");
	VManagement.AddMenu(MENU_HOUSE_SELL, "Sell");
}

CHouseData* CHouseManager::GetHouse(HouseIdentifier ID) const
{
	auto pHouse = std::find_if(CHouseData::Data().begin(), CHouseData::Data().end(), [ID](auto& p) { return p->GetID() == ID; });
	return pHouse != CHouseData::Data().end() ? *pHouse : nullptr;
}

CHouseData* CHouseManager::GetHouseByPos(vec2 Pos) const
{
	auto pHouse = std::find_if(CHouseData::Data().begin(), CHouseData::Data().end(), [Pos](auto& p) { return distance(Pos, p->GetPos()) < 128.0f; });
	return pHouse != CHouseData::Data().end() ? *pHouse : nullptr;
}

CHouseData::CPlantzone* CHouseManager::GetHousePlantzoneByPos(vec2 Pos) const
{
	for(auto& p : CHouseData::Data())
	{
		for(auto& Plantzone : p->GetPlantzonesManager()->GetContainer())
		{
			if(distance(Pos, Plantzone.second.GetPos()) < Plantzone.second.GetRadius())
				return &Plantzone.second;
		}
	}

	return nullptr;
}
