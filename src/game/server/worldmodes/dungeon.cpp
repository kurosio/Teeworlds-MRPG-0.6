/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "dungeon.h"

#include <game/server/entity.h>
#include <game/server/gamecontext.h>

#include <game/server/core/entities/logic/botwall.h>
#include <game/server/core/entities/logic/logicwall.h>

#include <game/server/core/components/accounts/account_manager.h>
#include <game/server/core/components/dungeons/dungeon_manager.h>

CGameControllerDungeon::CGameControllerDungeon(class CGS* pGS, CDungeonData* pDungeon) : IGameController(pGS)
{
	m_GameFlags = 0;
	m_ActivePlayers = 0;
	m_pDungeon = pDungeon;

	// create waiting door
	m_pEntWaitingDoor = new CEntityDungeonWaitingDoor(&GS()->m_World, pDungeon->GetWaitingDoorPos());

	// create logic doors
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_dungeons_door", "WHERE DungeonID = '{}'", pDungeon->GetID());
	while(pRes->next())
	{
		const int RequiredBotID = pRes->getInt("BotID");
		vec2 Pos = vec2(pRes->getInt("PosX"), pRes->getInt("PosY"));
		m_vpEntLogicDoor.emplace_back(new CLogicDungeonDoorKey(&GS()->m_World, Pos, RequiredBotID));
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

void CGameControllerDungeon::KillAllPlayers() const
{
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		auto* pCharacter = GS()->GetPlayerChar(i);
		if(pCharacter && GS()->IsPlayerInWorld(i, m_pDungeon->GetWorldID()))
		{
			pCharacter->Die(i, WEAPON_WORLD);
		}
	}
}

void CGameControllerDungeon::ChangeState(int State)
{
	const auto CurrentState = m_pDungeon->GetState();
	if(State == CurrentState)
		return;

	m_pDungeon->SetState(State);

	// inactive state
	if(State == CDungeonData::STATE_INACTIVE)
	{
		m_MaximumTick = 0;
		m_FinishedTick = 0;
		m_StartingTick = 0;
		m_LastStartingTick = 0;
		m_SafeTick = 0;

		// update
		SetMobsSpawn(false);
	}

	// waiting state
	else if(State == CDungeonData::STATE_WAITING)
	{
		m_SyncDungeon = GetSyncFactor();
		m_StartingTick = Server()->TickSpeed() * g_Config.m_SvDungeonWaitingTime;

		// update
		m_pDungeon->UpdateProgress(0);
		m_pEntWaitingDoor->Close();
		SetMobsSpawn(false);
		ResetDoorKeyState();
	}

	// started state
	else if(State == CDungeonData::STATE_STARTED)
	{
		m_ActivePlayers = GetPlayersNum();
		m_MaximumTick = Server()->TickSpeed() * 600;
		m_SafeTick = Server()->TickSpeed() * 30;

		// update
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
		m_SafeTick = 0;
		m_FinishedTick = Server()->TickSpeed() * 20;

		SetMobsSpawn(false);

		//dynamic_string Buffer;
		//int FinishTime = -1;
		////int BestPassageHelp = 0;

		//// dungeon finished information
		//char aTimeFormat[64];
		//str_format(aTimeFormat, sizeof(aTimeFormat), "Time: '%d minute(s) %d second(s)'.", FinishTime / 60, FinishTime - (FinishTime / 60 * 60));
		//GS()->Chat(-1, "Group{}!", Buffer.buffer());
		////GS()->Chat(-1, "'{}' finished {}!", CDungeonData::ms_aDungeon[m_DungeonID].m_aName, aTimeFormat);
	}

	//// - - - - - - - - - - - - - - - - - - - - - -
	//// used when changing state to finished
	//else if(State == DUNGEON_FINISHED)
	//{
	//	SetMobsSpawn(false);
	//	KillAllPlayers();
	//}
}

void CGameControllerDungeon::StateTick()
{
	const auto Players = GetPlayersNum();
	const auto State = m_pDungeon->GetState();
	const auto WorldID = m_pDungeon->GetWorldID();
	m_pDungeon->UpdatePlayers(Players);

	// update inactive and waiting state
	if(State != CDungeonData::STATE_INACTIVE)
	{
		if(Players < 1)
		{
			ChangeState(CDungeonData::STATE_INACTIVE);
			return;
		}
	}
	else
	{
		if(Players >= 1)
		{
			ChangeState(CDungeonData::STATE_WAITING);
			return;
		}
	}

	// waiting state
	if(State == CDungeonData::STATE_WAITING)
	{
		if(m_StartingTick)
		{
			// the ability to start prematurely on readiness
			const int PlayersReadyState = PlayersReady();
			if(PlayersReadyState >= Players && m_StartingTick > 10 * Server()->TickSpeed())
			{
				m_LastStartingTick = m_StartingTick;
				m_StartingTick = 10 * Server()->TickSpeed();
			}
			else if(PlayersReadyState < Players && m_LastStartingTick > 0)
			{
				const int SkippedTick = 10 * Server()->TickSpeed() - m_StartingTick;
				m_StartingTick = m_LastStartingTick - SkippedTick;
				m_LastStartingTick = 0;
			}

			// output before the start of the passage
			const int Time = m_StartingTick / Server()->TickSpeed();
			GS()->BroadcastWorld(WorldID, BroadcastPriority::VeryImportant, 500,
				"Dungeon waiting {} sec!\nPlayer's are ready to start right now {} of {}!\nYou can change state with 'Vote yes'",
				Time, PlayersReadyState, Players);

			// update waiting time
			m_StartingTick--;
			if(!m_StartingTick)
			{
				m_ActivePlayers = Players;
				ChangeState(CDungeonData::STATE_STARTED);
			}
		}

		m_ShiftRoundStartTick = Server()->Tick();
	}

	// started
	else if(State == CDungeonData::STATE_STARTED)
	{
		// update players time
		for(int i = 0; i < MAX_PLAYERS; i++)
		{
			CPlayer* pPlayer = GS()->GetPlayer(i);
			if(!pPlayer || !GS()->IsPlayerInWorld(i, WorldID))
				continue;

			pPlayer->GetSharedData().m_TempTimeDungeon++;
		}

		// security tick during which time the player will not return to the old world
		if(m_SafeTick)
		{
			m_SafeTick--;
			if(!m_SafeTick)
			{
				GS()->ChatWorld(WorldID, "Dungeon:", "The security timer is over, be careful!");
			}
		}

		// finish the dungeon when the dungeon is successfully completed
		if(GetRemainingMobsNum() <= 0)
		{
			ChangeState(CDungeonData::STATE_FINISHED);
		}
	}

	//// - - - - - - - - - - - - - - - - - - - - - -
	//// used in the tick when the waiting is finished
	//else if(State == CDungeonData::STATE_FINISHED)
	//{
	//	if(m_FinishedTick)
	//	{
	//		const int Time = m_FinishedTick / Server()->TickSpeed();
	//		GS()->BroadcastWorld(m_WorldID, BroadcastPriority::VeryImportant, 500, "Dungeon ended {} sec!", Time);

	//		m_FinishedTick--;
	//	}

	//	if(!m_FinishedTick)
	//		ChangeState(DUNGEON_FINISHED);
	//}
}

void CGameControllerDungeon::OnCharacterDeath(CPlayer* pVictim, CPlayer* pKiller, int Weapon)
{
	IGameController::OnCharacterDeath(pVictim, pKiller, Weapon);

	if(pVictim->IsBot())
	{
		auto* pVictimBot = static_cast<CPlayerBot*>(pVictim);

		// dissable allowed spawn after die
		if(m_pDungeon->GetState() >= CDungeonData::STATE_STARTED)
			pVictimBot->SetAllowedSpawn(false);

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

	if(State >= CDungeonData::STATE_STARTED)
	{
		const int ClientID = pChr->GetPlayer()->GetCID();

		// player died after the safety timer ended
		if(!m_SafeTick)
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
		CPlayer* pPlayer = GS()->GetPlayer(i);
		if(!pPlayer || !GS()->IsPlayerInWorld(i, m_pDungeon->GetWorldID()))
			continue;

		pPlayer->m_VotesData.UpdateVotesIf(MENU_DUNGEONS);
	}

	IGameController::OnCharacterSpawn(pChr);
	return true;
}

void CGameControllerDungeon::UpdateDoorKeyState()
{
	for(auto* pDoor = (CLogicDungeonDoorKey*)GS()->m_World.FindFirst(CGameWorld::ENTTYPE_DUNGEON_PROGRESS_DOOR);
		pDoor; pDoor = (CLogicDungeonDoorKey*)pDoor->TypeNext())
	{
		if(pDoor->SyncStateChanges())
		{
			GS()->ChatWorld(m_pDungeon->GetWorldID(), "Dungeon:", "Door creaking.. Opened door somewhere!");
		}
	}
}

void CGameControllerDungeon::ResetDoorKeyState()
{
	for(auto* pDoor = (CLogicDungeonDoorKey*)GS()->m_World.FindFirst(CGameWorld::ENTTYPE_DUNGEON_PROGRESS_DOOR);
		pDoor; pDoor = (CLogicDungeonDoorKey*)pDoor->TypeNext())
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

int CGameControllerDungeon::PlayersReady() const
{
    int ReadyPlayers = 0;

    for(int i = 0; i < MAX_PLAYERS; i++)
    {
        auto* pPlayer = GS()->GetPlayer(i);
        if(pPlayer && GS()->IsPlayerInWorld(i, m_pDungeon->GetWorldID()) && pPlayer->GetSharedData().m_TempDungeonReady)
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
		if(Server()->GetClientWorldID(i) == WorldID)
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
			{
				pPlayer->GetCharacter()->Die(i, WEAPON_WORLD);
			}
		}
	}
}

// TODO: something to do with the balance
int CGameControllerDungeon::GetSyncFactor() const
{
	//int MaxFactor = 0;
	//int MinFactor = std::numeric_limits<int>::max();
	//int BotCount = 0;

	//for(int i = MAX_PLAYERS; i < MAX_CLIENTS; i++)
	//{
	//	CPlayerBot* pBotPlayer = dynamic_cast<CPlayerBot*>(GS()->GetPlayer(i));
	//	if(pBotPlayer && pBotPlayer->GetBotType() == TYPE_BOT_MOB && pBotPlayer->GetCurrentWorldID() == m_WorldID)
	//	{
	//		const int LevelDisciple = pBotPlayer->GetTotalAttributes();
	//		MinFactor = minimum(MinFactor, LevelDisciple);
	//		MaxFactor = maximum(MaxFactor, LevelDisciple);
	//		BotCount++;
	//	}
	//}

	//if(BotCount == 0)
	//{
		return 0; // No bot's, return default value
	//}

	//return (MaxFactor + MinFactor) / 2;
}

int CGameControllerDungeon::GetAttributeDungeonSyncByClass(ProfessionIdentifier ProfID, AttributeIdentifier ID) const
{
	float Percent = 0.0f;
	const float ActiveAttribute = m_SyncDungeon / 2.0f;
	const AttributeGroup Type = GS()->GetAttributeInfo(ID)->GetGroup();

	// - - - - - - - - -- - - -
	// balance tanks
	if(ProfID == ProfessionIdentifier::Tank)
	{
		// basic default tank upgrades
		if(Type == AttributeGroup::Tank)
			Percent = 50.0f;

		// very small dps boost
		else if(Type == AttributeGroup::DamageType && ID != AttributeIdentifier::DMG)
			return 0;
	}

	// - - - - - - - - - - - - -
	// balance dps
	if(ProfID == ProfessionIdentifier::Dps)
	{
		// basic default dps upgrades
		if(Type == AttributeGroup::Dps || Type == AttributeGroup::DamageType)
			Percent = 0.1f;

		// very small tank boost
		else if(Type == AttributeGroup::Tank)
			Percent = 5.f;
	}

	// - - - - - - - - - - - - -
	// balance healer
	if(ProfID == ProfessionIdentifier::Healer)
	{
		// basic default healer upgrades
		if(Type == AttributeGroup::Healer)
			Percent = minimum(25.0f + (m_ActivePlayers * 2.0f), 50.0f);

		// small tank boost
		else if(Type == AttributeGroup::Tank)
			Percent = 10.f;

		// very small dps boost
		else if(Type == AttributeGroup::DamageType && ID != AttributeIdentifier::DMG)
			return 0;
	}

	// return final stat by percent rest
	const int AttributeSyncProcent = translate_to_percent_rest(ActiveAttribute, Percent);
	return maximum(AttributeSyncProcent, 1);
}

void CGameControllerDungeon::Tick()
{
	if(m_MaximumTick)
	{
		m_MaximumTick--;
		if(!m_MaximumTick)
			ChangeState(CDungeonData::STATE_FINISHED);
	}

	StateTick();
	IGameController::Tick();
}

void CGameControllerDungeon::Snap()
{
	// vanilla snap
	CNetObj_GameInfo* pGameInfoObj = (CNetObj_GameInfo*)Server()->SnapNewItem(NETOBJTYPE_GAMEINFO, 0, sizeof(CNetObj_GameInfo));
	if(!pGameInfoObj)
		return;

	pGameInfoObj->m_GameFlags = m_GameFlags;
	pGameInfoObj->m_GameStateFlags = 0;
	pGameInfoObj->m_RoundStartTick = m_ShiftRoundStartTick;
	pGameInfoObj->m_WarmupTimer = 0;
	pGameInfoObj->m_RoundNum = 0;
	pGameInfoObj->m_RoundCurrent = 1;

	// ddnet snap
	CNetObj_GameInfoEx* pGameInfoEx = (CNetObj_GameInfoEx*)Server()->SnapNewItem(NETOBJTYPE_GAMEINFOEX, 0, sizeof(CNetObj_GameInfoEx));
	if(!pGameInfoEx)
		return;

	pGameInfoEx->m_Flags = GAMEINFOFLAG_GAMETYPE_PLUS | GAMEINFOFLAG_ALLOW_EYE_WHEEL | GAMEINFOFLAG_ALLOW_HOOK_COLL | GAMEINFOFLAG_PREDICT_VANILLA;
	pGameInfoEx->m_Flags2 = GAMEINFOFLAG2_GAMETYPE_CITY | GAMEINFOFLAG2_ALLOW_X_SKINS | GAMEINFOFLAG2_HUD_DDRACE | GAMEINFOFLAG2_HUD_HEALTH_ARMOR | GAMEINFOFLAG2_HUD_AMMO;
	pGameInfoEx->m_Version = GAMEINFO_CURVERSION;
}

CEntityDungeonWaitingDoor::CEntityDungeonWaitingDoor(CGameWorld* pGameWorld, vec2 Pos)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_DUNGEON_DOOR, Pos)
{
	m_Closed = true;
	GS()->Collision()->FillLengthWall(32, vec2(0, -1), &m_Pos, &m_PosTo);
	GameWorld()->InsertEntity(this);
}

void CEntityDungeonWaitingDoor::Tick()
{
	if(!m_Closed)
		return;

	for(CCharacter* pChar = (CCharacter*)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
	{
		vec2 IntersectPos;
		if(closest_point_on_line(m_Pos, m_PosTo, pChar->m_Core.m_Pos, IntersectPos))
		{
			float Distance = distance(IntersectPos, pChar->m_Core.m_Pos);
			if(Distance <= (float)g_Config.m_SvDoorRadiusHit)
			{
				pChar->SetDoorHit(m_Pos, m_PosTo);
				pChar->Die(pChar->GetPlayer()->GetCID(), WEAPON_WORLD);
			}
		}
	}
}

void CEntityDungeonWaitingDoor::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient) || !m_Closed)
		return;

	GS()->SnapLaser(SnappingClient, GetID(), m_Pos, m_PosTo, Server()->Tick() - 2, LASERTYPE_DOOR);
}