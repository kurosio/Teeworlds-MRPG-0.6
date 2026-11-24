#include "duties_manager.h"
#include "duties_db.h"

#include <engine/storage.h>
#include <game/server/gamecontext.h>
#include <game/server/core/components/accounts/account_manager.h>
#include <game/server/worldmodes/dungeon/dungeon.h>
#include <game/server/worldmodes/rhythm/rhythm.h>

void CDutiesManager::OnPreInit()
{
	auto Dungeons = DutiesDb::ListDungeons();
	while(Dungeons.Next())
	{
		const int ID = Dungeons.GetInt("ID");
		auto WaitingDoorPos = vec2(Dungeons.GetInt("DoorX"), Dungeons.GetInt("DoorY"));
		auto WorldID = Dungeons.GetInt("WorldID");
		auto Scenario = Dungeons.GetJson("Scenario");
		auto TimeLimit = Dungeons.GetInt("TimeLimit");

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
		if(const auto& BestPlayer = GetBestDungeonPlayer())
			VDungeon.Add("Best player: {~} with {} points.", BestPlayer->first, BestPlayer->second);
		else if(VDungeon.WaitLoad(BestDungeonLoad()))
			VDungeon.Add("Best player: loading ...");
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
		for(int i = 0; i < Server()->GetWorldsSize(); i++)
		{
			const auto* pDetail = Server()->GetWorldDetail(i);
			if(pDetail->GetType() == WorldType::MiniGames)
			{
				const auto ClientsNum = Server()->GetClientsCountByWorld(i);
				const char* pStatus = (ClientsNum > 0 ? "Active" : "Waiting");
				VMiniGames.AddMenu(MENU_MINIGAMES_SELECT, i, "Lv{}. {} : {}", pDetail->GetRequiredLevel(), Server()->GetWorldName(i), pStatus);
			}
		}
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
		if(const auto& BestPlayer = GetBestRhythmPlayer())
			VRhythm.Add("Best player: {~} with {} points.", BestPlayer->first, BestPlayer->second);
		else if(VRhythm.WaitLoad(BestRhythmLoad()))
			VRhythm.Add("Best player: loading ...");
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

	// minigames select
	if(Menulist == MENU_MINIGAMES_SELECT)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_DUTIES_LIST);

		if(const auto WorldID = pPlayer->m_VotesData.GetExtraID())
			ShowMiniGamesInfo(pPlayer, WorldID.value());

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

	// minigames enter
	if(PPSTR(pCmd, "MINIGAMES_JOIN") == 0)
	{
		auto WorldIdOpt = GetIfExists<int>(Extras, 0);
		if(!WorldIdOpt.has_value() || !Server()->IsWorldType(WorldIdOpt.value(), WorldType::MiniGames))
			return true;

		// check equal player world
		if(GS()->IsPlayerInWorld(ClientID, WorldIdOpt))
		{
			GS()->Chat(ClientID, "You are already in this mini-game.");
			pPlayer->m_VotesData.UpdateVotesIf(MENU_DUTIES_LIST);
			return true;
		}

		// check valid level
		if(pPlayer->Account()->GetLevel() < Server()->GetWorldDetail(WorldIdOpt.value())->GetRequiredLevel())
		{
			GS()->Chat(ClientID, "Your level is too low to join this mini-game.");
			pPlayer->m_VotesData.UpdateVotesIf(MENU_DUTIES_LIST);
			return true;
		}

		GS()->Chat(-1, "'{~}' joined mini-game '{}'!", Server()->ClientName(ClientID), Server()->GetWorldName(WorldIdOpt.value()));
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
	const auto& vTopRecords = Core()->GetDungeonTopList(pDungeon->GetID(), 5);
	if(vTopRecords.empty())
	{
		if(VRecords.WaitLoad(CMmoController::DungeonLoad(pDungeon->GetID(), 5)))
			VRecords.Add("Loading ...");
		else
			VRecords.Add("No records yet.");
	}
	else for(const auto& [Pos, RowData] : vTopRecords)
	{
		char aBuf[128];
		auto Time = RowData.m_Value.to_int();
		str_time(Time, TIME_MINS, aBuf, sizeof(aBuf));
		VRecords.Add("{}. {~} - {}.", Pos, RowData.m_Name, aBuf);
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
	else if(GS()->IsWorldType(WorldType::MiniGames))
	{
		VoteWrapper(ClientID).AddOption("DUTIES_EXIT", "Exit from Mini-game");
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
	const auto& vDifficulties = GetRhythmDifficulties(WorldID);
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
		const auto& vTopRecords = Core()->GetRhythmTopList(WorldID, Difficulty, 3);
		if(vTopRecords.empty())
		{
			if(VOptions.WaitLoad(CMmoController::RhythmLoad(WorldID, Difficulty, 3)))
				VOptions.Add("Loading ...");
			else
				VOptions.Add("No records yet.");
		}
		else
		{
			for(const auto& [Pos, RowData] : vTopRecords)
				VOptions.Add("{}. {~} - {} score.", Pos, RowData.m_Name, RowData.m_Value.to_int());
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

void CDutiesManager::ShowMiniGamesInfo(CPlayer* pPlayer, int WorldID) const
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
	VoteWrapper::AddEmptyline(ClientID);

	// options
	VoteWrapper VOptions(ClientID);
	VOptions.AddOption("MINIGAMES_JOIN", WorldID, "Join mini-game");
	VoteWrapper::AddEmptyline(ClientID);
}

const std::vector<std::string>& CDutiesManager::GetRhythmDifficulties(int WorldID) const
{
	// read once per world and kept: the files do not change while the server runs
	if(const auto it = m_DifficultiesByWorld.find(WorldID); it != m_DifficultiesByWorld.end())
		return it->second;

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

	return m_DifficultiesByWorld.emplace(WorldID, std::move(vDifficulties)).first->second;
}

CVoteLoad CDutiesManager::BestDungeonLoad()
{
	return CVoteLoad("best_dungeon");
}

CVoteLoad CDutiesManager::BestRhythmLoad()
{
	return CVoteLoad("best_rhythm");
}

bool CDutiesManager::NeedBest(const SBestPlayer& Cache, const CVoteLoad& Load) const
{
	// the value stays valid for a while, so opening the menu again does not query again
	constexpr int LifetimeSeconds = 20;
	if(Cache.m_Loaded && GS()->Server()->Tick() - Cache.m_Stamp < LifetimeSeconds * GS()->Server()->TickSpeed())
		return false;

	return Load.Begin();
}

void CDutiesManager::SetBest(SBestPlayer& Cache, std::optional<std::pair<std::string, int>>&& Value, const CVoteLoad& Load) const
{
	Cache.m_Value = std::move(Value);
	Cache.m_Stamp = GS()->Server()->Tick();
	Cache.m_Loaded = true;

	// the menus that showed the waiting state are drawn again with the value
	Load.Finish();
}

const std::optional<std::pair<std::string, int>>& CDutiesManager::GetBestDungeonPlayer() const
{
	const CVoteLoad Load = BestDungeonLoad();
	if(NeedBest(m_BestDungeon, Load))
	{
		DutiesDb::ReadBestDungeonPlayer()
			.Then([this, Load](Db::CResultSet&& Best)
		{
			std::optional<std::pair<std::string, int>> Value {};
			if(Best.HasRows())
				Value = std::make_pair(std::string(Instance::Server()->GetAccountNickname(Best.GetInt("UserID"))), Best.GetInt("Points"));
			SetBest(m_BestDungeon, std::move(Value), Load);
		});
	}

	return m_BestDungeon.m_Value;
}

const std::optional<std::pair<std::string, int>>& CDutiesManager::GetBestRhythmPlayer() const
{
	const CVoteLoad Load = BestRhythmLoad();
	if(NeedBest(m_BestRhythm, Load))
	{
		DutiesDb::ReadBestRhythmPlayer()
			.Then([this, Load](Db::CResultSet&& Best)
		{
			std::optional<std::pair<std::string, int>> Value {};
			if(Best.HasRows())
				Value = std::make_pair(std::string(Instance::Server()->GetAccountNickname(Best.GetInt("UserID"))), Best.GetInt("Points"));
			SetBest(m_BestRhythm, std::move(Value), Load);
		});
	}

	return m_BestRhythm.m_Value;
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
