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
			const int HouseID = pRes->getInt("ID");
			CHouseData::ms_aHouse[HouseID].m_DoorX = pRes->getInt("DoorX");
			CHouseData::ms_aHouse[HouseID].m_DoorY = pRes->getInt("DoorY");
			CHouseData::ms_aHouse[HouseID].m_PosX = pRes->getInt("PosX");
			CHouseData::ms_aHouse[HouseID].m_PosY = pRes->getInt("PosY");
			CHouseData::ms_aHouse[HouseID].m_UserID = pRes->getInt("UserID");
			CHouseData::ms_aHouse[HouseID].m_Price = pRes->getInt("Price");
			CHouseData::ms_aHouse[HouseID].m_Bank = pRes->getInt("HouseBank");
			CHouseData::ms_aHouse[HouseID].m_WorldID = pRes->getInt("WorldID");
			str_copy(CHouseData::ms_aHouse[HouseID].m_aClass, pRes->getString("Class").c_str(), sizeof CHouseData::ms_aHouse[HouseID].m_aClass);
			CHouseData::ms_aHouse[HouseID].m_PlantID = pRes->getInt("PlantID");
			CHouseData::ms_aHouse[HouseID].m_PlantPosX = pRes->getInt("PlantX");
			CHouseData::ms_aHouse[HouseID].m_PlantPosY = pRes->getInt("PlantY");
			if (GS()->GetWorldID() == CHouseData::ms_aHouse[HouseID].m_WorldID && CHouseData::ms_aHouse[HouseID].m_UserID > 0 && !CHouseData::ms_aHouse[HouseID].m_Door)
			{
				CHouseData::ms_aHouse[HouseID].m_Door = nullptr;
				CHouseData::ms_aHouse[HouseID].m_Door = new HouseDoor(&GS()->m_World, vec2(CHouseData::ms_aHouse[HouseID].m_DoorX, CHouseData::ms_aHouse[HouseID].m_DoorY));
			}
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
			m_aDecorationHouse[DecoID] = new CDecorationHouses(&GS()->m_World, vec2(pRes->getInt("PosX"),
				pRes->getInt("PosY")), pRes->getInt("HouseID"), pRes->getInt("DecoID"));
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
			const int HouseID = GetHouse(pChr->m_Core.m_Pos);
			if(HouseID > 0)
			{
				ShowHouseMenu(pPlayer, HouseID);
			}
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
		ShowDecorationList(pPlayer);
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
		const int HouseID = OwnerHouseID(pPlayer->Acc().m_UserID);
		const int PlantItemID = GetPlantsID(HouseID);

		GS()->AVH(ClientID, TAB_INFO_HOUSE_PLANT, "Plants Information");
		GS()->AVM(ClientID, "null", NOPE, TAB_INFO_HOUSE_PLANT, "SELECT item and in tab select 'To plant'");
		GS()->AV(ClientID, "null");

		GS()->AVM(ClientID, "null", NOPE, NOPE, "Housing Active Plants: {STR}", GS()->GetItemInfo(PlantItemID)->GetName());
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
		BuyHouse(HouseID, pPlayer);
		return true;
	}

	if(PPSTR(CMD, "HSPAWN") == 0)
	{
		if(!pPlayer->GetCharacter())
		{
			return true;
		}

		const int HouseID = PlayerHouseID(pPlayer);
		if(HouseID < 0)
		{
			GS()->Chat(ClientID, "You not owner home!");
			return true;
		}

		vec2 Position = GetPositionHouse(HouseID);
		const int HouseWorldID = CHouseData::ms_aHouse[HouseID].m_WorldID;
		if(!GS()->IsPlayerEqualWorld(ClientID, HouseWorldID))
		{
			pPlayer->GetTempData().m_TempTeleportPos = Position;
			pPlayer->ChangeWorld(HouseWorldID);
			return true;
		}
		pPlayer->GetCharacter()->ChangePosition(Position);
		return true;
	}

	if(PPSTR(CMD, "HSELL") == 0)
	{
		const int HouseID = VoteID;
		if(HouseID <= 0 || Get != 777)
		{
			GS()->Chat(ClientID, "A verification number was entered incorrectly.");
			return true;
		}

		SellHouse(HouseID);
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

		if(pPlayer->SpendCurrency(Get))
		{
			AddSafeDeposit(pPlayer, Get);
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

		TakeFromSafeDeposit(pPlayer, Get);
		GS()->StrongUpdateVotes(ClientID, MENU_HOUSE);
		return true;
	}

	if(PPSTR(CMD, "HOUSEDOOR") == 0)
	{
		if(ChangeStateDoor(VoteID))
		{
			GS()->StrongUpdateVotes(ClientID, MENU_HOUSE);
		}
		return true;
	}

	if(PPSTR(CMD, "DECOSTART") == 0)
	{
		const int HouseID = PlayerHouseID(pPlayer);
		const vec2 PositionHouse = GetPositionHouse(HouseID);
		if(HouseID <= 0 || distance(PositionHouse, pPlayer->GetCharacter()->m_Core.m_Pos) > 600)
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
		const int HouseID = PlayerHouseID(pPlayer);
		if(HouseID > 0 && DeleteDecorationHouse(VoteID))
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
		const int HouseID = PlayerHouseID(pPlayer);
		if(HouseID < 0)
		{
			GS()->Chat(ClientID, "You not owner home!");
			return true;
		}

		if(GetPlantsID(HouseID) <= 0)
		{
			GS()->Chat(ClientID, "Your home does not support plants!");
			return true;
		}

		const int ItemID = VoteID;
		if(!pPlayer->SpendCurrency(1, ItemID))
		{
			return true;
		}

		const int ChanceSuccesful = VoteID2;
		if(ChanceSuccesful != 0)
		{
			GS()->Chat(ClientID, "Unfortunately plant did not take root!");
			GS()->UpdateVotes(ClientID, pPlayer->m_OpenVoteMenu);
			return true;
		}

		GS()->Chat(-1, "Congratulations {STR}, planted at home {STR}!", Server()->ClientName(ClientID), GS()->GetItemInfo(ItemID)->GetName());
		ChangePlantsID(HouseID, ItemID);
		GS()->UpdateVotes(ClientID, pPlayer->m_OpenVoteMenu);
	}

	return false;
}

/* #########################################################################
	FUNCTIONS HOUSES PLANTS
######################################################################### */
void CHouseCore::ChangePlantsID(int HouseID, int PlantID)
{
	if(CHouseData::ms_aHouse.find(HouseID) == CHouseData::ms_aHouse.end())
	{
		return;
	}

	bool Updates = false;
	for(CJobItems* pPlant = (CJobItems*)GS()->m_World.FindFirst(CGameWorld::ENTTYPE_JOBITEMS); pPlant; pPlant = (CJobItems*)pPlant->TypeNext())
	{
		if(pPlant->m_HouseID != HouseID)
		{
			continue;
		}

		pPlant->m_ItemID = PlantID;
		Updates = true;
	}

	if(Updates)
	{
		CHouseData::ms_aHouse[HouseID].m_PlantID = PlantID;
		Sqlpool.Execute<DB::UPDATE>("tw_houses", "PlantID = '%d' WHERE ID = '%d'", PlantID, HouseID);
	}
}

/* #########################################################################
	FUNCTIONS HOUSES DECORATION
######################################################################### */
bool CHouseCore::AddDecorationHouse(int DecoID, int HouseID, vec2 Position)
{
	if(HouseID <= 0)
	{
		return false;
	}

	vec2 PositionHouse = GetPositionHouse(HouseID);
	if(distance(PositionHouse, Position) > 400.0f)
	{
		return false;
	}

	ResultPtr pRes = Sqlpool.Execute<DB::SELECT>("ID", "tw_houses_decorations", "WHERE HouseID = '%d'", HouseID);
	if(static_cast<int>(pRes->rowsCount()) >= g_Config.m_SvLimitDecoration)
	{
		return false;
	}

	ResultPtr pRes2 = Sqlpool.Execute<DB::SELECT>("ID", "tw_houses_decorations", "ORDER BY ID DESC LIMIT 1");
	const int InitID = pRes2->next() ? pRes2->getInt("ID")+1 : 1;

	Sqlpool.Execute<DB::INSERT>("tw_houses_decorations", "(ID, DecoID, HouseID, PosX, PosY, WorldID) VALUES ('%d', '%d', '%d', '%d', '%d', '%d')",
		InitID, DecoID, HouseID, static_cast<int>(Position.x), static_cast<int>(Position.y), GS()->GetWorldID());

	m_aDecorationHouse[InitID] = new CDecorationHouses(&GS()->m_World, Position, HouseID, DecoID);
	return true;
}

bool CHouseCore::DeleteDecorationHouse(int ID)
{
	if(m_aDecorationHouse.find(ID) != m_aDecorationHouse.end())
	{
		if(m_aDecorationHouse[ID])
		{
			delete m_aDecorationHouse[ID];
			m_aDecorationHouse[ID] = nullptr;
		}
		m_aDecorationHouse.erase(ID);
		Sqlpool.Execute<DB::REMOVE>("tw_houses_decorations", "WHERE ID = '%d'", ID);
		return true;
	}
	return false;
}

void CHouseCore::ShowDecorationList(CPlayer *pPlayer)
{
	const int HouseID = PlayerHouseID(pPlayer);
	const int ClientID = pPlayer->GetCID();
	for (auto deco = m_aDecorationHouse.begin(); deco != m_aDecorationHouse.end(); ++deco)
	{
		if(deco->second && deco->second->m_HouseID == HouseID)
		{
			GS()->AVD(ClientID, "DECODELETE", deco->first, deco->second->m_DecoID, 1, "{STR}:{INT} back to the inventory",
				GS()->GetItemInfo(deco->second->m_DecoID)->GetName(), deco->first);
		}
	}
}

/* #########################################################################
	MENUS HOUSES
######################################################################### */
void CHouseCore::ShowHouseMenu(CPlayer* pPlayer, int HouseID)
{
	const int ClientID = pPlayer->GetCID();
	GS()->AVH(ClientID, TAB_INFO_HOUSE, "House {INT} . {STR}", HouseID, CHouseData::ms_aHouse[HouseID].m_aClass);
	GS()->AVM(ClientID, "null", NOPE, TAB_INFO_HOUSE, "Owner House: {STR}", Job()->PlayerName(CHouseData::ms_aHouse[HouseID].m_UserID));

	GS()->AV(ClientID, "null");
	GS()->ShowVotesItemValueInformation(pPlayer, itGold);
	GS()->AV(ClientID, "null");

	if(CHouseData::ms_aHouse[HouseID].m_UserID <= 0)
	{
		GS()->AVM(ClientID, "BUYHOUSE", HouseID, NOPE, "Buy this house. Price {VAL}gold", CHouseData::ms_aHouse[HouseID].m_Price);
	}
	else
	{
		GS()->AVM(ClientID, "null", HouseID, NOPE, "This house has already been purchased!");
	}

	GS()->AV(ClientID, "null");
}

void CHouseCore::ShowPersonalHouse(CPlayer* pPlayer)
{
	const int ClientID = pPlayer->GetCID();
	const int HouseID = PlayerHouseID(pPlayer);
	if(HouseID <= 0)
	{
		GS()->AVL(ClientID, "null", "You not owner home!");
		return;
	}

	const bool StateDoor = GetHouseDoor(HouseID);
	GS()->AVH(ClientID, TAB_HOUSE_STAT, "House stats {INT} Class {STR} Door [{STR}]", HouseID, CHouseData::ms_aHouse[HouseID].m_aClass, StateDoor ? "Closed" : "Opened");
	GS()->AVM(ClientID, "null", NOPE, TAB_HOUSE_STAT, "/doorhouse - interactive with door.");
	GS()->AVM(ClientID, "null", NOPE, TAB_HOUSE_STAT, "- - - - - - - - - -");
	GS()->AVM(ClientID, "null", NOPE, TAB_HOUSE_STAT, "Notes: Minimal operation house balance 100gold");
	GS()->AVM(ClientID, "null", NOPE, TAB_HOUSE_STAT, "In your safe is: {VAL}gold", CHouseData::ms_aHouse[HouseID].m_Bank);
	GS()->AV(ClientID, "null");
	//
	GS()->AVL(ClientID, "null", "◍ Your gold: {VAL}gold", pPlayer->GetItem(itGold)->GetValue());
	GS()->AVM(ClientID, "HOUSEADD", 1, NOPE, "Add to the safe gold. (Amount in a reason)");
	GS()->AVM(ClientID, "HOUSETAKE", 1, NOPE, "Take the safe gold. (Amount in a reason)");
	GS()->AV(ClientID, "null");
	//
	GS()->AVL(ClientID, "null", "▤ House system");
	GS()->AVM(ClientID, "HOUSEDOOR", HouseID, NOPE, "Change state to [\"{STR}\"]", StateDoor ? "OPEN" : "CLOSED");
	GS()->AVM(ClientID, "HSPAWN", 1, NOPE, "Teleport to your house");
	GS()->AVM(ClientID, "HSELL", HouseID, NOPE, "Sell your house (in reason 777)");
	if(GS()->IsPlayerEqualWorld(ClientID, CHouseData::ms_aHouse[HouseID].m_WorldID))
	{
		GS()->AVM(ClientID, "MENU", MENU_HOUSE_DECORATION, NOPE, "Settings Decorations");
		GS()->AVM(ClientID, "MENU", MENU_HOUSE_PLANTS, NOPE, "Settings Plants");
	}
	else
	{
		GS()->AVM(ClientID, "null", MENU_HOUSE_DECORATION, NOPE, "More settings allow, only on house zone");
	}
}

/* #########################################################################
	GET CHECK HOUSES
######################################################################### */
bool CHouseCore::IsHouseHasOwner(int HouseID) const
{
	if(CHouseData::ms_aHouse.find(HouseID) != CHouseData::ms_aHouse.end())
	{
		return CHouseData::ms_aHouse[HouseID].m_UserID >= 1;
	}
	return false;
}

int CHouseCore::GetHouse(vec2 Pos, bool Plants) const
{
	float Distance = Plants ? 300.0f : 128.0f;
	for(const auto& ihome : CHouseData::ms_aHouse)
	{
		if (ihome.second.m_WorldID != GS()->GetWorldID())
		{
			continue;
		}

		vec2 PosHome = Plants ? vec2(ihome.second.m_PlantPosX, ihome.second.m_PlantPosY) : vec2(ihome.second.m_PosX, ihome.second.m_PosY);
		if(distance(PosHome, Pos) < Distance)
		{
			return ihome.first;
		}
	}
	return -1;
}

int CHouseCore::GetHousePrice(int HouseID) const
{
	if(CHouseData::ms_aHouse.find(HouseID) != CHouseData::ms_aHouse.end())
	{
		return CHouseData::ms_aHouse.at(HouseID).m_Price;
	}
	return -1;
}

bool CHouseCore::GetHouseDoor(int HouseID) const
{
	if(CHouseData::ms_aHouse.find(HouseID) != CHouseData::ms_aHouse.end())
	{
		bool DoorClose = CHouseData::ms_aHouse.at(HouseID).m_Door;
		return DoorClose;
	}
	return false;
}

vec2 CHouseCore::GetPositionHouse(int HouseID) const
{
	if(CHouseData::ms_aHouse.find(HouseID) != CHouseData::ms_aHouse.end())
	{
		return vec2(CHouseData::ms_aHouse.at(HouseID).m_PosX, CHouseData::ms_aHouse.at(HouseID).m_PosY);
	}

	return vec2(0, 0);
}

int CHouseCore::PlayerHouseID(CPlayer *pPlayer) const
{
	for (auto ihome = CHouseData::ms_aHouse.begin(); ihome != CHouseData::ms_aHouse.end(); ++ihome) {
		if(CHouseData::ms_aHouse.at(ihome->first).m_UserID == pPlayer->Acc().m_UserID)
		{
			return ihome->first;
		}
	}
	return -1;
}

int CHouseCore::OwnerHouseID(int UserID) const
{
	for (auto ihome = CHouseData::ms_aHouse.begin(); ihome != CHouseData::ms_aHouse.end(); ++ihome) {
		if(CHouseData::ms_aHouse.at(ihome->first).m_UserID == UserID)
		{
			return ihome->first;
		}
	}
	return -1;
}

int CHouseCore::GetPlantsID(int HouseID) const
{
	if(CHouseData::ms_aHouse.find(HouseID) != CHouseData::ms_aHouse.end())
	{
		return CHouseData::ms_aHouse.at(HouseID).m_PlantID;
	}
	return -1;
}

const char *CHouseCore::ClassName(int HouseID) const
{
	if(CHouseData::ms_aHouse.find(HouseID) != CHouseData::ms_aHouse.end())
	{
		return CHouseData::ms_aHouse.at(HouseID).m_aClass;
	}
	return "None";
}

const char *CHouseCore::OwnerName(int HouseID)
{
	if(CHouseData::ms_aHouse.find(HouseID) != CHouseData::ms_aHouse.end())
	{
		return Job()->PlayerName(CHouseData::ms_aHouse[HouseID].m_UserID);
	}
	return "No owner";
}

/* #########################################################################
	FUNCTIONS HOUSES
######################################################################### */
void CHouseCore::BuyHouse(int HouseID, CPlayer *pPlayer)
{
	const int ClientID = pPlayer->GetCID();
	if(PlayerHouseID(pPlayer) > 0)
	{
		GS()->Chat(ClientID, "You already have a home.");
		return;
	}

	ResultPtr pRes = Sqlpool.Execute<DB::SELECT>("UserID, Price", "tw_houses", "WHERE ID = '%d' AND UserID IS NULL", HouseID);
	if(pRes->next())
	{
		const int Price = pRes->getInt("Price");
		if(!pPlayer->SpendCurrency(Price))
		{
			return;
		}

		CHouseData::ms_aHouse[HouseID].m_Bank = 0;
		CHouseData::ms_aHouse[HouseID].m_UserID = pPlayer->Acc().m_UserID;
		Sqlpool.Execute<DB::UPDATE>("tw_houses", "UserID = '%d', HouseBank = '0' WHERE ID = '%d'", CHouseData::ms_aHouse[HouseID].m_UserID, HouseID);

		GS()->Chat(-1, "{STR} becomes the owner of the house class {STR}", Server()->ClientName(ClientID), CHouseData::ms_aHouse[HouseID].m_aClass);
		GS()->ChatDiscord(DC_SERVER_INFO, "Server information", "**{STR} becomes the owner of the house class {STR}**", Server()->ClientName(ClientID), CHouseData::ms_aHouse[HouseID].m_aClass);
		GS()->UpdateVotes(ClientID, pPlayer->m_OpenVoteMenu);
		return;
	}

	GS()->Chat(ClientID, "House has already been purchased!");
}

void CHouseCore::SellHouse(int HouseID)
{
	ResultPtr pRes = Sqlpool.Execute<DB::SELECT>("HouseBank, UserID", "tw_houses", "WHERE ID = '%d' AND UserID IS NOT NULL", HouseID);
	if(pRes->next())
	{
		const int UserID = pRes->getInt("UserID");
		const int Price = CHouseData::ms_aHouse[HouseID].m_Price + pRes->getInt("HouseBank");
		GS()->SendInbox("System", UserID, "House is sold", "Your house is sold !", itGold, Price, 0);
		Sqlpool.Execute<DB::UPDATE>("tw_houses", "UserID = NULL, HouseBank = '0' WHERE ID = '%d'", HouseID);

		CPlayer *pPlayer = GS()->GetPlayerFromUserID(UserID);
		if(pPlayer)
		{
			GS()->Chat(pPlayer->GetCID(), "Your House is sold!");
			GS()->UpdateVotes(pPlayer->GetCID(), MENU_MAIN);
		}
		GS()->Chat(-1, "House: {INT} have been is released!", HouseID);
		GS()->ChatDiscord(DC_SERVER_INFO, "Server information", "**[House: {INT}] have been sold!**", HouseID);
	}

	if(CHouseData::ms_aHouse[HouseID].m_Door)
	{
		delete CHouseData::ms_aHouse[HouseID].m_Door;
		CHouseData::ms_aHouse[HouseID].m_Door = nullptr;
	}

	CHouseData::ms_aHouse[HouseID].m_UserID = -1;
	CHouseData::ms_aHouse[HouseID].m_Bank = 0;
}

void CHouseCore::TakeFromSafeDeposit(CPlayer* pPlayer, int TakeValue)
{
	const int ClientID = pPlayer->GetCID();
	ResultPtr pRes = Sqlpool.Execute<DB::SELECT>("ID, HouseBank", "tw_houses", "WHERE UserID = '%d'", pPlayer->Acc().m_UserID);
	if(!pRes->next())
	{
		return;
	}

	const int HouseID = pRes->getInt("ID");
	const int Bank = pRes->getInt("HouseBank");
	if(Bank < TakeValue)
	{
		GS()->Chat(ClientID, "Acceptable for take {VAL}gold", Bank);
		return;
	}

	pPlayer->AddMoney(TakeValue);
	CHouseData::ms_aHouse[HouseID].m_Bank = Bank - TakeValue;
	Sqlpool.Execute<DB::UPDATE>("tw_houses", "HouseBank = '%d' WHERE ID = '%d'", CHouseData::ms_aHouse[HouseID].m_Bank, HouseID);
	GS()->Chat(ClientID, "You take {VAL} gold in the safe {VAL}!", TakeValue, CHouseData::ms_aHouse[HouseID].m_Bank);
}

void CHouseCore::AddSafeDeposit(CPlayer *pPlayer, int Balance)
{
	const int ClientID = pPlayer->GetCID();
	ResultPtr pRes = Sqlpool.Execute<DB::SELECT>("ID, HouseBank", "tw_houses", "WHERE UserID = '%d'", pPlayer->Acc().m_UserID);
	if(!pRes->next())
	{
		return;
	}

	const int HouseID = pRes->getInt("ID");
	CHouseData::ms_aHouse[HouseID].m_Bank = pRes->getInt("HouseBank") + Balance;
	GS()->Chat(ClientID, "You put {VAL} gold in the safe {VAL}!", Balance, CHouseData::ms_aHouse[HouseID].m_Bank);
	Sqlpool.Execute<DB::UPDATE>("tw_houses", "HouseBank = '%d' WHERE ID = '%d'", CHouseData::ms_aHouse[HouseID].m_Bank, HouseID);
}

bool CHouseCore::ChangeStateDoor(int HouseID)
{
	if(CHouseData::ms_aHouse.find(HouseID) == CHouseData::ms_aHouse.end() || CHouseData::ms_aHouse[HouseID].m_UserID <= 0)
	{
		return false;
	}

	if(CHouseData::ms_aHouse[HouseID].m_WorldID != GS()->GetWorldID())
	{
		GS()->ChatAccount(CHouseData::ms_aHouse[HouseID].m_UserID, "Change state door can only near your house.");
		return false;
	}

	if(CHouseData::ms_aHouse[HouseID].m_Door)
	{
		delete CHouseData::ms_aHouse[HouseID].m_Door;
		CHouseData::ms_aHouse[HouseID].m_Door = nullptr;
	}
	else
	{
		CHouseData::ms_aHouse[HouseID].m_Door = new HouseDoor(&GS()->m_World, vec2(CHouseData::ms_aHouse[HouseID].m_DoorX, CHouseData::ms_aHouse[HouseID].m_DoorY));
	}

	const bool StateDoor = CHouseData::ms_aHouse[HouseID].m_Door;
	GS()->ChatAccount(CHouseData::ms_aHouse[HouseID].m_UserID, "You {STR} the house.", StateDoor ? "closed" : "opened");
	return true;
}