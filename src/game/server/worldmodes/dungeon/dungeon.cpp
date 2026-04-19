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
	ChangeState(CDungeonData::STATE_FINISHED);
}

void CGameControllerDungeon::ChangeState(int State)
{
	const auto CurrentState = m_pDungeon->GetState();
	if(State == CurrentState)
		return;

	// update dungeon state
	m_pDungeon->SetState(State);

	// inactive state
	if(State == CDungeonData::STATE_INACTIVE)
	{
		// update
		m_EndTick = 0;
		m_SafetyTick = 0;
		m_WaitingTick = 0;
		m_LastWaitingTick = 0;
		m_StartedPlayersNum = 0;
		m_pEntWaitingDoor->Close();
	}

	// waiting state
	else if(State == CDungeonData::STATE_WAITING)
	{
		// update
		m_WaitingTick = Server()->TickSpeed() * g_Config.m_SvDungeonWaitingTime;
	}

	// started state
	else if(State == CDungeonData::STATE_STARTED)
	{
		// update start time
		for(int i = 0; i < MAX_PLAYERS; i++)
		{
			auto* pPlayer = GS()->GetPlayer(i);
			if(pPlayer && GS()->IsPlayerInWorld(i, m_pDungeon->GetWorldID()))
				pPlayer->GetSharedData().m_TempStartDungeonTick = Server()->Tick();
		}

		// update
		m_StartedPlayersNum = GetPlayersNum();
		m_SafetyTick = Server()->TickSpeed() * 30;
		m_EndTick = Server()->TickSpeed() * 600;
		m_pEntWaitingDoor->Open();
		KillAllPlayers();

		// information
		const auto WorldID = m_pDungeon->GetWorldID();
		GS()->ChatWorld(WorldID, "Dungeon:", "The security timer is enabled for 30 seconds!");
		GS()->ChatWorld(WorldID, "Dungeon:", "You are given 10 minutes to complete of dungeon!");
		GS()->BroadcastWorld(WorldID, BroadcastPriority::VeryImportant, 500, "Dungeon started!");
	}

	// finish state
	else if(State == CDungeonData::STATE_FINISHED)
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
			GS()->Chat(-1, "'{}' finished '{}'.", Server()->ClientName(i), m_pDungeon->GetName());
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
		if(m_WaitingTick)
		{
			// the ability to start prematurely on readiness
			const int PlayersReadyNum = GetPlayersReadyNum();
			if(PlayersReadyNum >= PlayersNum && m_WaitingTick > 10 * Server()->TickSpeed())
			{
				m_LastWaitingTick = m_WaitingTick;
				m_WaitingTick = 10 * Server()->TickSpeed();
			}
			else if(PlayersReadyNum < PlayersNum && m_LastWaitingTick > 0)
			{
				const int SkippedTick = 10 * Server()->TickSpeed() - m_WaitingTick;
				m_WaitingTick = m_LastWaitingTick - SkippedTick;
				m_LastWaitingTick = 0;
			}

			// output before the start of the passage
			GS()->BroadcastWorld(WorldID, BroadcastPriority::VeryImportant, 100, "Players {}/{} ready \u2014 press 'Vote yes' to join in!", PlayersReadyNum, PlayersNum);

			// update waiting time
			m_WaitingTick--;
			if(!m_WaitingTick)
			{
				ChangeState(CDungeonData::STATE_STARTED);
			}
		}

		m_ShiftRoundStartTick = Server()->Tick();
	}

	// started
	else if(State == CDungeonData::STATE_STARTED)
	{
		// security tick during which time the player will not return to the old world
		if(m_SafetyTick)
		{
			m_SafetyTick--;
			if(!m_SafetyTick)
				GS()->ChatWorld(WorldID, "Dungeon:", "The security timer is over, be careful!");
		}
	}

	// finished
	else if(State == CDungeonData::STATE_FINISHED)
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
	CGameControllerDefault::OnCharacterDeath(pVictim, pKiller, Weapon);
}

bool CGameControllerDungeon::OnCharacterSpawn(CCharacter* pChr)
{
	const auto State = m_pDungeon->GetState();
	auto* pPlayer = pChr->GetPlayer();
	const int ClientID = pPlayer->GetCID();

	if(State >= CDungeonData::STATE_STARTED)
	{
		// update scenario and add participant by start
		if(!GS()->ScenarioGroupManager()->IsActive(m_ScenarioID))
			m_ScenarioID = GS()->ScenarioGroupManager()->RegisterScenario<CDungeonScenario>(ClientID, m_pDungeon->GetScenario());
		else if(auto pScenario = GS()->ScenarioGroupManager()->GetScenario(m_ScenarioID))
			pScenario->AddParticipant(ClientID);
		else
			dbg_assert(false, "Failed to register scenario id for dungeon.");

		// player died after the safety timer ended or dungeon finished
		if(!m_SafetyTick || (m_SafetyTick && State == CDungeonData::STATE_FINISHED))
		{
			GS()->Chat(ClientID, "You were thrown out of dungeon!");
			const int LatestCorrectWorldID = GS()->Core()->AccountManager()->GetLastVisitedWorldID(pPlayer);
			pPlayer->ChangeWorld(LatestCorrectWorldID);
			if(auto pScenario = GS()->ScenarioGroupManager()->GetScenario(m_ScenarioID))
				pScenario->RemoveParticipant(ClientID);
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

		if(pPlayer->GetSharedData().m_TempDungeonReady)
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
	pGameInfoObj->m_WarmupTimer = m_WaitingTick;
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
