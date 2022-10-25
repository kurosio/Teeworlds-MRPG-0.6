/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "HouseCore.h"

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>

#include <game/server/mmocore/GameEntities/decoration_houses.h>
#include <game/server/mmocore/GameEntities/jobitems.h>
#include "Entities/HouseDoor.h"

#include <game/server/mmocore/Components/Inventory/InventoryCore.h>

void CHouseCore::OnInitWorld(const char* pWhereLocalWorld)
{
	// load house
	const auto InitHouses = Sqlpool.Prepare<DB::SELECT>("*", "tw_houses", pWhereLocalWorld);
	InitHouses->AtExecute([this](IServer*, ResultPtr pRes)
	{
		while (pRes->next())
		{
			HouseIdentifier ID = pRes->getInt("ID");
			int AccountID = pRes->getInt("UserID");
			std::string ClassName = pRes->getString("Class").c_str();
			int Price = pRes->getInt("Price");
			int Bank = pRes->getInt("HouseBank");
			vec2 Pos(pRes->getInt("PosX"), pRes->getInt("PosY"));
			vec2 DoorPos(pRes->getInt("DoorX"), pRes->getInt("DoorY"));
			vec2 PlantPos(pRes->getInt("PlantX"), pRes->getInt("PlantY"));
			int PlantItemID = pRes->getInt("PlantID");
			int WorldID = pRes->getInt("WorldID");

			CHouseData(ID).Init(AccountID, ClassName, Price, Bank, Pos, { DoorPos }, PlantPos, PlantItemID, WorldID);
		}
		
		Job()->ShowLoadingProgress("Houses", CHouseData::ms_aHouse.size());
	});


	// load decoration
	const auto InitHouseDecorations = Sqlpool.Prepare<DB::SELECT>("*", "tw_houses_decorations", pWhereLocalWorld);
	InitHouseDecorations->AtExecute([this](IServer*, ResultPtr pRes)
	{
		while (pRes->next())
		{
			const int DecoID = pRes->getInt("ID");
			m_aDecorationHouse[DecoID] = new CDecorationHouses(&GS()->m_World, vec2(pRes->getInt("PosX"), pRes->getInt("PosY")), , pRes->getInt("DecoID"));
		}
		Job()->ShowLoadingProgress("Houses Decorations", m_aDecorationHouse.size());
	});
}

bool CHouseCore::OnHandleTile(CCharacter* pChr, int IndexCollision)
{
	CPlayer* pPlayer = pChr->GetPlayer();
	const int ClientID = pPlayer->GetCID();

	if(pChr->GetHelper()->TileEnter(IndexCollision, TILE_PLAYER_HOUSE))
	{
		GS()->Chat(ClientID, "You can see menu in the votes!");
		GS()->UpdateVotes(ClientID, pPlayer->m_OpenVoteMenu);
		return true;
	}
	if(pChr->GetHelper()->TileExit(IndexCollision, TILE_PLAYER_HOUSE))
	{
		GS()->Chat(ClientID, "You left the active zone, menu is restored!");
		GS()->UpdateVotes(ClientID, pPlayer->m_OpenVoteMenu);
		return true;
	}

	return false;
}

bool CHouseCore::OnHandleMenulist(CPlayer* pPlayer, int Menulist, bool ReplaceMenu)
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

	if(Menulist == MENU_HOUSE_DECORATION)
	{
		pPlayer->m_LastVoteMenu = MENU_HOUSE;
		GS()->AVH(ClientID, TAB_INFO_DECORATION, "Decorations Information");
		GS()->AVM(ClientID, "null", NOPE, TAB_INFO_DECORATION, "Add: SELECT your item in list. SELECT (Add to house),");
		GS()->AVM(ClientID, "null", NOPE, TAB_INFO_DECORATION, "later press (ESC) and mouse select position");
		GS()->AVM(ClientID, "null", NOPE, TAB_INFO_DECORATION, "Return in inventory: SELECT down your decorations");
		GS()->AVM(ClientID, "null", NOPE, TAB_INFO_DECORATION, "and press (Back to inventory).");

		Job()->Item()->ListInventory(ClientID, ItemType::TYPE_DECORATION);
		GS()->AV(ClientID, "null");
		pPlayer->Acc().GetHouse()->ShowDecorations();
		GS()->AddVotesBackpage(ClientID);
		return true;
	}

	if(Menulist == MENU_HOUSE)
	{
		pPlayer->m_LastVoteMenu = MENU_MAIN;

		ShowPersonalHouse(pPlayer);
		GS()->AddVotesBackpage(ClientID);
		return true;
	}

	if(Menulist == MENU_HOUSE_PLANTS)
	{
		pPlayer->m_LastVoteMenu = MENU_HOUSE;

		CHouseData* pHouse = pPlayer->Acc().GetHouse();
		if(!pHouse)
		{
			GS()->AVL(ClientID, "null", "You not owner home!");
			return true;
		}

		GS()->AVH(ClientID, TAB_INFO_HOUSE_PLANT, "Plants Information");
		GS()->AVM(ClientID, "null", NOPE, TAB_INFO_HOUSE_PLANT, "SELECT item and in tab select 'To plant'");
		GS()->AV(ClientID, "null");

		GS()->AVM(ClientID, "null", NOPE, NOPE, "Housing Active Plants: {STR}", GS()->GetItemInfo(pHouse->GetPlantItemID())->GetName());
		GS()->Mmo()->Item()->ListInventory(ClientID, FUNCTION_PLANTS);
		GS()->AddVotesBackpage(ClientID);
		return true;
	}

	return false;
}

bool CHouseCore::OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, const int VoteID, const int VoteID2, int Get, const char* GetText)
{
	const int ClientID = pPlayer->GetCID();
	if(PPSTR(CMD, "BUYHOUSE") == 0)
	{
		const int HouseID = VoteID;
		if(CHouseData* pHouse = GetHouse(HouseID))
		{
			pHouse->Buy(pPlayer);
		}

		return true;
	}

	if(PPSTR(CMD, "HSPAWN") == 0)
	{
		if(!pPlayer->GetCharacter())
		{
			return true;
		}
		
		CHouseData* pHouse = pPlayer->Acc().GetHouse();
		if(!pHouse)
		{
			GS()->Chat(ClientID, "You not owner home!");
			return true;
		}

		if(!GS()->IsPlayerEqualWorld(ClientID, pHouse->GetWorldID()))
		{
			pPlayer->GetTempData().m_TempTeleportPos = pHouse->GetPos();
			pPlayer->ChangeWorld(pHouse->GetWorldID());
			return true;
		}

		pPlayer->GetCharacter()->ChangePosition(pHouse->GetPos());
		return true;
	}

	if(PPSTR(CMD, "HSELL") == 0)
	{
		CHouseData* pHouse = pPlayer->Acc().GetHouse();
		if(!pHouse || Get != 777)
		{
			GS()->Chat(ClientID, "A verification number was entered incorrectly.");
			return true;
		}

		pHouse->Sell();
		GS()->UpdateVotes(ClientID, MENU_MAIN);
		return true;
	}

	if(PPSTR(CMD, "HOUSEADD") == 0)
	{
		if(Get < 100)
		{
			GS()->Chat(ClientID, "Minimal 100 gold.");
			return true;
		}

		if(CHouseData* pHouse = pPlayer->Acc().GetHouse(); pHouse && pPlayer->SpendCurrency(Get))
		{
			pHouse->GetBank()->Add(Get);
			GS()->StrongUpdateVotes(ClientID, MENU_HOUSE);
		}
		return true;
	}

	if(PPSTR(CMD, "HOUSETAKE") == 0)
	{
		if(Get < 100)
		{
			GS()->Chat(ClientID, "Minimal 100 gold.");
			return true;
		}

		if(CHouseData* pHouse = pPlayer->Acc().GetHouse())
		{
			pHouse->GetBank()->Take(Get);
			GS()->StrongUpdateVotes(ClientID, MENU_HOUSE);
		}
		return true;
	}

	if(PPSTR(CMD, "HOUSEDOOR") == 0)
	{
		if(CHouseData* pHouse = pPlayer->Acc().GetHouse())
		{
			pHouse->GetDoor()->Reverse();
			GS()->StrongUpdateVotes(ClientID, MENU_HOUSE);
		}
		return true;
	}

	if(PPSTR(CMD, "DECOSTART") == 0)
	{
		CHouseData* pHouse = pPlayer->Acc().GetHouse();
		if(!pHouse)
		{
			GS()->Chat(ClientID, "You not owner home!");
			return true;
		}

		if(distance(pHouse->GetPos(), pPlayer->GetCharacter()->m_Core.m_Pos) > 600)
		{
			GS()->Chat(ClientID, "Long distance from the house, or you do not own the house!");
			return true;
		}

		GS()->StartCustomVotes(ClientID, MENU_INVENTORY);
		GS()->AV(ClientID, "null", "Please close vote and press Left Mouse,");
		GS()->AV(ClientID, "null", "on position where add decoration!");
		GS()->AddVotesBackpage(ClientID);
		GS()->EndCustomVotes(ClientID);

		pPlayer->GetTempData().m_TempDecoractionID = VoteID;
		pPlayer->GetTempData().m_TempDecorationType = DECORATIONS_HOUSE;
		return true;
	}

	if(PPSTR(CMD, "DECODELETE") == 0)
	{
		CHouseData* pHouse = pPlayer->Acc().GetHouse();
		if(!pHouse)
		{
			GS()->Chat(ClientID, "You not owner home!");
			return true;
		}

		if(pHouse->RemoveDecoration(VoteID))
		{
			CPlayerItem* pPlayerItem = pPlayer->GetItem(VoteID2);
			GS()->Chat(ClientID, "You back to the backpack {STR}!", pPlayerItem->Info()->GetName());
			pPlayerItem->Add(1);
		}

		GS()->StrongUpdateVotes(ClientID, MENU_HOUSE_DECORATION);
		return true;
	}

	if(PPSTR(CMD, "HOMEPLANTSET") == 0)
	{
		CHouseData* pHouse = pPlayer->Acc().GetHouse();
		if(!pHouse)
		{
			GS()->Chat(ClientID, "You not owner home!");
			return true;
		}

		if(pHouse->GetPlantItemID() <= 0)
		{
			GS()->Chat(ClientID, "Your home does not support plants!");
			return true;
		}

		ItemIdentifier TryItemID = VoteID;
		if(pPlayer->SpendCurrency(1, TryItemID))
		{
			const int ChanceSuccesful = VoteID2;
			if(ChanceSuccesful != 0)
			{
				GS()->Chat(ClientID, "Unfortunately plant did not take root!");
				GS()->UpdateVotes(ClientID, pPlayer->m_OpenVoteMenu);
				return true;
			}

			GS()->Chat(-1, "Congratulations {STR}, planted at home {STR}!", Server()->ClientName(ClientID), GS()->GetItemInfo(TryItemID)->GetName());
			pHouse->UpdatePlantItemID(TryItemID);
			GS()->UpdateVotes(ClientID, pPlayer->m_OpenVoteMenu);
		}

		return true;
	}

	return false;
}

/* #########################################################################
	MENUS HOUSES
######################################################################### */
void CHouseCore::ShowHouseMenu(CPlayer* pPlayer, CHouseData* pHouse)
{
	HouseIdentifier ID = pHouse->GetID();
	const int ClientID = pPlayer->GetCID();

	GS()->AVH(ClientID, TAB_INFO_HOUSE, "House {INT} . {STR}", ID, pHouse->GetClassName());
	GS()->AVM(ClientID, "null", NOPE, TAB_INFO_HOUSE, "Owner House: {STR}", Job()->PlayerName(pHouse->GetAccountID()));

	GS()->AV(ClientID, "null");
	GS()->ShowVotesItemValueInformation(pPlayer, itGold);
	GS()->AV(ClientID, "null");

	if(pHouse->GetAccountID() <= 0)
	{
		GS()->AVM(ClientID, "BUYHOUSE", ID, NOPE, "Buy this house. Price {VAL}gold", pHouse->GetPrice());
	}
	else
	{
		GS()->AVM(ClientID, "null", ID, NOPE, "This house has already been purchased!");
	}

	GS()->AV(ClientID, "null");
}

void CHouseCore::ShowPersonalHouse(CPlayer* pPlayer)
{
	const int ClientID = pPlayer->GetCID();

	CHouseData* pHouse = pPlayer->Acc().GetHouse();
	if(!pHouse)
	{
		GS()->AVL(ClientID, "null", "You not owner home!");
		return;
	}

	HouseIdentifier ID = pHouse->GetID();
	bool StateDoor = pHouse->GetDoor()->GetState();

	GS()->AVH(ClientID, TAB_HOUSE_STAT, "House stats {INT} Class {STR} Door [{STR}]", ID, pHouse->GetClassName(), StateDoor ? "Closed" : "Opened");
	GS()->AVM(ClientID, "null", NOPE, TAB_HOUSE_STAT, "/doorhouse - interactive with door.");
	GS()->AVM(ClientID, "null", NOPE, TAB_HOUSE_STAT, "- - - - - - - - - -");
	GS()->AVM(ClientID, "null", NOPE, TAB_HOUSE_STAT, "Notes: Minimal operation house balance 100gold");
	GS()->AVM(ClientID, "null", NOPE, TAB_HOUSE_STAT, "In your safe is: {VAL}gold", pHouse->GetBank()->Get());
	GS()->AV(ClientID, "null");

	GS()->AVL(ClientID, "null", "◍ Your gold: {VAL}gold", pPlayer->GetItem(itGold)->GetValue());
	GS()->AVM(ClientID, "HOUSEADD", 1, NOPE, "Add to the safe gold. (Amount in a reason)");
	GS()->AVM(ClientID, "HOUSETAKE", 1, NOPE, "Take the safe gold. (Amount in a reason)");
	GS()->AV(ClientID, "null");

	GS()->AVL(ClientID, "null", "▤ House system");
	GS()->AVM(ClientID, "HOUSEDOOR", ID, NOPE, "Change state to [\"{STR}\"]", StateDoor ? "OPEN" : "CLOSED");
	GS()->AVM(ClientID, "HSPAWN", 1, NOPE, "Teleport to your house");
	GS()->AVM(ClientID, "HSELL", ID, NOPE, "Sell your house (in reason 777)");

	if(GS()->IsPlayerEqualWorld(ClientID, pHouse->GetWorldID()))
	{
		GS()->AVM(ClientID, "MENU", MENU_HOUSE_DECORATION, NOPE, "Settings Decorations");
		GS()->AVM(ClientID, "MENU", MENU_HOUSE_PLANTS, NOPE, "Settings Plants");
	}
	else
	{
		GS()->AVM(ClientID, "null", MENU_HOUSE_DECORATION, NOPE, "More settings allow, only on house zone");
	}
}

CHouseData* CHouseCore::GetHouseByAccountID(int AccountID)
{
	CHouseData* pData = nullptr;

	for(auto& pHouse : CHouseData::Data())
	{
		if(pHouse.GetAccountID() == AccountID)
		{
			pData = &pHouse;
			break;
		}
	}

	return pData;
}

CHouseData* CHouseCore::GetHouse(HouseIdentifier ID)
{
	return std::find_if(CHouseData::Data().begin(), CHouseData::Data().end(), [ID](auto& p) { return p.GetID() == ID; });
}

CHouseData* CHouseCore::GetHouseByPos(vec2 Pos)
{
	return std::find_if(CHouseData::Data().begin(), CHouseData::Data().end(), [Pos](auto& p) { return distance(Pos, p.GetPos()) < 128.0f; });
}

CHouseData* CHouseCore::GetHouseByPlantPos(vec2 Pos)
{
	return std::find_if(CHouseData::Data().begin(), CHouseData::Data().end(), [Pos](auto& p) { return distance(Pos, p.GetPlantPos()) < 300.0f; });
}