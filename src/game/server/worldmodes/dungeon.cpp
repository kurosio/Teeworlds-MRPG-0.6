/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "dungeon.h"

#include <game/server/entity.h>
#include <game/server/gamecontext.h>

#include <game/server/core/entities/logic/botwall.h>
#include <game/server/core/entities/logic/logicwall.h>

#include <game/server/core/components/accounts/account_manager.h>
#include <game/server/core/components/dungeons/dungeon_manager.h>

CGameControllerDungeon::CGameControllerDungeon(class CGS* pGS) : IGameController(pGS)
{
	m_GameFlags = 0;
	m_WorldID = GS()->GetWorldID();
	m_ActivePlayers = 0;

	// init dungeon zone
	//auto iter = std::find_if(CDungeonData::ms_aDungeon.begin(), CDungeonData::ms_aDungeon.end(), [&](const std::pair<int, CDungeonData>& pDungeon)
	//{ return pDungeon.second.m_WorldID == m_WorldID; });
	//dbg_assert(iter != CDungeonData::ms_aDungeon.end(), "dungeon world type not found dungeon data");
	//m_DungeonID = iter->first;

	//// door creation to start
	//vec2 DoorPosition = vec2(CDungeonData::ms_aDungeon[m_DungeonID].m_DoorX, CDungeonData::ms_aDungeon[m_DungeonID].m_DoorY);
	//m_DungeonDoor = new DungeonDoor(&GS()->m_World, DoorPosition);
	//ChangeState(DUNGEON_WAITING);

	// key door construction
	//ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_dungeons_door", "WHERE DungeonID = '{}'", m_DungeonID);
	//while(pRes->next())
	//{
	//	const int DungeonBotID = pRes->getInt("BotID");
	//	DoorPosition = vec2(pRes->getInt("PosX"), pRes->getInt("PosY"));
	//	new CLogicDungeonDoorKey(&GS()->m_World, DoorPosition, DungeonBotID);
	//}
}

void CGameControllerDungeon::KillAllPlayers() const
{
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		CCharacter* pCharacter = GS()->GetPlayerChar(i);
		if(pCharacter && GS()->IsPlayerInWorld(i, m_WorldID))
			pCharacter->Die(i, WEAPON_WORLD);
	}
}

void CGameControllerDungeon::ChangeState(int State)
{
	m_StateDungeon = State;
	//CDungeonData::ms_aDungeon[m_DungeonID].m_State = State;

	// - - - - - - - - - - - - - - - - - - - - - -
	// used when changing state to waiting
	if(State == DUNGEON_WAITING)
	{
		m_MaximumTick = 0;
		m_FinishedTick = 0;
		m_StartingTick = 0;
		m_LastStartingTick = 0;
		m_SafeTick = 0;

		//CDungeonData::ms_aDungeon[m_DungeonID].m_Progress = 0;
		SetMobsSpawn(false);
		ResetDoorKeyState();
	}

	// - - - - - - - - - - - - - - - - - - - - - -
	// used when changing state to waiting start
	else if(State == DUNGEON_WAITING_START)
	{
		m_SyncDungeon = GetSyncFactor();
		m_StartingTick = Server()->TickSpeed() * g_Config.m_SvDungeonWaitingTime;

		SetMobsSpawn(false);
	}

	// - - - - - - - - - - - - - - - - - - - - - -
	// used when changing state to start
	else if(State == DUNGEON_STARTED)
	{
		m_ActivePlayers = PlayersNum();
		m_MaximumTick = Server()->TickSpeed() * 600;
		m_SafeTick = Server()->TickSpeed() * 30;

		GS()->ChatWorld(m_WorldID, "Dungeon:", "The security timer is enabled for 30 seconds!");
		GS()->ChatWorld(m_WorldID, "Dungeon:", "You are given 10 minutes to complete of dungeon!");
		GS()->BroadcastWorld(m_WorldID, BroadcastPriority::VeryImportant, 500, "Dungeon started!");

		SetMobsSpawn(true);
		KillAllPlayers();
	}

	// - - - - - - - - - - - - - - - - - - - - - -
	// used when changing state to waiting finish
	else if(State == DUNGEON_WAITING_FINISH)
	{
		m_SafeTick = 0;
		m_FinishedTick = Server()->TickSpeed() * 20;

		SetMobsSpawn(false);

		dynamic_string Buffer;
		int FinishTime = -1;
		int BestPassageHelp = 0;

		// dungeon finished information
		char aTimeFormat[64];
		str_format(aTimeFormat, sizeof(aTimeFormat), "Time: '%d minute(s) %d second(s)'.", FinishTime / 60, FinishTime - (FinishTime / 60 * 60));
		GS()->Chat(-1, "Group{}!", Buffer.buffer());
		//GS()->Chat(-1, "'{}' finished {}!", CDungeonData::ms_aDungeon[m_DungeonID].m_aName, aTimeFormat);
	}

	// - - - - - - - - - - - - - - - - - - - - - -
	// used when changing state to finished
	else if(State == DUNGEON_FINISHED)
	{
		SetMobsSpawn(false);
		KillAllPlayers();
	}

	// door state
	m_DungeonDoor->SetState(State);
}

void CGameControllerDungeon::StateTick()
{
	const int Players = PlayersNum();
	//CDungeonData::ms_aDungeon[m_DungeonID].m_Players = Players;

	// - - - - - - - - - - - - - - - - - - - - - -
	// dungeon
	if(Players < 1 && m_StateDungeon != DUNGEON_WAITING)
		ChangeState(DUNGEON_WAITING);

	// - - - - - - - - - - - - - - - - - - - - - -
	// used in tick when waiting
	if(m_StateDungeon == DUNGEON_WAITING)
	{
		if(Players >= 1)
			ChangeState(DUNGEON_WAITING_START);
	}

	// - - - - - - - - - - - - - - - - - - - - - -
	// used in the tick when the waiting started
	else if(m_StateDungeon == DUNGEON_WAITING_START)
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
			GS()->BroadcastWorld(m_WorldID, BroadcastPriority::VeryImportant, 500, "Dungeon waiting {} sec!\nPlayer's are ready to start right now {} of {}!\nYou can change state with 'Vote yes'", Time, PlayersReadyState, Players);

			m_StartingTick--;
			if(!m_StartingTick)
			{
				m_ActivePlayers = Players;
				ChangeState(DUNGEON_STARTED);
			}
		}
		m_ShiftRoundStartTick = Server()->Tick();
	}

	// - - - - - - - - - - - - - - - - - - - - - -
	// used in tick when dunegon is started
	else if(m_StateDungeon == DUNGEON_STARTED)
	{
		// update players time
		for(int i = 0; i < MAX_PLAYERS; i++)
		{
			CPlayer* pPlayer = GS()->GetPlayer(i);
			if(!pPlayer || !GS()->IsPlayerInWorld(i, m_WorldID))
				continue;

			pPlayer->GetTempData().m_TempTimeDungeon++;
		}

		// security tick during which time the player will not return to the old world
		if(m_SafeTick)
		{
			m_SafeTick--;
			if(!m_SafeTick)
				GS()->ChatWorld(m_WorldID, "Dungeon:", "The security timer is over, be careful!");
		}

		// finish the dungeon when the dungeon is successfully completed
		if(LeftMobsToWin() <= 0)
			ChangeState(DUNGEON_WAITING_FINISH);
	}

	// - - - - - - - - - - - - - - - - - - - - - -
	// used in the tick when the waiting is finished
	else if(m_StateDungeon == DUNGEON_WAITING_FINISH)
	{
		if(m_FinishedTick)
		{
			const int Time = m_FinishedTick / Server()->TickSpeed();
			GS()->BroadcastWorld(m_WorldID, BroadcastPriority::VeryImportant, 500, "Dungeon ended {} sec!", Time);

			m_FinishedTick--;
		}

		if(!m_FinishedTick)
			ChangeState(DUNGEON_FINISHED);
	}
}

void CGameControllerDungeon::OnCharacterDeath(CPlayer* pVictim, CPlayer* pKiller, int Weapon)
{
	IGameController::OnCharacterDeath(pVictim, pKiller, Weapon);

	if(pVictim->IsBot())
	{
		CPlayerBot* pVictimBot = static_cast<CPlayerBot*>(pVictim);

		if(m_StateDungeon >= DUNGEON_STARTED)
		{
			pVictimBot->SetAllowedSpawn(false);
		}

		if(pKiller->GetCID() != pVictim->GetCID() && pVictimBot->GetBotType() == TYPE_BOT_MOB)
		{
			const int Progress = 100 - translate_to_percent(CountMobs(), LeftMobsToWin());
			//CDungeonData::ms_aDungeon[m_DungeonID].m_Progress = Progress;
			GS()->ChatWorld(m_WorldID, "Dungeon:", "The dungeon is completed on [{}%]", Progress);
			UpdateDoorKeyState();
		}

	}
}

bool CGameControllerDungeon::OnCharacterSpawn(CCharacter* pChr)
{
	if(m_StateDungeon >= DUNGEON_STARTED)
	{
		const int ClientID = pChr->GetPlayer()->GetCID();

		// update tanking client status
		if(pChr->GetPlayer()->Account()->GetActiveProfessionID() == ProfessionIdentifier::Tank)
			pChr->GetPlayer()->m_MoodState = Mood::Tank;

		// player died after the safety timer ended
		if(!m_SafeTick)
		{
			GS()->Chat(ClientID, "You were thrown out of dungeon!");

			const int LatestCorrectWorldID = GS()->Core()->AccountManager()->GetLastVisitedWorldID(pChr->GetPlayer());
			pChr->GetPlayer()->ChangeWorld(LatestCorrectWorldID);
			return false;
		}
	}
	else
	{
		// update vote menu for player
		for(int i = 0; i < MAX_PLAYERS; i++)
		{
			CPlayer* pPlayer = GS()->GetPlayer(i);
			if(!pPlayer || !GS()->IsPlayerInWorld(i, m_WorldID))
				continue;

			pPlayer->m_VotesData.UpdateVotesIf(MENU_DUNGEONS);
		}
	}

	IGameController::OnCharacterSpawn(pChr);
	return true;
}

void CGameControllerDungeon::UpdateDoorKeyState()
{
	for(CLogicDungeonDoorKey* pDoor = (CLogicDungeonDoorKey*)GS()->m_World.FindFirst(CGameWorld::ENTTYPE_DUNGEON_PROGRESS_DOOR);
		pDoor; pDoor = (CLogicDungeonDoorKey*)pDoor->TypeNext())
	{
		if(pDoor->SyncStateChanges())
			GS()->ChatWorld(m_WorldID, "Dungeon:", "Door creaking.. Opened door somewhere!");
	}
}

void CGameControllerDungeon::ResetDoorKeyState()
{
	for(CLogicDungeonDoorKey* pDoor = (CLogicDungeonDoorKey*)GS()->m_World.FindFirst(CGameWorld::ENTTYPE_DUNGEON_PROGRESS_DOOR);
		pDoor; pDoor = (CLogicDungeonDoorKey*)pDoor->TypeNext())
		pDoor->ResetDoor();
}

int CGameControllerDungeon::CountMobs() const
{
	int CountMobs = 0;
	for(int i = MAX_PLAYERS; i < MAX_CLIENTS; i++)
	{
		CPlayerBot* BotPlayer = static_cast<CPlayerBot*>(GS()->GetPlayer(i));
		if(BotPlayer && BotPlayer->GetBotType() == TYPE_BOT_MOB && m_WorldID == BotPlayer->GetCurrentWorldID())
			CountMobs++;
	}
	return CountMobs;
}

int CGameControllerDungeon::PlayersReady() const
{
    int ReadyPlayers = 0;
    for(int i = 0; i < MAX_PLAYERS; i++)
    {
        CPlayer* pPlayer = GS()->GetPlayer(i);
        if(pPlayer && GS()->IsPlayerInWorld(i, m_WorldID) && pPlayer->GetTempData().m_TempDungeonReady)
        {
            ReadyPlayers++;
        }
    }
    return ReadyPlayers;
}

int CGameControllerDungeon::PlayersNum() const
{
	int ActivePlayers = 0;
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		if(Server()->GetClientWorldID(i) == m_WorldID)
			ActivePlayers++;
	}
	return ActivePlayers;
}

int CGameControllerDungeon::LeftMobsToWin() const
{
	int LeftMobs = 0;
	for(int i = MAX_PLAYERS; i < MAX_CLIENTS; i++)
	{
		CPlayerBot* BotPlayer = dynamic_cast<CPlayerBot*>(GS()->GetPlayer(i));
		if(BotPlayer && BotPlayer->GetBotType() == TYPE_BOT_MOB && BotPlayer->GetCharacter() && m_WorldID == BotPlayer->GetCurrentWorldID())
			LeftMobs++;
	}
	return LeftMobs;
}

void CGameControllerDungeon::SetMobsSpawn(bool AllowedSpawn)
{
	for(int i = MAX_PLAYERS; i < MAX_CLIENTS; i++)
	{
		CPlayerBot* BotPlayer = dynamic_cast<CPlayerBot*>(GS()->GetPlayer(i));
		if(BotPlayer && BotPlayer->GetBotType() == TYPE_BOT_MOB && m_WorldID == BotPlayer->GetCurrentWorldID())
		{
			BotPlayer->SetAllowedSpawn(AllowedSpawn);
			if(!AllowedSpawn && BotPlayer->GetCharacter())
				BotPlayer->GetCharacter()->Die(i, WEAPON_WORLD);
		}
	}
}

// TODO: something to do with the balance
int CGameControllerDungeon::GetSyncFactor() const
{
	int MaxFactor = 0;
	int MinFactor = std::numeric_limits<int>::max();
	int BotCount = 0;

	for(int i = MAX_PLAYERS; i < MAX_CLIENTS; i++)
	{
		CPlayerBot* pBotPlayer = dynamic_cast<CPlayerBot*>(GS()->GetPlayer(i));
		if(pBotPlayer && pBotPlayer->GetBotType() == TYPE_BOT_MOB && pBotPlayer->GetCurrentWorldID() == m_WorldID)
		{
			const int LevelDisciple = pBotPlayer->GetTotalAttributes();
			MinFactor = minimum(MinFactor, LevelDisciple);
			MaxFactor = maximum(MaxFactor, LevelDisciple);
			BotCount++;
		}
	}

	if(BotCount == 0)
	{
		return 0; // No bot's, return default value
	}

	return (MaxFactor + MinFactor) / 2;
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
			ChangeState(DUNGEON_FINISHED);
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

DungeonDoor::DungeonDoor(CGameWorld* pGameWorld, vec2 Pos)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_DUNGEON_DOOR, Pos)
{
	GS()->Collision()->FillLengthWall(32, vec2(0, -1), &m_Pos, &m_PosTo);
	m_State = DUNGEON_WAITING;

	GameWorld()->InsertEntity(this);
}

void DungeonDoor::Tick()
{
	if(m_State >= DUNGEON_STARTED)
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

void DungeonDoor::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient) || m_State >= DUNGEON_STARTED)
		return;

	CNetObj_Laser* pObj = static_cast<CNetObj_Laser*>(Server()->SnapNewItem(NETOBJTYPE_LASER, GetID(), sizeof(CNetObj_Laser)));
	if(!pObj)
		return;

	pObj->m_X = int(m_Pos.x);
	pObj->m_Y = int(m_Pos.y);
	pObj->m_FromX = int(m_PosTo.x);
	pObj->m_FromY = int(m_PosTo.y);
	pObj->m_StartTick = Server()->Tick() - 2;
}