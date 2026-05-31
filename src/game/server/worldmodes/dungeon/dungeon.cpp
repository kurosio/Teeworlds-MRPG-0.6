/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "entities/waiting_door.h"
#include "dungeon.h"

#include <game/server/core/balance/balance.h>

#include <game/server/entity.h>
#include <game/server/gamecontext.h>
#include <game/server/entities/character_bot.h>

#include <game/server/core/components/accounts/account_manager.h>
#include <game/server/core/components/duties/duties_manager.h>

#include <scenarios/managers/scenario_group_manager.h>
#include <scenarios/impl/scenario_dungeon.h>

CGameControllerDungeon::CGameControllerDungeon(class CGS* pGS, CDungeonData* pDungeon) : CGameControllerDefault(pGS)
{
	m_GameFlags = 0;
	m_pDungeon = pDungeon;
	m_pEntWaitingDoor = new CEntityDungeonWaitingDoor(&GS()->m_World, pDungeon->GetWaitingDoorPos());
	ChangeState(CDungeonData::STATE_INACTIVE);
}

CGameControllerDungeon::~CGameControllerDungeon()
{
	delete m_pEntWaitingDoor;
}

void CGameControllerDungeon::FinishDungeon()
{
	ChangeState(CDungeonData::STATE_FINISH);
}

void CGameControllerDungeon::ChangeState(int NewState)
{
	const auto CurrentState = m_pDungeon->GetState();
	if(NewState == CurrentState)
		return;

	// update dungeon state
	m_pDungeon->SetState(NewState);

	// inactive state
	if(NewState == CDungeonData::STATE_INACTIVE)
	{
		// update
		m_EndTick = 0;
		m_WarmupTick = 0;
		m_LastWarmupTick = 0;
		m_ScenarioID = 0;
		m_StartedPlayersNum = 0;
		m_SafeSpawnTick = 0;
		m_pEntWaitingDoor->Close();
	}

	// waiting state
	else if(NewState == CDungeonData::STATE_WAITING)
	{
		// update
		m_WarmupTick = Server()->TickSpeed() * g_Config.m_SvDutiesWarmup;
	}

	// started state
	else if(NewState == CDungeonData::STATE_ACTIVE)
	{
		// update & initialize by state start
		m_ScenarioID = GS()->ScenarioGroupManager()->RegisterScenario<CDungeonScenario>(-1, m_pDungeon->GetScenario());
		m_StartedPlayersNum = GetPlayersNum();
		m_EndTick = Server()->TickSpeed() * 600;
		m_SafeSpawnTick = Server()->Tick() + (Server()->TickSpeed() * g_Config.m_SvDungeonSafeTime);
		m_pEntWaitingDoor->Open();

		// assert
		dbg_assert(m_ScenarioID != -1, "failed to register dungeon scenario");

		// add players to dungeon scenario
		auto pScenario = GS()->ScenarioGroupManager()->GetScenario(m_ScenarioID);
		for(int i = 0; i < MAX_PLAYERS; i++)
		{
			auto* pPlayer = GS()->GetPlayer(i);
			if(pPlayer && GS()->IsPlayerInWorld(i, m_pDungeon->GetWorldID()))
			{
				pScenario->AddParticipant(i);
				pPlayer->GetSharedData().m_TempStartDungeonTick = Server()->Tick();
			}
		}

		KillAllPlayers();

		// information
		const auto WorldID = m_pDungeon->GetWorldID();
		GS()->ChatWorld(WorldID, "Dungeon:", "You are given 10 minutes to complete of dungeon!");
		GS()->ChatWorld(WorldID, "Dungeon:", "Safe time is active for {} seconds.", (int)g_Config.m_SvDungeonSafeTime);
		GS()->BroadcastWorld(WorldID, BroadcastPriority::VeryImportant, 500, "Dungeon started!");
	}

	// finish state
	else if(NewState == CDungeonData::STATE_FINISH)
	{
		// update
		m_EndTick = Server()->TickSpeed() * 20;

		// update finish time and save best records
		for(int i = 0; i < MAX_PLAYERS; i++)
		{
			auto* pPlayer = GS()->GetPlayer(i);
			if(!pPlayer || pPlayer->GetTeam() == TEAM_SPECTATORS || !GS()->IsPlayerInWorld(i, m_pDungeon->GetWorldID()))
				continue;

			SaveDungeonRecord(pPlayer);
			GS()->Chat(-1, "'{~}' finished '{}'.", Server()->ClientName(i), m_pDungeon->GetName());
		}
	}
}


void CGameControllerDungeon::Process()
{
	const auto PlayersNum = GetPlayersNum();
	const auto WorldID = m_pDungeon->GetWorldID();
	m_pDungeon->UpdatePlayers(PlayersNum);

	auto State = m_pDungeon->GetState();

	// update inactive and waiting state
	if(PlayersNum < 1 && State != CDungeonData::STATE_INACTIVE)
	{
		ChangeState(CDungeonData::STATE_INACTIVE);
		State = m_pDungeon->GetState();
	}
	else if(PlayersNum >= 1 && State == CDungeonData::STATE_INACTIVE)
	{
		ChangeState(CDungeonData::STATE_WAITING);
		State = m_pDungeon->GetState();
	}

	// waiting state
	if(State == CDungeonData::STATE_WAITING)
	{
		if(m_WarmupTick)
		{
			// the ability to start prematurely on readiness
			const int PlayersReadyNum = GetPlayersReadyNum();
			if(PlayersReadyNum >= PlayersNum && m_WarmupTick > 10 * Server()->TickSpeed())
			{
				m_LastWarmupTick = m_WarmupTick;
				m_WarmupTick = 10 * Server()->TickSpeed();
			}
			else if(PlayersReadyNum < PlayersNum && m_LastWarmupTick > 0)
			{
				const int SkippedTick = 10 * Server()->TickSpeed() - m_WarmupTick;
				m_WarmupTick = m_LastWarmupTick - SkippedTick;
				m_LastWarmupTick = 0;
			}

			// output before the start of the passage
			GS()->BroadcastWorld(WorldID, BroadcastPriority::VeryImportant, 100, "Players {}/{} ready \u2014 press 'Vote yes' to join in!", PlayersReadyNum, PlayersNum);

			// update waiting time
			m_WarmupTick--;
			if(!m_WarmupTick)
			{
				ChangeState(CDungeonData::STATE_ACTIVE);
			}
		}

		m_ShiftRoundStartTick = Server()->Tick();
	}

	// finished
	else if(State == CDungeonData::STATE_FINISH)
	{
		if(m_EndTick)
		{
			const auto Sec = m_EndTick / Server()->TickSpeed();
			GS()->BroadcastWorld(WorldID, BroadcastPriority::VeryImportant, 500, "Dungeon ended {} sec!", Sec);
		}
		else if(PlayersNum < 1)
		{
			ChangeState(CDungeonData::STATE_INACTIVE);
		}
	}
}


void CGameControllerDungeon::OnCharacterDeath(CPlayer* pVictim, CPlayer* pKiller, int Weapon)
{
	const auto State = m_pDungeon->GetState();
	const int VictimCID = pVictim->GetCID();

	// remove dungeon lives
	if(State >= CDungeonData::STATE_ACTIVE && m_SafeSpawnTick < Server()->Tick())
	{
		auto pScenario = GS()->ScenarioGroupManager()->GetScenario(m_ScenarioID);
		if(pScenario)
		{
			if(pScenario->ConsumeGroupLife())
				GS()->ChatWorld(GS()->GetWorldID(), "Dungeon scenario", "Group lives left: {}.", pScenario->GetGroupLives());
			else
				pScenario->RemoveParticipant(VictimCID);
		}
	}

	CGameControllerDefault::OnCharacterDeath(pVictim, pKiller, Weapon);
}

bool CGameControllerDungeon::OnCharacterSpawn(CCharacter* pChr)
{
	const auto State = m_pDungeon->GetState();
	auto* pPlayer = pChr->GetPlayer();
	const int ClientID = pPlayer->GetCID();

	// check dungeon state and lives for thrown out of dungeon
	if(State >= CDungeonData::STATE_ACTIVE && m_SafeSpawnTick < Server()->Tick())
	{
		auto pScenario = GS()->ScenarioGroupManager()->GetScenario(m_ScenarioID);
		const bool DungeonFinished = (State == CDungeonData::STATE_FINISH);
		const bool NoGroupLives = pScenario && pScenario->GetGroupLives() <= 0;
		if(!pScenario || DungeonFinished || NoGroupLives)
		{
			GS()->Chat(ClientID, "You were thrown out of dungeon!");

			if(pScenario)
				pScenario->RemoveParticipant(ClientID);

			const int LatestCorrectWorldID = GS()->Core()->AccountManager()->GetLastVisitedWorldID(pPlayer);
			pPlayer->ChangeWorld(LatestCorrectWorldID);
			return false;
		}

		CGameControllerDefault::OnCharacterSpawn(pChr);
		return true;
	}

	// update vote menu for player
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		auto* pPlayer = GS()->GetPlayer(i);
		if(!pPlayer || pPlayer->GetTeam() == TEAM_SPECTATORS || !GS()->IsPlayerInWorld(i, m_pDungeon->GetWorldID()))
			continue;

		pPlayer->m_VotesData.UpdateVotesIf(MENU_DUTIES_LIST);
	}

	CGameControllerDefault::OnCharacterSpawn(pChr);
	return true;
}


void CGameControllerDungeon::KillAllPlayers() const
{
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		auto* pCharacter = GS()->GetPlayerChar(i);
		if(pCharacter && GS()->IsPlayerInWorld(i, m_pDungeon->GetWorldID()))
			pCharacter->Die(i, WEAPON_WORLD);
	}
}


int CGameControllerDungeon::GetPlayersReadyNum() const
{
    int ReadyPlayers = 0;
	const auto WorldID = m_pDungeon->GetWorldID();

    for(int i = 0; i < MAX_PLAYERS; i++)
    {
		auto* pPlayer = GS()->GetPlayer(i);
		if(!pPlayer || pPlayer->GetTeam() == TEAM_SPECTATORS || !GS()->IsPlayerInWorld(i, WorldID))
			continue;

		if(pPlayer->GetSharedData().m_TempDutiesReady)
            ReadyPlayers++;
    }

    return ReadyPlayers;
}


int CGameControllerDungeon::GetPlayersNum() const
{
	int PlayersNum = 0;
	const auto WorldID = m_pDungeon->GetWorldID();

	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		auto* pPlayer = GS()->GetPlayer(i);
		if(!pPlayer || pPlayer->GetTeam() == TEAM_SPECTATORS || !GS()->IsPlayerInWorld(i, WorldID))
			continue;

		PlayersNum++;
	}

	return PlayersNum;
}

void CGameControllerDungeon::Tick()
{
	if(m_EndTick)
	{
		m_EndTick--;
		if(!m_EndTick)
			KillAllPlayers();
	}

	Process();
	CGameControllerDefault::Tick();
}

void CGameControllerDungeon::Snap()
{
	// vanilla snap
	auto* pGameInfoObj = (CNetObj_GameInfo*)Server()->SnapNewItem(NETOBJTYPE_GAMEINFO, 0, sizeof(CNetObj_GameInfo));
	if(!pGameInfoObj)
		return;

	pGameInfoObj->m_GameFlags = m_GameFlags;
	pGameInfoObj->m_GameStateFlags = 0;
	pGameInfoObj->m_RoundStartTick = m_ShiftRoundStartTick;
	pGameInfoObj->m_WarmupTimer = m_WarmupTick;
	pGameInfoObj->m_RoundNum = 0;
	pGameInfoObj->m_RoundCurrent = 1;

	// ddnet snap
	auto* pGameInfoEx = (CNetObj_GameInfoEx*)Server()->SnapNewItem(NETOBJTYPE_GAMEINFOEX, 0, sizeof(CNetObj_GameInfoEx));
	if(!pGameInfoEx)
		return;

	pGameInfoEx->m_Flags = GAMEINFOFLAG_GAMETYPE_PLUS | GAMEINFOFLAG_ALLOW_HOOK_COLL | GAMEINFOFLAG_PREDICT_VANILLA;
	pGameInfoEx->m_Flags2 = GAMEINFOFLAG2_GAMETYPE_CITY | GAMEINFOFLAG2_ALLOW_X_SKINS | GAMEINFOFLAG2_HUD_DDRACE | GAMEINFOFLAG2_HUD_HEALTH_ARMOR | GAMEINFOFLAG2_HUD_AMMO;
	pGameInfoEx->m_Version = GAMEINFO_CURVERSION;
}

void CGameControllerDungeon::SaveDungeonRecord(const CPlayer* pPlayer) const
{
	if(!pPlayer || !pPlayer->Account())
		return;

	const int StartTick = pPlayer->GetSharedData().m_TempStartDungeonTick;
	if(StartTick <= 0)
		return;

	const int FinishTime = std::max(1, Server()->Tick() - StartTick);
	const int AccountID = pPlayer->Account()->GetID();
	const int DungeonID = m_pDungeon->GetID();

	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_dungeons_records",
		"WHERE UserID = '{}' AND DungeonID = '{}' ORDER BY Time ASC LIMIT 1", AccountID, DungeonID);

	if(!pRes->next())
	{
		Database->Execute<DB::INSERT>("tw_dungeons_records", "(UserID, DungeonID, Time) VALUES ('{}', '{}', '{}')",
			AccountID, DungeonID, FinishTime);
		return;
	}

	const int BestTime = pRes->getInt("Time");
	if(FinishTime < BestTime)
	{
		const int RecordID = pRes->getInt("ID");
		Database->Execute<DB::UPDATE>("tw_dungeons_records", "Time = '{}' WHERE ID = '{}'", FinishTime, RecordID);
	}
}
