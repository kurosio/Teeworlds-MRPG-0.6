#include "duties_manager.h"

#include <engine/storage.h>
#include <game/server/gamecontext.h>
#include <game/server/core/components/accounts/account_manager.h>
#include <game/server/worldmodes/dungeon/dungeon.h>
#include <game/server/worldmodes/rhythm/rhythm.h>

void CDutiesManager::OnPreInit()
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_dungeons");
	while(pRes->next())
	{
		const int ID = pRes->getInt("ID");
		auto WaitingDoorPos = vec2(pRes->getInt("DoorX"), pRes->getInt("DoorY"));
		auto WorldID = pRes->getInt("WorldID");
		auto Scenario = pRes->getJson("Scenario");
		auto TimeLimit = pRes->getInt("TimeLimit");

		auto* pDungeon = CDungeonData::CreateElement(ID);
		pDungeon->Init(WaitingDoorPos, WorldID, Scenario, TimeLimit);
	}
}


bool CDutiesManager::OnSendMenuVotes(CPlayer* pPlayer, int Menulist)
{
	const int ClientID = pPlayer->GetCID();

	if(Menulist == EMenuList::MENU_DUTIES_LIST)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);

		// inside menu
		ShowInsideMenu(pPlayer);

		// dungeons
		VoteWrapper VDungeon(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_SIMPLE, "\u262C Dungeons ({})", GetWorldsCountByType(WorldType::Dungeon));
		if(const auto BestPlayer = GetBestDungeonPlayer())
			VDungeon.Add("Best player: {~} with {} points.", BestPlayer->first, BestPlayer->second);
		else
			VDungeon.Add("Best player: not found yet.");
		for(const auto* pDungeon : CDungeonData::Data())
		{
			const char* pStatus = (pDungeon->IsPlaying() ? "Active" : "Waiting");
			VDungeon.AddMenu(MENU_DUNGEON_SELECT, pDungeon->GetID(), "Lv{}. {} : {}", pDungeon->GetLevel(), pDungeon->GetName(), pStatus);
		}
		VoteWrapper::AddEmptyline(ClientID);

		// pvp
		VoteWrapper VPvP(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_SIMPLE, "\u2694 PvP ({})", GetWorldsCountByType(WorldType::PvP));
		for(int i = 0; i < Server()->GetWorldsSize(); i++)
		{
			const auto* pDetail = Server()->GetWorldDetail(i);
			if(pDetail->GetType() == WorldType::PvP)
			{
				const auto ClientsNum = Server()->GetClientsCountByWorld(i);
				const char* pStatus = (ClientsNum > 0 ? "Active" : "Waiting");
				VPvP.AddMenu(MENU_PVP_SELECT, i, "Lv{}. {} : {}", pDetail->GetRequiredLevel(), Server()->GetWorldName(i), pStatus);
			}
		}
		VoteWrapper::AddEmptyline(ClientID);

		// Mini-games
		VoteWrapper VMiniGames(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_SIMPLE, "\u266C Mini-games ({})", GetWorldsCountByType(WorldType::MiniGames));
		VoteWrapper::AddEmptyline(ClientID);

		// Social worlds
		VoteWrapper VSocial(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_SIMPLE, "\u260E Social ({})", GetWorldsCountByType(WorldType::Social));
		VSocial.Add("Churches, casinos and other social locations.");
		for(int i = 0; i < Server()->GetWorldsSize(); i++)
		{
			const auto* pDetail = Server()->GetWorldDetail(i);
			if(pDetail->GetType() == WorldType::Social)
			{
				const auto ClientsNum = Server()->GetClientsCountByWorld(i);
				VSocial.AddMenu(MENU_SOCIAL_SELECT, i, "Lv{}. {} : {} online", pDetail->GetRequiredLevel(), Server()->GetWorldName(i), ClientsNum);
			}
		}
		VoteWrapper::AddEmptyline(ClientID);

		// Rhythm
		VoteWrapper VRhythm(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_SIMPLE, "\u266C Rhythm ({})", GetWorldsCountByType(WorldType::Rhythm));
		if(const auto BestPlayer = GetBestRhythmPlayer())
			VRhythm.Add("Best player: {~} with {} points.", BestPlayer->first, BestPlayer->second);
		else
			VRhythm.Add("Best player: not found yet.");
		for(int i = 0; i < Server()->GetWorldsSize(); i++)
		{
			const auto* pDetail = Server()->GetWorldDetail(i);
			if(pDetail->GetType() == WorldType::Rhythm)
			{
				const auto ClientsNum = Server()->GetClientsCountByWorld(i);
				const char* pStatus = (ClientsNum > 0 ? "Active" : "Waiting");
				VRhythm.AddMenu(MENU_RHYTHM_SELECT, i, "Lv{}. {} : {}", pDetail->GetRequiredLevel(), Server()->GetWorldName(i), pStatus);
			}
		}
		VoteWrapper::AddEmptyline(ClientID);

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

	// rhythm select
	if(Menulist == MENU_RHYTHM_SELECT)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_DUTIES_LIST);

		if(const auto WorldID = pPlayer->m_VotesData.GetExtraID())
			ShowRhythmInfo(pPlayer, WorldID.value());

		// add backpage
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// social select
	if(Menulist == MENU_SOCIAL_SELECT)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_DUTIES_LIST);

		if(const auto WorldID = pPlayer->m_VotesData.GetExtraID())
			ShowSocialInfo(pPlayer, WorldID.value());

		// add backpage
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	return false;
}


bool CDutiesManager::OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, const std::vector<std::any> &Extras, int ReasonNumber, const char* pReason)
{
	if(!pPlayer->GetCharacter() || !pPlayer->GetCharacter()->IsAlive())
		return false;

	const int ClientID = pPlayer->GetCID();

	// dungeon enter
	if(PPSTR(pCmd, "DUNGEON_JOIN") == 0)
	{
		// check valid dungeon
        const int DungeonID = GetIfExists<int>(Extras, 0, NOPE);
		auto* pDungeon = GetDungeonByID(DungeonID);
		if(!pDungeon)
			return true;

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
		if(pPlayer->Account()->GetLevel() < Server()->GetWorldDetail(pDungeon->GetWorldID())->GetRequiredLevel())
		{
			GS()->Chat(ClientID, "Your level is low to pass this dungeon.");
			pPlayer->m_VotesData.UpdateVotesIf(MENU_DUTIES_LIST);
			return true;
		}

		// information and join
		GS()->Chat(-1, "'{~}' joined to Dungeon '{}'!", Server()->ClientName(ClientID), pDungeon->GetName());
		pPlayer->ChangeWorld(pDungeon->GetWorldID());
		return true;
	}

	// pvp enter
	if(PPSTR(pCmd, "PVP_JOIN") == 0)
	{
		// check equal player world
        auto WorldIdOpt = GetIfExists<int>(Extras, 0);
		if(GS()->IsPlayerInWorld(ClientID, WorldIdOpt))
		{
			GS()->Chat(ClientID, "You are already in this dungeon.");
			pPlayer->m_VotesData.UpdateVotesIf(MENU_DUTIES_LIST);
			return true;
		}

		// check valid level
		if(pPlayer->Account()->GetLevel() < Server()->GetWorldDetail(WorldIdOpt.value())->GetRequiredLevel())
		{
			GS()->Chat(ClientID, "Your level is low to pass.");
			pPlayer->m_VotesData.UpdateVotesIf(MENU_DUTIES_LIST);
			return true;
		}

        if(WorldIdOpt.has_value())
		    pPlayer->ChangeWorld(WorldIdOpt.value());
		return true;
	}

	// rhythm enter
	if(PPSTR(pCmd, "RHYTHM_JOIN") == 0)
	{
		auto WorldIdOpt = GetIfExists<int>(Extras, 0);
		const auto Difficulty = GetIfExists<std::string>(Extras, 1, std::string("Normal"));
		if(!WorldIdOpt.has_value())
			return true;

		// check equal player world
		if(GS()->IsPlayerInWorld(ClientID, WorldIdOpt))
		{
			GS()->Chat(ClientID, "You are already in this rhythm mode.");
			pPlayer->m_VotesData.UpdateVotesIf(MENU_DUTIES_LIST);
			return true;
		}

		// check is started
		auto* pWorldGS = dynamic_cast<CGS*>(Server()->GameServer(WorldIdOpt.value()));
		if(pWorldGS && pWorldGS->IsDutyStarted())
		{
			GS()->Chat(ClientID, "Rhythm round already active. Wait for the next one.");
			pPlayer->m_VotesData.UpdateVotesIf(MENU_DUTIES_LIST);
			return true;
		}

		// check is locked for now difficulty
		const int ClientsNum = Server()->GetClientsCountByWorld(WorldIdOpt.value());
		const std::string LockedDifficulty = CGameControllerRhythm::GetSelectedDifficultyForWorld(WorldIdOpt.value());
		if(ClientsNum > 0 && Difficulty != LockedDifficulty)
		{
			GS()->Chat(ClientID, "This rhythm room is locked to '{~}' difficulty now.", LockedDifficulty);
			pPlayer->m_VotesData.UpdateVotesIf(MENU_DUTIES_LIST);
			return true;
		}

		// check valid level
		if(pPlayer->Account()->GetLevel() < Server()->GetWorldDetail(WorldIdOpt.value())->GetRequiredLevel())
		{
			GS()->Chat(ClientID, "Your level is low to pass.");
			pPlayer->m_VotesData.UpdateVotesIf(MENU_DUTIES_LIST);
			return true;
		}

		// join
		CGameControllerRhythm::SetSelectedDifficultyForWorld(WorldIdOpt.value(), Difficulty);
		if(pWorldGS)
		{
			if(auto* pRhythmController = dynamic_cast<CGameControllerRhythm*>(pWorldGS->m_pController))
				pRhythmController->ApplyDifficulty(Difficulty);
		}
		GS()->Chat(-1, "'{~}' joined to '{}' [{~}]!", Server()->ClientName(ClientID), Server()->GetWorldName(WorldIdOpt.value()), Difficulty);
		pPlayer->ChangeWorld(WorldIdOpt.value());
		return true;
	}

	// social enter
	if(PPSTR(pCmd, "SOCIAL_JOIN") == 0)
	{
		auto WorldIdOpt = GetIfExists<int>(Extras, 0);
		if(!WorldIdOpt.has_value() || !Server()->IsWorldType(WorldIdOpt.value(), WorldType::Social))
			return true;

		// check equal player world
		if(GS()->IsPlayerInWorld(ClientID, WorldIdOpt))
		{
			GS()->Chat(ClientID, "You are already in this social world.");
			pPlayer->m_VotesData.UpdateVotesIf(MENU_DUTIES_LIST);
			return true;
		}

		// check valid level
		if(pPlayer->Account()->GetLevel() < Server()->GetWorldDetail(WorldIdOpt.value())->GetRequiredLevel())
		{
			GS()->Chat(ClientID, "Your level is low to visit this social world.");
			pPlayer->m_VotesData.UpdateVotesIf(MENU_DUTIES_LIST);
			return true;
		}

		GS()->Chat(-1, "'{~}' joined to social world '{}'!", Server()->ClientName(ClientID), Server()->GetWorldName(WorldIdOpt.value()));
		pPlayer->ChangeWorld(WorldIdOpt.value());
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
	VoteWrapper::AddEmptyline(ClientID);

	// options
	VoteWrapper VOptions(ClientID);
	VOptions.AddOption("DUNGEON_JOIN", pDungeon->GetID(), "Join to the dungeon");
	VoteWrapper::AddEmptyline(ClientID);

	// records
	VoteWrapper VRecords(ClientID, VWF_SEPARATE_OPEN| VWF_STYLE_SIMPLE, "Records of passing");
	auto vTopRecords = Core()->GetDungeonTopList(pDungeon->GetID(), 5);
	if(vTopRecords.empty())
	{
		VRecords.Add("No records yet.");
	}
	else for(auto& [Pos, RowData] : vTopRecords)
	{
		char aBuf[128];
		auto Time = RowData.Data["Time"].to_int();
		str_time(Time, TIME_MINS, aBuf, sizeof(aBuf));
		VRecords.Add("{}. {~} - {}.", Pos, RowData.Name, aBuf);
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
	else if(GS()->IsWorldType(WorldType::Rhythm))
	{
		VoteWrapper(ClientID).AddOption("DUTIES_EXIT", "Exit from Rhythm");
		VoteWrapper::AddEmptyline(ClientID);
	}
	else if(GS()->IsWorldType(WorldType::Social))
	{
		VoteWrapper(ClientID).AddOption("DUTIES_EXIT", "Exit from Social");
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

void CDutiesManager::ShowRhythmInfo(CPlayer* pPlayer, int WorldID) const
{
	const auto ClientID = pPlayer->GetCID();
	const auto ClientsNum = Server()->GetClientsCountByWorld(WorldID);
	const char* pStatus = (ClientsNum > 0 ? "Active" : "Waiting");
	const char* pName = Server()->GetWorldName(WorldID);
	const auto vDifficulties = GetRhythmDifficulties(WorldID);
	const bool IsDifficultyLocked = ClientsNum > 0;
	const std::string LockedDifficulty = CGameControllerRhythm::GetSelectedDifficultyForWorld(WorldID);

	// info
	VoteWrapper VInfo(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_DOUBLE | VWF_ALIGN_TITLE, "\u2692 Detail information");
	VInfo.Add("Name: {}", pName);
	VInfo.Add("Status: {}", pStatus);
	VInfo.Add("Players in-game: {}", ClientsNum);
	VInfo.Add("Goal: hit notes on beat.");
	VInfo.Add("Controls: Left / Jump / Right.");
	VInfo.Add("Tip: perfect timing gives max score.");
	if(IsDifficultyLocked)
		VInfo.Add("Current difficulty lock: {}", LockedDifficulty);
	VoteWrapper::AddEmptyline(ClientID);

	// records by difficulty
	for(const auto& Difficulty : vDifficulties)
	{
		// add record list
		VoteWrapper VOptions(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_SIMPLE, "Rhythm [{~}]", Difficulty);
		auto vTopRecords = Core()->GetRhythmTopList(WorldID, Difficulty, 3);
		if(vTopRecords.empty())
			VOptions.Add("No records yet.");
		else
		{
			for(auto& [Pos, RowData] : vTopRecords)
				VOptions.Add("{}. {~} - {} score.", Pos, RowData.Name, RowData.Data["Score"].to_int());
		}

		// lock difficulty if another is active
		if(IsDifficultyLocked && Difficulty != LockedDifficulty)
			VOptions.Add("Join [{}] (locked now)", Difficulty);
		else
			VOptions.AddOption("RHYTHM_JOIN", MakeAnyList(WorldID, Difficulty), "Join [{}]", Difficulty);
		VoteWrapper::AddEmptyline(ClientID);
	}
}

void CDutiesManager::ShowSocialInfo(CPlayer* pPlayer, int WorldID) const
{
	const auto ClientID = pPlayer->GetCID();
	const auto ClientsNum = Server()->GetClientsCountByWorld(WorldID);
	const auto* pDetail = Server()->GetWorldDetail(WorldID);
	const char* pName = Server()->GetWorldName(WorldID);

	// info
	VoteWrapper VInfo(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_DOUBLE | VWF_ALIGN_TITLE, "\u2692 Detail information");
	VInfo.Add("Name: {}", pName);
	VInfo.Add("Recommended level: {} LVL", pDetail->GetRequiredLevel());
	VInfo.Add("Players in-game: {}", ClientsNum);
	VInfo.Add("Purpose: social location for meetings and roleplay.");
	VInfo.Add("Examples: church, casino and similar places.");
	VoteWrapper::AddEmptyline(ClientID);

	// options
	VoteWrapper VOptions(ClientID);
	VOptions.AddOption("SOCIAL_JOIN", WorldID, "Visit social world");
	VoteWrapper::AddEmptyline(ClientID);
}

std::vector<std::string> CDutiesManager::GetRhythmDifficulties(int WorldID) const
{
	const std::string_view MapName = Server()->GetWorldName(WorldID);
	struct SScanData
	{
		std::string_view m_MapName;
		std::vector<std::string> m_vDifficulties;
	};

	auto Callback = [](const char* pName, int IsDir, int, void* pUser) -> int
	{
		if(IsDir)
			return 0;

		auto& Data = *static_cast<SScanData*>(pUser);
		const std::string_view FileName(pName);
		const std::string Prefix = std::string(Data.m_MapName) + "[";
		if(!str_startswith(FileName.data(), Prefix.c_str()) || !str_endswith(FileName.data(), "].json"))
			return 0;

		const size_t Begin = Prefix.size();
		const size_t End = FileName.size() - std::string_view("].json").size();
		if(Begin >= End)
			return 0;

		Data.m_vDifficulties.emplace_back(FileName.substr(Begin, End - Begin));
		return 0;
	};

	SScanData Data;
	Data.m_MapName = MapName;
	GS()->Storage()->ListDirectory(IStorageEngine::TYPE_ALL, "maps/rhythm", Callback, &Data);

	auto& vDifficulties = Data.m_vDifficulties;
	std::sort(vDifficulties.begin(), vDifficulties.end());
	vDifficulties.erase(std::unique(vDifficulties.begin(), vDifficulties.end()), vDifficulties.end());

	if(vDifficulties.empty())
		vDifficulties.emplace_back("Default");

	return vDifficulties;
}

std::optional<std::pair<std::string, int>> CDutiesManager::GetBestDungeonPlayer() const
{
	ResultPtr pRes = Database->Execute<DB::SELECT>(
		"UserID, SUM(CASE WHEN Time > 0 THEN GREATEST(1, 10000 / Time) ELSE 10000 END) AS Points",
		"tw_dungeons_records", "GROUP BY UserID ORDER BY Points DESC LIMIT 1");
	if(!pRes->next())
		return std::nullopt;

	const int UserID = pRes->getInt("UserID");
	const int Points = pRes->getInt("Points");
	return std::make_pair(std::string(GS()->Server()->GetAccountNickname(UserID)), Points);
}

std::optional<std::pair<std::string, int>> CDutiesManager::GetBestRhythmPlayer() const
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("UserID, SUM(Score) AS Points", "tw_rhythm_records", "GROUP BY UserID ORDER BY Points DESC LIMIT 1");
	if(!pRes->next())
		return std::nullopt;

	const int UserID = pRes->getInt("UserID");
	const int Points = pRes->getInt("Points");
	return std::make_pair(std::string(GS()->Server()->GetAccountNickname(UserID)), Points);
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
