#include "duties_manager.h"

#include <game/server/gamecontext.h>
#include <game/server/core/components/accounts/account_manager.h>
#include <game/server/worldmodes/dungeon/dungeon.h>

void CDutiesManager::OnPreInit()
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_dungeons");
	while(pRes->next())
	{
		const int ID = pRes->getInt("ID");
		auto Level = pRes->getInt("Level");
		auto WaitingDoorPos = vec2(pRes->getInt("DoorX"), pRes->getInt("DoorY"));
		auto WorldID = pRes->getInt("WorldID");
		auto Scenario = pRes->getJson("Scenario");

		auto* pDungeon = CDungeonData::CreateElement(ID);
		pDungeon->Init(WaitingDoorPos, Level, WorldID, Scenario);
	}
}


bool CDutiesManager::OnSendMenuVotes(CPlayer* pPlayer, int Menulist)
{
	const int ClientID = pPlayer->GetCID();

	if(Menulist == EMenuList::MENU_DUTIES_LIST)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);

		// add selector
		VoteWrapper VSelectorType(ClientID, VWF_SEPARATE_OPEN|VWF_ALIGN_TITLE|VWF_STYLE_STRICT_BOLD, "\u261C Duties list");
		VSelectorType.AddMenu(MENU_DUTIES_LIST, (int)WorldType::MiniGames, "\u2659 Mini-games ({})", GetWorldsCountByType(WorldType::MiniGames));
		VSelectorType.AddMenu(MENU_DUTIES_LIST, (int)WorldType::Dungeon, "\u262C Dungeons ({})", GetWorldsCountByType(WorldType::Dungeon));
		VSelectorType.AddMenu(MENU_DUTIES_LIST, (int)WorldType::DeepDungeon, "\u262A Deep dungeons ({})", GetWorldsCountByType(WorldType::DeepDungeon));
		VSelectorType.AddMenu(MENU_DUTIES_LIST, (int)WorldType::TreasureDungeon, "\u2619 Treasure dungeons ({})", GetWorldsCountByType(WorldType::TreasureDungeon));
		VSelectorType.AddMenu(MENU_DUTIES_LIST, (int)WorldType::PvP, "\u2694 PvP ({})", GetWorldsCountByType(WorldType::PvP));
		VoteWrapper::AddEmptyline(ClientID);

		// inside menu
		ShowInsideMenu(pPlayer);

		// dungeon list
		if(const auto Type = pPlayer->m_VotesData.GetExtraID())
			ShowDutiesList(pPlayer, (WorldType)(*Type));

		// add backpage buttom
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// dungeon select
	if(Menulist == MENU_DUNGEON_SELECT)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_DUTIES_LIST);

		if(const auto DungeonID = pPlayer->m_VotesData.GetExtraID())
			ShowDungeonInfo(pPlayer, GetDungeonByID(DungeonID.value()));

		// add backpage
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// pvp select
	if(Menulist == MENU_PVP_SELECT)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_DUTIES_LIST);

		if(const auto WorldID = pPlayer->m_VotesData.GetExtraID())
			ShowPvpInfo(pPlayer, WorldID.value());

		// add backpage
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	return false;
}


bool CDutiesManager::OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, const int Extra1, const int Extra2, int ReasonNumber, const char* pReason)
{
	if(!pPlayer->GetCharacter() || !pPlayer->GetCharacter()->IsAlive())
		return false;

	const int ClientID = pPlayer->GetCID();

	// dungeon enter
	if(PPSTR(pCmd, "DUNGEON_JOIN") == 0)
	{
		// check valid dungeon
		auto* pDungeon = GetDungeonByID(Extra1);
		if(!pDungeon)
			return true;

		// avaialbe dungeon join
		if(!g_Config.m_SvAvailableDungeonJoin)
		{
			GS()->Chat(ClientID, "Dungeons disabled by server.");
			pPlayer->m_VotesData.UpdateVotesIf(MENU_DUTIES_LIST);
			return true;
		}

		// check equal player world
		if(GS()->IsPlayerInWorld(ClientID, pDungeon->GetWorldID()))
		{
			GS()->Chat(ClientID, "You are already in this dungeon.");
			pPlayer->m_VotesData.UpdateVotesIf(MENU_DUTIES_LIST);
			return true;
		}

		// check is playing
		if(pDungeon->IsPlaying())
		{
			GS()->Chat(ClientID, "At the moment players are passing this dungeon.");
			pPlayer->m_VotesData.UpdateVotesIf(MENU_DUTIES_LIST);
			return true;
		}

		// check valid level
		if(pPlayer->Account()->GetLevel() < pDungeon->GetLevel())
		{
			GS()->Chat(ClientID, "Your level is low to pass this dungeon.");
			pPlayer->m_VotesData.UpdateVotesIf(MENU_DUTIES_LIST);
			return true;
		}

		// information and join
		GS()->Chat(-1, "'{}' joined to Dungeon '{}'!", Server()->ClientName(ClientID), pDungeon->GetName());
		pPlayer->ChangeWorld(pDungeon->GetWorldID());
		return true;
	}

	// pvp enter
	if(PPSTR(pCmd, "PVP_JOIN") == 0)
	{
		// check equal player world
		if(GS()->IsPlayerInWorld(ClientID, Extra1))
		{
			GS()->Chat(ClientID, "You are already in this dungeon.");
			pPlayer->m_VotesData.UpdateVotesIf(MENU_DUTIES_LIST);
			return true;
		}

		pPlayer->ChangeWorld(Extra1);
		return true;
	}

	//dungeon exit
	if(PPSTR(pCmd, "DUTIES_EXIT") == 0)
	{
		const int LatestCorrectWorldID = Core()->AccountManager()->GetLastVisitedWorldID(pPlayer);
		pPlayer->ChangeWorld(LatestCorrectWorldID);
		return true;
	}

	return false;
}


void CDutiesManager::ShowDutiesList(CPlayer* pPlayer, WorldType Type) const
{
	const int ClientID = pPlayer->GetCID();

	// default dungeon type
	if(Type == WorldType::Dungeon)
	{
		VoteWrapper VDungeon(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_SIMPLE, "\u262C Dungeons");
		for(const auto* pDungeon : CDungeonData::Data())
		{
			const char* pStatus = (pDungeon->IsPlaying() ? "Active dungeon" : "Waiting players");
			VDungeon.AddMenu(MENU_DUNGEON_SELECT, pDungeon->GetID(), "Lv{}. {} : {}", pDungeon->GetLevel(), pDungeon->GetName(), pStatus);
		}
	}
	else if(Type == WorldType::PvP)
	{
		VoteWrapper VPvP(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_SIMPLE, "\u262C PvP");
		for(int i = 0; i < Server()->GetWorldsSize(); i++)
		{
			const auto* pDetail = Server()->GetWorldDetail(i);
			if(pDetail->GetType() == WorldType::PvP)
			{
				const auto ClientsNum = Server()->GetClientsCountByWorld(i);
				const char* pStatus = (ClientsNum > 0 ? "Active" : "Waiting");
				VPvP.AddMenu(MENU_PVP_SELECT, i, "{} : {}", Server()->GetWorldName(i), pStatus);
			}
		}
	}
}

void CDutiesManager::ShowDungeonInfo(CPlayer* pPlayer, CDungeonData* pDungeon) const
{
	if(!pDungeon)
		return;

	const auto ClientID = pPlayer->GetCID();
	const char* pStatus = (pDungeon->IsPlaying() ? "Active dungeon" : "Waiting players");

	// info
	VoteWrapper VInfo(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_DOUBLE | VWF_ALIGN_TITLE, "\u2692 Detail information");
	VInfo.Add("Name: {}", pDungeon->GetName());
	VInfo.Add("Recommended level: {} LVL", pDungeon->GetLevel());
	VInfo.Add("Recommended classes: ALL"); // TODO;
	VInfo.Add("Status: {}", pStatus);
	VInfo.Add("Players in-game: {}", pDungeon->GetPlayersNum());
	VInfo.Add("Current passing progress: {}", pDungeon->GetProgress());
	VoteWrapper::AddEmptyline(ClientID);

	// options
	VoteWrapper VOptions(ClientID);
	VOptions.AddOption("DUNGEON_JOIN", pDungeon->GetID(), "Join to the dungeon");
	VoteWrapper::AddEmptyline(ClientID);

	// records
	VoteWrapper VRecords(ClientID, VWF_SEPARATE_OPEN| VWF_STYLE_SIMPLE, "Records of passing");
	auto vTopRecords = Core()->GetDungeonTopList(pDungeon->GetID(), 5);
	for(auto& [Pos, RowData] : vTopRecords)
	{
		char aBuf[128];
		auto Time = RowData.Data["Time"].to_int();
		str_time(Time, TIME_MINS, aBuf, sizeof(aBuf));
		VRecords.Add("{}. {} - {}.", Pos, RowData.Name, aBuf);
	}
}

void CDutiesManager::ShowInsideMenu(CPlayer* pPlayer) const
{
	const int ClientID = pPlayer->GetCID();

	if(GS()->IsWorldType(WorldType::Dungeon))
	{
		const auto* pController = (CGameControllerDungeon*)GS()->m_pController;
		if(!pController || !pController->GetDungeon())
			return;

		const char* pName = pController->GetDungeon()->GetName();
		VoteWrapper(ClientID).AddOption("DUTIES_EXIT", "Exit dungeon {} (warning)", pName);
	}
	else if(GS()->IsWorldType(WorldType::PvP))
	{
		VoteWrapper(ClientID).AddOption("DUTIES_EXIT", "Exit from PvP");
		VoteWrapper::AddEmptyline(ClientID);
	}
}

void CDutiesManager::ShowPvpInfo(CPlayer* pPlayer, int WorldID) const
{
	const auto ClientID = pPlayer->GetCID();
	const auto ClientsNum = Server()->GetClientsCountByWorld(WorldID);
	const auto* pDetail = Server()->GetWorldDetail(WorldID);
	const char* pStatus = (ClientsNum > 0 ? "Active" : "Waiting");
	const char* pName = Server()->GetWorldName(WorldID);

	// info
	VoteWrapper VInfo(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_DOUBLE | VWF_ALIGN_TITLE, "\u2692 Detail information");
	VInfo.Add("Name: {}", pName);
	VInfo.Add("Recommended level: {} LVL", pDetail->GetRequiredLevel());
	VInfo.Add("Recommended classes: ALL");
	VInfo.Add("Status: {}", pStatus);
	VInfo.Add("Players in-game: {}", ClientsNum);
	VoteWrapper::AddEmptyline(ClientID);

	// options
	VoteWrapper VOptions(ClientID);
	VOptions.AddOption("PVP_JOIN", WorldID, "Join to the PvP");
	VoteWrapper::AddEmptyline(ClientID);

	// records
	VoteWrapper VRecords(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_SIMPLE, "Best PvP players");
	VRecords.Add("Coming...");
}


CDungeonData* CDutiesManager::GetDungeonByID(int DungeonID) const
{
	auto pDungeon = std::ranges::find_if(CDungeonData::Data(), [DungeonID](auto* pDungeon)
	{ return pDungeon->GetID() == DungeonID; });
	return pDungeon != CDungeonData::Data().end() ? *pDungeon : nullptr;
}


CDungeonData* CDutiesManager::GetDungeonByWorldID(int WorldID) const
{
	auto pDungeon = std::ranges::find_if(CDungeonData::Data(), [WorldID](auto* pDungeon)
	{ return pDungeon->GetWorldID() == WorldID; });
	return pDungeon != CDungeonData::Data().end() ? *pDungeon : nullptr;
}

int CDutiesManager::GetWorldsCountByType(WorldType Type) const
{
	int Result = 0;

	for(int i = 0; i < Server()->GetWorldsSize(); i++)
	{
		const auto* pDetail = Server()->GetWorldDetail(i);
		if(pDetail->GetType() == Type)
			Result++;
	}

	return Result;
}
