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
		std::string JsonFarmzones = pRes->getString("Farmzones").c_str();
		std::string JsonProperties = pRes->getString("Properties").c_str();

		CHouse::CreateElement(ID)->Init(AccountID, ClassName, Price, Bank, WorldID, std::move(AccessList), std::move(JsonDoors), std::move(JsonFarmzones), std::move(JsonProperties));
	}

	Core()->ShowLoadingProgress("Houses", CHouse::Data().size());
}

void CHouseManager::OnTick()
{
	// Check if the current world ID is not equal to the main world (once use House get instance object self world id) ID and current tick
	if(GS()->GetWorldID() != MAIN_WORLD_ID || (Server()->Tick() % Server()->TickSpeed() != 0))
		return;

	// Calculate the remaining lifetime of a text update
	int LifeTime = (Server()->TickSpeed() * 10);

	// Get the house data
	const auto& HouseData = CHouse::Data();
	for(const auto& p : HouseData)
	{
		// Update the text with the remaining lifetime
		p->TextUpdate(LifeTime);
	}
}

void CHouseManager::OnHandleTimePeriod(TIME_PERIOD Period)
{
	for(auto& p : CHouse::Data())
		p->HandleTimePeriod(Period);
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

	// menu house farm zones list
	if(Menulist == MENU_HOUSE_FARMZONE_LIST)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_HOUSE);
		ShowFarmzonesControl(pPlayer);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// menu house farm zone selected
	if(Menulist == MENU_HOUSE_FARMZONE_SELECTED)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_HOUSE_FARMZONE_LIST);
		ShowFarmzoneEdit(pPlayer, pPlayer->m_VotesData.GetMenuTemporaryInteger());
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	return false;
}

bool CHouseManager::OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, const int VoteID, const int VoteID2, int Get, const char* GetText)
{
	const int ClientID = pPlayer->GetCID();

	// buy house
	if(PPSTR(CMD, "HOUSE_BUY") == 0)
	{
		const int HouseID = VoteID;
		if(auto* pHouse = GetHouse(HouseID))
			pHouse->Buy(pPlayer);
		return true;
	}

	// start decoration edit mode
	if(PPSTR(CMD, "HOUSE_DECORATION_EDIT") == 0)
	{
		// check house valid
		auto* pHouse = pPlayer->Account()->GetHouse();
		if(!pHouse)
		{
			GS()->Chat(ClientID, "You do not have your own home!");
			return true;
		}

		// result
		if(pHouse->GetDecorationManager()->StartDrawing(pPlayer))
			GS()->Chat(ClientID, "You can now draw decorations.");
		else
			GS()->Chat(ClientID, "You can't draw decorations.");
		return true;
	}

	// house farm zone try plant
	if(PPSTR(CMD, "HOUSE_FARM_ZONE_TRY_PLANT") == 0)
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
		const int& FarmzoneID = VoteID;
		const ItemIdentifier& ItemID = VoteID2;

		// check farmzone valid
		auto pFarmzone = pHouse->GetFarmzonesManager()->GetFarmzoneByID(FarmzoneID);
		if(!pFarmzone)
		{
			GS()->Chat(ClientID, "Farm zone not found.");
			return true;
		}

		// check is same planted item with current
		if(ItemID == pFarmzone->GetItemID())
		{
			GS()->Chat(ClientID, "This item has already been planted on this farm.");
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
				// update data
				GS()->Chat(ClientID, "You have successfully plant to farm zone.");
				pFarmzone->ChangeItem(ItemID);
			}
			else
			{
				GS()->Chat(ClientID, "You failed plant to farm zone.");
			}
			pPlayer->m_VotesData.UpdateVotesIf(MENU_HOUSE_FARMZONE_SELECTED);
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

	// house add to invited list
	if(PPSTR(CMD, "HOUSE_INVITED_LIST_ADD") == 0)
	{
		const int UserID = VoteID;
		if(auto* pHouse = pPlayer->Account()->GetHouse())
			pHouse->GetDoorManager()->AddAccess(UserID);

		pPlayer->m_VotesData.UpdateVotesIf(MENU_HOUSE_DOOR_LIST);
		return true;
	}

	// house remove from invited list
	if(PPSTR(CMD, "HOUSE_INVITED_LIST_REMOVE") == 0)
	{
		const int UserID = VoteID;
		if(auto* pHouse = pPlayer->Account()->GetHouse())
			pHouse->GetDoorManager()->RemoveAccess(UserID);

		pPlayer->m_VotesData.UpdateVotesIf(MENU_HOUSE_DOOR_LIST);
		return true;
	}

	return false;
}

void CHouseManager::ShowSell(CPlayer* pPlayer) const
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

void CHouseManager::ShowDoorsController(CPlayer* pPlayer) const
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

void CHouseManager::ShowFarmzonesControl(CPlayer* pPlayer) const
{
	auto* pHouse = pPlayer->Account()->GetHouse();
	if(!pHouse)
		return;

	// initialize variables
	int ClientID = pPlayer->GetCID();
	int FarmzonesNum = (int)pHouse->GetFarmzonesManager()->GetContainer().size();

	// information
	VoteWrapper VInfo(ClientID, VWF_STYLE_STRICT_BOLD | VWF_SEPARATE, "\u2324 Farm zones information");
	VInfo.Add("You can control your farm zones in the house");
	VInfo.Add("Your home has: {} farm zones.", FarmzonesNum);
	VoteWrapper::AddEmptyline(ClientID);

	// farm zones control
	VoteWrapper VFarmzones(ClientID, VWF_OPEN | VWF_STYLE_SIMPLE, "\u2743 Farm zone's control");
	for(auto& [ID, Farmzone] : pHouse->GetFarmzonesManager()->GetContainer())
		VFarmzones.AddMenu(MENU_HOUSE_FARMZONE_SELECTED, ID, "Farm {} zone / {}", Farmzone.GetName(), GS()->GetItemInfo(Farmzone.GetItemID())->GetName());

	VoteWrapper::AddEmptyline(ClientID);
}

void CHouseManager::ShowFarmzoneEdit(CPlayer* pPlayer, int FarmzoneID) const
{
	auto* pHouse = pPlayer->Account()->GetHouse();
	if(!pHouse)
		return;

	auto* pFarmzone = pHouse->GetFarmzonesManager()->GetFarmzoneByID(FarmzoneID);
	if(!pFarmzone)
		return;

	// initialize variables
	int ClientID = pPlayer->GetCID();
	CItemDescription* pItem = GS()->GetItemInfo(pFarmzone->GetItemID());

	// information
	VoteWrapper VInfo(ClientID, VWF_SEPARATE | VWF_STYLE_STRICT_BOLD, "\u2741 Farm {} zone", pFarmzone->GetName());
	VInfo.Add("You can grow a plant on the property");
	VInfo.Add("Chance: {}%", s_GuildChancePlanting);
	VInfo.Add("Planted: {}", pItem->GetName());
	VoteWrapper::AddEmptyline(ClientID);

	// items list availables can be planted
	VoteWrapper VPossiblePlanting(ClientID, VWF_OPEN | VWF_STYLE_SIMPLE, "\u2741 Possible items for planting");
	std::vector<ItemIdentifier> vItems = Core()->InventoryManager()->GetItemIDsCollectionByFunction(FUNCTION_FARMING);
	for(auto& ID : vItems)
	{
		CPlayerItem* pPlayerItem = pPlayer->GetItem(ID);
		if(pPlayerItem->HasItem() && ID != pFarmzone->GetItemID())
			VPossiblePlanting.AddOption("HOUSE_FARM_ZONE_TRY_PLANT", FarmzoneID, ID, "Try plant {} (has {})", pPlayerItem->Info()->GetName(), pPlayerItem->GetValue());
	}
	VoteWrapper::AddEmptyline(ClientID);
}

void CHouseManager::ShowBuyHouse(CPlayer* pPlayer, CHouse* pHouse)
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
	VManagement.AddOption("HOUSE_DECORATION_EDIT", "Decoration editor");
	VManagement.AddMenu(MENU_HOUSE_FARMZONE_LIST, "Farms");
	VManagement.AddMenu(MENU_HOUSE_DOOR_LIST, "Doors");
	VManagement.AddOption("HOUSE_SPAWN", "Move to house");
	VManagement.AddMenu(MENU_HOUSE_SELL, "Sell");
}

CHouse* CHouseManager::GetHouse(HouseIdentifier ID) const
{
	auto pHouse = std::find_if(CHouse::Data().begin(), CHouse::Data().end(), [ID](auto& p) { return p->GetID() == ID; });
	return pHouse != CHouse::Data().end() ? *pHouse : nullptr;
}

CHouse* CHouseManager::GetHouseByPos(vec2 Pos) const
{
	auto pHouse = std::find_if(CHouse::Data().begin(), CHouse::Data().end(), [Pos](auto& p) { return distance(Pos, p->GetPos()) < 128.0f; });
	return pHouse != CHouse::Data().end() ? *pHouse : nullptr;
}

CHouse::CFarmzone* CHouseManager::GetHouseFarmzoneByPos(vec2 Pos) const
{
	for(auto& p : CHouse::Data())
	{
		for(auto& Farmzone : p->GetFarmzonesManager()->GetContainer())
		{
			if(distance(Pos, Farmzone.second.GetPos()) < Farmzone.second.GetRadius())
				return &Farmzone.second;
		}
	}

	return nullptr;
}