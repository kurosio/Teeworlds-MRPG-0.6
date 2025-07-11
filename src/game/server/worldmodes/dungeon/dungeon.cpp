/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "entities/waiting_door.h"
#include "entities/progress_door.h"
#include "dungeon.h"

#include <game/server/entity.h>
#include <game/server/gamecontext.h>
#include <game/server/entities/character_bot.h>

#include <game/server/core/components/accounts/account_manager.h>
#include <game/server/core/components/duties/duties_manager.h>

#include <game/server/core/scenarios/scenario_dungeon.h>
#include <game/server/core/tools/scenario_group_manager.h>

CGameControllerDungeon::CGameControllerDungeon(class CGS* pGS, CDungeonData* pDungeon) : IGameController(pGS)
{
	m_GameFlags = 0;
	m_pDungeon = pDungeon;

	// create waiting door
	m_pEntWaitingDoor = new CEntityDungeonWaitingDoor(&GS()->m_World, pDungeon->GetWaitingDoorPos());

	// create logic doors
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_dungeons_door", "WHERE DungeonID = '{}'", pDungeon->GetID());
	while(pRes->next())
	{
		const auto RequiredBotID = pRes->getInt("BotID");
		const auto DoorPos = vec2(pRes->getInt("PosX"), pRes->getInt("PosY"));
		m_vpEntLogicDoor.emplace_back(new CEntityDungeonProgressDoor(&GS()->m_World, DoorPos, RequiredBotID));
	}

	// update state
	ChangeState(CDungeonData::STATE_INACTIVE);
}

CGameControllerDungeon::~CGameControllerDungeon()
{
	delete m_pEntWaitingDoor;
	for(auto*& pEnt : m_vpEntLogicDoor)
		delete pEnt;
	m_vpEntLogicDoor.clear();
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
		m_FinishTick = 0;
		m_SafetyTick = 0;
		m_WaitingTick = 0;
		m_LastWaitingTick = 0;
		m_StartedPlayersNum = 0;
		m_pDungeon->UpdateProgress(0);
		m_pEntWaitingDoor->Close();
		SetMobsSpawn(false);
		ResetDoorKeyState();
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
		m_vSyncFactor.clear();
		PrepareSyncFactors(m_vSyncFactor);
		m_StartedPlayersNum = GetPlayersNum();
		m_SafetyTick = Server()->TickSpeed() * 30;
		m_EndTick = Server()->TickSpeed() * 600;
		m_pEntWaitingDoor->Open();
		SetMobsSpawn(true);
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
		m_FinishTick = Server()->TickSpeed() * 20;
		SetMobsSpawn(false);

		// update finish time
		// TODO: replace
		for(int i = 0; i < MAX_PLAYERS; i++)
		{
			auto* pPlayer = GS()->GetPlayer(i);
			if(!pPlayer || pPlayer->GetTeam() == TEAM_SPECTATORS || !GS()->IsPlayerInWorld(i, m_pDungeon->GetWorldID()))
				continue;

			//int FinishTime = Server()->Tick() - pPlayer->GetSharedData().m_TempStartDungeonTick;
			GS()->Chat(-1, "'{}' finished '{}'.", Server()->ClientName(i), m_pDungeon->GetName());
		}
	}
}


void CGameControllerDungeon::Process()
{
	const auto PlayersNum = GetPlayersNum();
	const auto State = m_pDungeon->GetState();
	const auto WorldID = m_pDungeon->GetWorldID();
	m_pDungeon->UpdatePlayers(PlayersNum);

	// update inactive and waiting state
	if(PlayersNum < 1 && State != CDungeonData::STATE_INACTIVE)
		ChangeState(CDungeonData::STATE_INACTIVE);
	else if(PlayersNum >= 1 && State == CDungeonData::STATE_INACTIVE)
		ChangeState(CDungeonData::STATE_WAITING);

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

		// finish is successfully completed
		if(GetRemainingMobsNum() <= 0)
		{
			ChangeState(CDungeonData::STATE_FINISHED);
		}
	}

	// finished
	else if(State == CDungeonData::STATE_FINISHED)
	{
		m_FinishTick--;

		if(m_FinishTick)
		{
			const auto Sec = m_FinishTick / Server()->TickSpeed();
			GS()->BroadcastWorld(WorldID, BroadcastPriority::VeryImportant, 500, "Dungeon ended {} sec!", Sec);
		}
		else
			KillAllPlayers();
	}
}


void CGameControllerDungeon::OnCharacterDeath(CPlayer* pVictim, CPlayer* pKiller, int Weapon)
{
	IGameController::OnCharacterDeath(pVictim, pKiller, Weapon);

	if(pVictim->IsBot())
	{
		auto* pVictimBot = static_cast<CPlayerBot*>(pVictim);

		// update progress
		if(pKiller->GetCID() != pVictim->GetCID() && pVictimBot->GetBotType() == TYPE_BOT_MOB)
		{
			const int Progress = 100 - translate_to_percent(GetTotalMobsNum(), GetRemainingMobsNum());
			GS()->ChatWorld(m_pDungeon->GetWorldID(), "Dungeon:", "The dungeon is completed on [{}%]", Progress);
			m_pDungeon->UpdateProgress(Progress);
			UpdateDoorKeyState();
		}
	}
}

bool CGameControllerDungeon::OnCharacterSpawn(CCharacter* pChr)
{
	const auto State = m_pDungeon->GetState();
	const int ClientID = pChr->GetPlayer()->GetCID();

	if(State >= CDungeonData::STATE_STARTED)
	{
		if(!GS()->ScenarioGroupManager()->IsActive(m_ScenarioID))
			m_ScenarioID = GS()->ScenarioGroupManager()->RegisterScenario<CDungeonScenario>(pChr->GetPlayer()->GetCID(), m_pDungeon->GetScenario());
		else if(auto pScenario = GS()->ScenarioGroupManager()->GetScenario(m_ScenarioID))
			pScenario->AddParticipant(pChr->GetPlayer()->GetCID());
		else
			dbg_assert(false, "Failed to register scenario id for dungeon.");

		// player died after the safety timer ended
		if(!m_SafetyTick)
		{
			GS()->Chat(ClientID, "You were thrown out of dungeon!");

			const int LatestCorrectWorldID = GS()->Core()->AccountManager()->GetLastVisitedWorldID(pChr->GetPlayer());
			pChr->GetPlayer()->ChangeWorld(LatestCorrectWorldID);
			return false;
		}

		IGameController::OnCharacterSpawn(pChr);
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

	IGameController::OnCharacterSpawn(pChr);
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


void CGameControllerDungeon::UpdateDoorKeyState()
{
	for(auto*& pDoor : m_vpEntLogicDoor)
	{
		if(pDoor->Update())
			GS()->ChatWorld(m_pDungeon->GetWorldID(), "Dungeon:", "Door creaking.. Opened door somewhere!");
	}
}


void CGameControllerDungeon::ResetDoorKeyState()
{
	for(auto*& pDoor : m_vpEntLogicDoor)
		pDoor->ResetDoor();
}


int CGameControllerDungeon::GetTotalMobsNum() const
{
	int NumMobs = 0;

	for(int i = MAX_PLAYERS; i < MAX_CLIENTS; i++)
	{
		auto* pPlayer = static_cast<CPlayerBot*>(GS()->GetPlayer(i));
		if(!pPlayer || pPlayer->GetBotType() != TYPE_BOT_MOB)
			continue;

		NumMobs++;
	}

	return NumMobs;
}


int CGameControllerDungeon::GetRemainingMobsNum() const
{
	int LeftMobs = 0;

	for(int i = MAX_PLAYERS; i < MAX_CLIENTS; i++)
	{
		auto* pPlayer = dynamic_cast<CPlayerBot*>(GS()->GetPlayer(i));
		if(!pPlayer || !pPlayer->GetCharacter() || pPlayer->GetBotType() != TYPE_BOT_MOB)
			continue;

		LeftMobs++;
	}

	return LeftMobs;
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


void CGameControllerDungeon::SetMobsSpawn(bool AllowedSpawn)
{
	for(int i = MAX_PLAYERS; i < MAX_CLIENTS; i++)
	{
		auto* pPlayer = dynamic_cast<CPlayerBot*>(GS()->GetPlayer(i));
		if(pPlayer && pPlayer->GetBotType() == TYPE_BOT_MOB)
		{
			pPlayer->SetAllowedSpawn(AllowedSpawn);

			if(!AllowedSpawn && pPlayer->GetCharacter())
				pPlayer->GetCharacter()->Die(i, WEAPON_WORLD);
		}
	}
}

void CGameControllerDungeon::PrepareSyncFactors(std::map<AttributeIdentifier, int>& vResultMap)
{
	vResultMap[AttributeIdentifier::HP] = DEFAULT_BASE_HP;
	vResultMap[AttributeIdentifier::MP] = DEFAULT_BASE_MP;

	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		auto* pPlayer = GS()->GetPlayer(i, true);
		if(!pPlayer || pPlayer->GetTeam() == TEAM_SPECTATORS || !GS()->IsPlayerInWorld(i, m_pDungeon->GetWorldID()))
			continue;

		for(auto ID = (int)AttributeIdentifier::DMG; ID < (int)AttributeIdentifier::ATTRIBUTES_NUM; ID++)
		{
			const auto AttributeID = (AttributeIdentifier)ID;
			vResultMap[AttributeID] += pPlayer->GetTotalRawAttributeValue(AttributeID);
		}
	}
}


int CGameControllerDungeon::CalculateMobAttribute(AttributeIdentifier ID, int PowerLevel, float BaseFactor, int MinValue) const
{
	auto PlayersTotalAttribute = GetAttributeDungeonSync(ID);
	auto PlayersNum = m_StartedPlayersNum;
	float PowerLevelMultiplier = 1.0f + (PowerLevel - 1) * 0.15f;
	auto AttributeValue = static_cast<int>(PlayersTotalAttribute * BaseFactor * PowerLevelMultiplier / std::sqrt(PlayersNum));
	return std::max(MinValue, AttributeValue);
}


int CGameControllerDungeon::GetAttributeDungeonSync(AttributeIdentifier ID) const
{
	return m_vSyncFactor.contains(ID) ? m_vSyncFactor.at(ID) : 0;
}


void CGameControllerDungeon::RefreshSyncAttributes()
{
	bool Refresh = false;
	bool RefreshUnchangedData = false;
	std::map<AttributeIdentifier, int> vCurrentMap {};
	PrepareSyncFactors(vCurrentMap);

	for(auto& [ID, Value] : m_vSyncFactor)
	{
		if(vCurrentMap[ID] > Value)
		{
			Value = vCurrentMap[ID];
			Refresh = true;
			RefreshUnchangedData = (!RefreshUnchangedData && (ID == AttributeIdentifier::HP || ID == AttributeIdentifier::MP)) || RefreshUnchangedData;
		}
	}

	if(Refresh)
	{
		for(int i = MAX_PLAYERS; i < MAX_CLIENTS; i++)
		{
			auto* pPlayerBot = dynamic_cast<CPlayerBot*>(GS()->GetPlayer(i));
			if(!pPlayerBot || !pPlayerBot->GetCharacter())
				continue;

			// update total player stats
			for(auto& [Id, Info] : CAttributeDescription::Data())
			{
				auto totalAttribute = pPlayerBot->GetTotalRawAttributeValue(Id);
				pPlayerBot->UpdateTotalAttributeValue(Id, totalAttribute);
			}

			// refresh unchanged data
			if(RefreshUnchangedData)
			{
				const int MaxStartHP = pPlayerBot->GetTotalAttributeValue(AttributeIdentifier::HP);
				const int MaxStartMP = pPlayerBot->GetTotalAttributeValue(AttributeIdentifier::MP);
				pPlayerBot->InitBasicStats(MaxStartHP, MaxStartMP, MaxStartHP, MaxStartMP);

				auto* pChar = pPlayerBot->GetCharacter();
				pChar->IncreaseHealth(MaxStartHP);
				pChar->IncreaseMana(MaxStartMP);
			}
		}
	}
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
	IGameController::Tick();
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

	pGameInfoEx->m_Flags = GAMEINFOFLAG_GAMETYPE_PLUS | GAMEINFOFLAG_ALLOW_EYE_WHEEL | GAMEINFOFLAG_ALLOW_HOOK_COLL | GAMEINFOFLAG_PREDICT_VANILLA;
	pGameInfoEx->m_Flags2 = GAMEINFOFLAG2_GAMETYPE_CITY | GAMEINFOFLAG2_ALLOW_X_SKINS | GAMEINFOFLAG2_HUD_DDRACE | GAMEINFOFLAG2_HUD_HEALTH_ARMOR | GAMEINFOFLAG2_HUD_AMMO;
	pGameInfoEx->m_Version = GAMEINFO_CURVERSION;
}