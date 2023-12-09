/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "HouseManager.h"

#include <game/server/gamecontext.h>

#include <game/server/mmocore/Components/Inventory/InventoryManager.h>

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
			vec2 DoorPos(pRes->getInt("DoorX"), pRes->getInt("DoorY"));
			vec2 TextPos(pRes->getInt("TextX"), pRes->getInt("TextY"));
			vec2 PlantPos(pRes->getInt("PlantX"), pRes->getInt("PlantY"));
			int PlantItemID = pRes->getInt("PlantID");
			int WorldID = pRes->getInt("WorldID");
			std::string AccessData = pRes->getString("AccessData").c_str();

			CHouseData::CreateElement(ID)->Init(AccountID, ClassName, Price, Bank, Pos, TextPos, DoorPos, PlantPos, CItem(PlantItemID, 1), WorldID, AccessData);
		}

		Job()->ShowLoadingProgress("Houses", CHouseData::Data().size());
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
		_DEF_TILE_ENTER_ZONE_SEND_MSG_INFO(pPlayer);
		GS()->UpdateVotes(ClientID, pPlayer->m_CurrentVoteMenu);
		return true;
	}
	if(pChr->GetHelper()->TileExit(IndexCollision, TILE_PLAYER_HOUSE))
	{
		_DEF_TILE_EXIT_ZONE_SEND_MSG_INFO(pPlayer);
		GS()->UpdateVotes(ClientID, pPlayer->m_CurrentVoteMenu);
		return true;
	}

	return false;
}

bool CHouseManager::OnHandleMenulist(CPlayer* pPlayer, int Menulist, bool ReplaceMenu)
{
	const int ClientID = pPlayer->GetCID();
	if(ReplaceMenu)
	{
		CCharacter* pChr = pPlayer->GetCharacter();
		if(!pChr || !pChr->IsAlive())
		{
			return false;
		}

		if(pChr->GetHelper()->BoolIndex(TILE_PLAYER_HOUSE))
		{
			if(CHouseData* pHouse = GetHouseByPos(pChr->m_Core.m_Pos))
				ShowHouseMenu(pPlayer, pHouse);

			return true;
		}
		return false;
	}


	if(Menulist == MENU_HOUSE)
	{
		pPlayer->m_LastVoteMenu = MENU_MAIN;

		CHouseData* pHouse = pPlayer->Account()->GetHouse();
		if(!pHouse)
		{
			GS()->AVL(ClientID, "null", "You not owner home!");
			return true;
		}

		HouseIdentifier ID = pHouse->GetID();
		bool StateDoor = pHouse->GetDoor()->GetState();

		GS()->AVH(ClientID, TAB_HOUSE_STAT, "House stats {INT} Class {STR} Door [{STR}]", ID, pHouse->GetClassName(), StateDoor ? "Closed" : "Opened");
		GS()->AVM(ClientID, "null", NOPE, TAB_HOUSE_STAT, "/doorhouse - interactive with door.");
		GS()->AV(ClientID, "null");

		GS()->AVH(ClientID, TAB_HOUSE_SAFE_INTERACTIVE, "◍ House safe is: {VAL} Gold", pHouse->GetBank()->Get());
		GS()->AddVoteItemValue(ClientID, itGold, TAB_HOUSE_SAFE_INTERACTIVE);
		GS()->AVM(ClientID, "HOUSE_BANK_ADD", 1, TAB_HOUSE_SAFE_INTERACTIVE, "Add gold. (Amount in a reason)");
		GS()->AVM(ClientID, "HOUSE_BANK_TAKE", 1, TAB_HOUSE_SAFE_INTERACTIVE, "Take gold. (Amount in a reason)");
		GS()->AV(ClientID, "null");

		GS()->AVH(ClientID, TAB_HOUSE_MANAGING, "▤ Managing your home");
		GS()->AVM(ClientID, "HOUSE_SPAWN", ID, TAB_HOUSE_MANAGING, "Teleport to your house");
		GS()->AVM(ClientID, "HOUSE_DOOR", ID, TAB_HOUSE_MANAGING, "Change state door to [\"{STR}\"]", StateDoor ? "Open" : "Closed");
		GS()->AVM(ClientID, "HOUSE_SELL", ID, TAB_HOUSE_MANAGING, "Sell your house (in reason 777)");

		if(GS()->IsPlayerEqualWorld(ClientID, pHouse->GetWorldID()))
		{
			GS()->AVM(ClientID, "MENU", MENU_HOUSE_ACCESS_TO_DOOR, TAB_HOUSE_MANAGING, "Settings up accesses to your door");
			GS()->AVM(ClientID, "MENU", MENU_HOUSE_DECORATION, TAB_HOUSE_MANAGING, "Settings your decorations");
			GS()->AVM(ClientID, "MENU", MENU_HOUSE_PLANTS, TAB_HOUSE_MANAGING, "Settings your plants");
		}
		else
		{
			GS()->AVM(ClientID, "null", NOPE, TAB_HOUSE_MANAGING, "More settings allow, only on house zone");
		}

		GS()->AddVotesBackpage(ClientID);
		return true;
	}

	if(Menulist == MENU_HOUSE_DECORATION)
	{
		pPlayer->m_LastVoteMenu = MENU_HOUSE;
		CHouseData* pHouse = pPlayer->Account()->GetHouse();
		if(!pHouse)
		{
			GS()->AVL(ClientID, "null", "You not owner home!");
			return true;
		}

		GS()->AVH(ClientID, TAB_INFO_DECORATION, "Decorations Information");
		GS()->AVM(ClientID, "null", NOPE, TAB_INFO_DECORATION, "Add: SELECT your item in list. SELECT (Add to house),");
		GS()->AVM(ClientID, "null", NOPE, TAB_INFO_DECORATION, "later press (ESC) and mouse select position");
		GS()->AVM(ClientID, "null", NOPE, TAB_INFO_DECORATION, "Return in inventory: SELECT down your decorations");
		GS()->AVM(ClientID, "null", NOPE, TAB_INFO_DECORATION, "and press (Back to inventory).");

		Job()->Item()->ListInventory(ClientID, ItemType::TYPE_DECORATION);
		GS()->AV(ClientID, "null");
		pHouse->ShowDecorationList();
		GS()->AddVotesBackpage(ClientID);
		return true;
	}

	if(Menulist == MENU_HOUSE_PLANTS)
	{
		pPlayer->m_LastVoteMenu = MENU_HOUSE;

		CHouseData* pHouse = pPlayer->Account()->GetHouse();
		if(!pHouse)
		{
			GS()->AVL(ClientID, "null", "You not owner home!");
			return true;
		}

		GS()->AVH(ClientID, TAB_INFO_HOUSE_PLANT, "Plants Information");
		GS()->AVM(ClientID, "null", NOPE, TAB_INFO_HOUSE_PLANT, "Select an item and then click on 'To Plant'.");
		GS()->AV(ClientID, "null");

		GS()->AVM(ClientID, "null", NOPE, NOPE, "Housing active plants: {STR}", pHouse->GetPlantedItem()->Info()->GetName());
		GS()->Mmo()->Item()->ListInventory(ClientID, FUNCTION_PLANTS);
		GS()->AddVotesBackpage(ClientID);
		return true;
	}

	if(Menulist == MENU_HOUSE_ACCESS_TO_DOOR)
	{
		pPlayer->m_LastVoteMenu = MENU_HOUSE;

		CHouseData* pHouse = pPlayer->Account()->GetHouse();
		if(!pHouse)
		{
			GS()->AVL(ClientID, "null", "You not owner home!");
			return true;
		}

		// information
		GS()->AVH(ClientID, TAB_INFO_HOUSE_INVITES_TO_DOOR, "Player's access to door.");
		GS()->AVM(ClientID, "null", NOPE, TAB_INFO_HOUSE_INVITES_TO_DOOR, "You can add a limited number of players who can");
		GS()->AVM(ClientID, "null", NOPE, TAB_INFO_HOUSE_INVITES_TO_DOOR, "enter the house regardless of door status");
		GS()->AV(ClientID, "null");

		// show active access players to house
		CHouseDoorData* pHouseDoor = pHouse->GetDoor();
		GS()->AVH(ClientID, TAB_HOUSE_ACCESS_TO_DOOR_REMOVE, "You can add {INT} player's.", pHouseDoor->GetAvailableAccessSlots());
		GS()->AVM(ClientID, "null", NOPE, TAB_HOUSE_ACCESS_TO_DOOR_REMOVE, "You and your eidolon have full access");
		for(auto& p : pHouseDoor->GetAccesses())
		{
			GS()->AVM(ClientID, "HOUSE_INVITED_LIST_REMOVE", p, TAB_HOUSE_ACCESS_TO_DOOR_REMOVE, "Remove access from {STR}", Server()->GetAccountNickname(p));
		}

		// field to find player for append
		GS()->AV(ClientID, "null");
		GS()->AV(ClientID, "null", "Use reason. The entered value can be a partial.");
		GS()->AV(ClientID, "null", "Example: Find player: [], in reason nickname.");
		GS()->AVM(ClientID, "HOUSE_INVITED_LIST_FIND", 1, NOPE, "Find player: [{STR}]", pPlayer->GetTempData().m_aPlayerSearchBuf);
		GS()->AV(ClientID, "null");

		// search result
		GS()->AVH(ClientID, TAB_HOUSE_ACCESS_TO_DOOR_ADD, "Search result by [{STR}]", pPlayer->GetTempData().m_aPlayerSearchBuf);
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
				GS()->AVM(ClientID, "HOUSE_INVITED_LIST_ADD", UserID, TAB_HOUSE_ACCESS_TO_DOOR_ADD, "Give access for {STR}", cPlayerName.cstr());
				Found = true;
			}
			if(!Found)
			{
				GS()->AVM(ClientID, "null", NOPE, TAB_HOUSE_ACCESS_TO_DOOR_ADD, "Players for the request {STR} not found!", pPlayer->GetTempData().m_aPlayerSearchBuf);
			}
		}
		else
		{
			GS()->AVM(ClientID, "null", NOPE, TAB_HOUSE_ACCESS_TO_DOOR_ADD, "Set the reason for the search field");
		}

		// back page
		GS()->AddVotesBackpage(ClientID);
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
			pPlayer->GetTempData().m_TempTeleportPos = pHouse->GetPos();
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
		GS()->UpdateVotes(ClientID, MENU_MAIN);
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
		GS()->StrongUpdateVotes(ClientID, MENU_HOUSE);
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
		GS()->StrongUpdateVotes(ClientID, MENU_HOUSE);
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
		pHouse->GetDoor()->Reverse();
		GS()->StrongUpdateVotes(ClientID, MENU_HOUSE);
		return true;
	}

	if(PPSTR(CMD, "DECORATION_HOUSE_ADD") == 0)
	{
		// check player house
		CHouseData* pHouse = pPlayer->Account()->GetHouse();
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
		CHouseData* pHouse = pPlayer->Account()->GetHouse();
		if(!pHouse)
		{
			GS()->Chat(ClientID, "You do not have your own home!");
			return true;
		}

		// remove decoration
		if(pHouse->RemoveDecoration(VoteID))
		{
			CPlayerItem* pPlayerItem = pPlayer->GetItem(VoteID2);
			GS()->Chat(ClientID, "You back to the backpack {STR}!", pPlayerItem->Info()->GetName());
			pPlayerItem->Add(1);
			GS()->StrongUpdateVotes(ClientID, MENU_HOUSE_DECORATION);
		}

		return true;
	}

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
		if(pPlayer->SpendCurrency(1, TryItemID))
		{
			const int ChanceSuccesful = VoteID2;
			if(ChanceSuccesful != 0)
			{
				GS()->Chat(ClientID, "Unfortunately plant did not take root!");
				GS()->UpdateVotes(ClientID, pPlayer->m_CurrentVoteMenu);
				return true;
			}

			GS()->Chat(-1, "Congratulations {STR}, planted at home {STR}!", Server()->ClientName(ClientID), GS()->GetItemInfo(TryItemID)->GetName());
			pHouse->SetPlantItemID(TryItemID);
			GS()->UpdateVotes(ClientID, pPlayer->m_CurrentVoteMenu);
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
		GS()->StrongUpdateVotes(ClientID, MENU_HOUSE_ACCESS_TO_DOOR);
		return true;
	}

	if(PPSTR(CMD, "HOUSE_INVITED_LIST_ADD") == 0)
	{
		const int UserID = VoteID;
		if(CHouseData* pHouse = pPlayer->Account()->GetHouse())
			pHouse->GetDoor()->AddAccess(UserID);

		GS()->StrongUpdateVotes(ClientID, MENU_HOUSE_ACCESS_TO_DOOR);
		return true;
	}

	if(PPSTR(CMD, "HOUSE_INVITED_LIST_REMOVE") == 0)
	{
		const int UserID = VoteID;
		if(CHouseData* pHouse = pPlayer->Account()->GetHouse())
			pHouse->GetDoor()->RemoveAccess(UserID);

		GS()->StrongUpdateVotes(ClientID, MENU_HOUSE_ACCESS_TO_DOOR);
		return true;
	}

	return false;
}

/* #########################################################################
	MENUS HOUSES
######################################################################### */
void CHouseManager::ShowHouseMenu(CPlayer* pPlayer, CHouseData* pHouse)
{
	HouseIdentifier ID = pHouse->GetID();
	const int ClientID = pPlayer->GetCID();

	GS()->AVH(ClientID, TAB_INFO_HOUSE, "House {INT} . {STR}", ID, pHouse->GetClassName());
	GS()->AVM(ClientID, "null", NOPE, TAB_INFO_HOUSE, "Owner House: {STR}", Server()->GetAccountNickname(pHouse->GetAccountID()));

	GS()->AV(ClientID, "null");
	GS()->AddVoteItemValue(ClientID, itGold);
	GS()->AV(ClientID, "null");

	if(pHouse->GetAccountID() <= 0)
	{
		GS()->AVM(ClientID, "HOUSE_BUY", ID, NOPE, "Buy this house. Price {VAL}gold", pHouse->GetPrice());
	}
	else
	{
		GS()->AVM(ClientID, "null", ID, NOPE, "This house has already been purchased!");
	}

	GS()->AV(ClientID, "null");
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