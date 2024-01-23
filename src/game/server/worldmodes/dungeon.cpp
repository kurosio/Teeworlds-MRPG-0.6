/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "dungeon.h"

#include <engine/shared/config.h>
#include <game/server/entity.h>
#include <game/server/gamecontext.h>

#include <game/server/core/entities/botwall.h>
#include <game/server/core/entities/Logics/logicwall.h>
#include <teeother/system/string.h>

#include <game/server/core/components/Accounts/AccountManager.h>
#include <game/server/core/components/Dungeons/DungeonManager.h>

CGameControllerDungeon::CGameControllerDungeon(class CGS* pGS) : IGameController(pGS)
{
	m_GameFlags = 0;
	m_DungeonID = GS()->GetDungeonID();
	m_WorldID = GS()->GetWorldID();

	m_ActivePlayers = 0;
	m_TankClientID = -1;

	// door creation to start
	vec2 DoorPosition = vec2(CDungeonData::ms_aDungeon[m_DungeonID].m_DoorX, CDungeonData::ms_aDungeon[m_DungeonID].m_DoorY);
	m_DungeonDoor = new DungeonDoor(&GS()->m_World, DoorPosition);
	ChangeState(DUNGEON_WAITING);

	// key door construction
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_dungeons_door", "WHERE DungeonID = '%d'", m_DungeonID);
	while(pRes->next())
	{
		const int DungeonBotID = pRes->getInt("BotID");
		DoorPosition = vec2(pRes->getInt("PosX"), pRes->getInt("PosY"));
		new CLogicDungeonDoorKey(&GS()->m_World, DoorPosition, DungeonBotID);
	}
}

void CGameControllerDungeon::KillAllPlayers() const
{
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		CCharacter* pCharacter = GS()->GetPlayerChar(i);
		if(pCharacter && GS()->IsPlayerEqualWorld(i, m_WorldID))
			pCharacter->Die(i, WEAPON_WORLD);
	}
}

void CGameControllerDungeon::ChangeState(int State)
{
	m_StateDungeon = State;
	CDungeonData::ms_aDungeon[m_DungeonID].m_State = State;

	// - - - - - - - - - - - - - - - - - - - - - -
	// used when changing state to waiting
	if(State == DUNGEON_WAITING)
	{
		m_MaximumTick = 0;
		m_FinishedTick = 0;
		m_StartingTick = 0;
		m_LastStartingTick = 0;
		m_SafeTick = 0;

		CDungeonData::ms_aDungeon[m_DungeonID].m_Progress = 0;
		m_TankClientID = -1;

		SetMobsSpawn(false);
		ResetDoorKeyState();
	}

	// - - - - - - - - - - - - - - - - - - - - - -
	// used when changing state to waiting start
	else if(State == DUNGEON_WAITING_START)
	{
		m_SyncDungeon = GetSyncFactor();
		m_StartingTick = Server()->TickSpeed() * g_Config.m_SvTimeWaitingsDungeon;

		SetMobsSpawn(false);
	}

	// - - - - - - - - - - - - - - - - - - - - - -
	// used when changing state to start
	else if(State == DUNGEON_STARTED)
	{
		SelectTankPlayer();

		m_ActivePlayers = PlayersNum();
		m_MaximumTick = Server()->TickSpeed() * 600;
		m_SafeTick = Server()->TickSpeed() * 30;

		GS()->ChatWorldID(m_WorldID, "Dungeon:", "The security timer is enabled for 30 seconds!");
		GS()->ChatWorldID(m_WorldID, "Dungeon:", "You are given 10 minutes to complete of dungeon!");
		GS()->BroadcastWorldID(m_WorldID, BroadcastPriority::VERY_IMPORTANT, 500, "Dungeon started!");

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
		CPlayer* pBestPlayer = nullptr;

		for(int i = 0; i < MAX_PLAYERS; i++)
		{
			CPlayer* pPlayer = GS()->m_apPlayers[i];
			if(!pPlayer || !GS()->IsPlayerEqualWorld(i, m_WorldID))
				continue;

			// update finish time int sec
			m_Records[i].m_Time = GS()->m_apPlayers[i]->GetTempData().m_TempTimeDungeon / Server()->TickSpeed();
			FinishTime = m_Records[i].m_Time;

			// search best passage help
			if(m_Records[i].m_PassageHelp > BestPassageHelp)
			{
				BestPassageHelp = m_Records[i].m_PassageHelp;
				pBestPlayer = pPlayer;
			}

			// add names to group
			Buffer.append(", ");
			Buffer.append(Server()->ClientName(i));

			// save record and reset time for client
			GS()->Core()->DungeonManager()->SaveDungeonRecord(pPlayer, m_DungeonID, &m_Records[i]);
			GS()->m_apPlayers[i]->GetTempData().m_TempTimeDungeon = 0;
			m_Records[i].Reset();
		}

		// dungeon finished information
		char aTimeFormat[64];
		str_format(aTimeFormat, sizeof(aTimeFormat), "Time: %d minute(s) %d second(s)", FinishTime / 60, FinishTime - (FinishTime / 60 * 60));
		GS()->Chat(-1, "Group{STR}!", Buffer.buffer());
		GS()->Chat(-1, "{STR} finished {STR}!", CDungeonData::ms_aDungeon[m_DungeonID].m_aName, aTimeFormat);

		if(pBestPlayer)
			GS()->Chat(-1, "Most Valuable '{STR}'. With help {VAL} points.", Server()->ClientName(pBestPlayer->GetCID()), BestPassageHelp);
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
	CDungeonData::ms_aDungeon[m_DungeonID].m_Players = Players;

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
			GS()->BroadcastWorldID(m_WorldID, BroadcastPriority::VERY_IMPORTANT, 500, "Dungeon waiting {INT} sec!\nPlayer's are ready to start right now {INT} of {INT}!\nYou can change state with 'Vote yes'", Time, PlayersReadyState, Players);

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
			CPlayer* pPlayer = GS()->m_apPlayers[i];
			if(!pPlayer || !GS()->IsPlayerEqualWorld(i, m_WorldID))
				continue;

			pPlayer->GetTempData().m_TempTimeDungeon++;
		}

		// security tick during which time the player will not return to the old world
		if(m_SafeTick)
		{
			m_SafeTick--;
			if(!m_SafeTick)
				GS()->ChatWorldID(m_WorldID, "Dungeon:", "The security timer is over, be careful!");
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
			GS()->BroadcastWorldID(m_WorldID, BroadcastPriority::VERY_IMPORTANT, 500, "Dungeon ended {INT} sec!", Time);

			m_FinishedTick--;
		}

		if(!m_FinishedTick)
			ChangeState(DUNGEON_FINISHED);
	}
}

void CGameControllerDungeon::OnCharacterDamage(CPlayer* pFrom, CPlayer* pTo, int Damage)
{
	if(!pFrom || !pTo || Damage <= 0)
		return;

	/*
	 * Commentary, as the tank has weak damage. Then his help will be counted for his received damage.
	 */

	// if it's tank passage get how got size damage
	if(pFrom->IsBot() && pTo->m_MoodState == Mood::TANK)
	{
		const int ClientID = pTo->GetCID();
		m_Records[ClientID].m_PassageHelp += Damage;
	}

	// if it's not tank passage from size damage + health
	if(!pFrom->IsBot() && pTo->IsBot())
	{
		const int ClientID = pFrom->GetCID();
		m_Records[ClientID].m_PassageHelp += Damage;
	}
}

void CGameControllerDungeon::OnCharacterDeath(CPlayer* pVictim, CPlayer* pKiller, int Weapon)
{
	IGameController::OnCharacterDeath(pVictim, pKiller, Weapon);

	if(pVictim->IsBot())
	{
		CPlayerBot* pVictimBot = static_cast<CPlayerBot*>(pVictim);
		if(pKiller->GetCID() != pVictim->GetCID() && pVictimBot->GetBotType() == TYPE_BOT_MOB)
		{
			const int Progress = 100 - translate_to_percent(CountMobs(), LeftMobsToWin());
			CDungeonData::ms_aDungeon[m_DungeonID].m_Progress = Progress;
			GS()->ChatWorldID(m_WorldID, "Dungeon:", "The dungeon is completed on [{INT}%]", Progress);
			UpdateDoorKeyState();
		}
	}
}

bool CGameControllerDungeon::OnCharacterSpawn(CCharacter* pChr)
{
	if(!pChr->GetPlayer()->IsBot())
	{
		if(m_StateDungeon >= DUNGEON_STARTED)
		{
			const int ClientID = pChr->GetPlayer()->GetCID();

			// update tanking client status
			if(ClientID == m_TankClientID)
				pChr->GetPlayer()->m_MoodState = Mood::TANK;

			// player died after the safety timer ended
			if(!m_SafeTick)
			{
				GS()->Chat(ClientID, "You were thrown out of dungeon!");

				const int LatestCorrectWorldID = GS()->Core()->AccountManager()->GetHistoryLatestCorrectWorldID(pChr->GetPlayer());
				pChr->GetPlayer()->ChangeWorld(LatestCorrectWorldID);
				return false;
			}
		}
		else
		{
			// update vote menu for players
			for(int i = 0; i < MAX_PLAYERS; i++)
			{
				if(!GS()->m_apPlayers[i] || !GS()->IsPlayerEqualWorld(i, m_WorldID))
					continue;

				GS()->StrongUpdateVotes(i, MENU_DUNGEONS);
			}
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
			GS()->ChatWorldID(m_WorldID, "Dungeon:", "Door creaking.. Opened door somewhere!");
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
		CPlayerBot* BotPlayer = static_cast<CPlayerBot*>(GS()->m_apPlayers[i]);
		if(BotPlayer && BotPlayer->GetBotType() == TYPE_BOT_MOB && m_WorldID == BotPlayer->GetPlayerWorldID())
			CountMobs++;
	}
	return CountMobs;
}

int CGameControllerDungeon::PlayersReady() const
{
	int ReadyPlayers = 0;
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		if(!GS()->m_apPlayers[i] || !GS()->IsPlayerEqualWorld(i, m_WorldID) || !GS()->m_apPlayers[i]->GetTempData().m_TempDungeonReady)
			continue;
		ReadyPlayers++;
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
		CPlayerBot* BotPlayer = static_cast<CPlayerBot*>(GS()->m_apPlayers[i]);
		if(BotPlayer && BotPlayer->GetBotType() == TYPE_BOT_MOB && BotPlayer->GetCharacter() && m_WorldID == BotPlayer->GetPlayerWorldID())
			LeftMobs++;
	}
	return LeftMobs;
}

void CGameControllerDungeon::SetMobsSpawn(bool AllowedSpawn)
{
	for(int i = MAX_PLAYERS; i < MAX_CLIENTS; i++)
	{
		CPlayerBot* BotPlayer = static_cast<CPlayerBot*>(GS()->m_apPlayers[i]);
		if(BotPlayer && BotPlayer->GetBotType() == TYPE_BOT_MOB && m_WorldID == BotPlayer->GetPlayerWorldID())
		{
			BotPlayer->SetDungeonAllowedSpawn(AllowedSpawn);
			if(!AllowedSpawn && BotPlayer->GetCharacter())
				BotPlayer->GetCharacter()->Die(i, WEAPON_WORLD);
		}
	}
}

void CGameControllerDungeon::SelectTankPlayer()
{
	int MaximalVotes = 0;
	int MaximalHardness = 0;
	bool ChosenByPlayers = false;

	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		CPlayer* pPlayer = GS()->m_apPlayers[i];
		if(!pPlayer || Server()->GetClientWorldID(i) != m_WorldID)
			continue;

		// random if the votes of several players are equal
		if(MaximalVotes > 0 && pPlayer->GetTempData().m_TempTankVotingDungeon == MaximalVotes && rand() % 2 == 0)
			m_TankClientID = i;

		// the player is selected by votes
		if(pPlayer->GetTempData().m_TempTankVotingDungeon > MaximalVotes)
		{
			ChosenByPlayers = true;

			m_TankClientID = i;
			MaximalVotes = pPlayer->GetTempData().m_TempTankVotingDungeon;
		}

		// selection by hardness statistics, if there are no votes
		if(MaximalVotes <= 0 && pPlayer->GetTypeAttributesSize(AttributeGroup::Tank) > MaximalHardness)
		{
			m_TankClientID = i;
			MaximalHardness = pPlayer->GetTypeAttributesSize(AttributeGroup::Tank);
		}
	}

	// show information about class selection
	CPlayer* pTankPlayer = GS()->GetPlayer(m_TankClientID, true);
	if(pTankPlayer)
	{
		if(ChosenByPlayers)
		{
			GS()->ChatWorldID(m_WorldID, "Dungeon:", "Tank is assigned to '{STR}' with {INT} votes!",
				Server()->ClientName(m_TankClientID), pTankPlayer->GetTempData().m_TempTankVotingDungeon);
		}
		else
		{
			const int StrengthTank = pTankPlayer->GetTypeAttributesSize(AttributeGroup::Tank);
			GS()->ChatWorldID(m_WorldID, "Dungeon:", "Tank '{STR}' assigned with class strength {VAL}p!",
				Server()->ClientName(m_TankClientID), StrengthTank);
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
		CPlayerBot* pBotPlayer = static_cast<CPlayerBot*>(GS()->m_apPlayers[i]);
		if(pBotPlayer && pBotPlayer->GetBotType() == TYPE_BOT_MOB && pBotPlayer->GetPlayerWorldID() == m_WorldID)
		{
			const int LevelDisciple = pBotPlayer->GetAttributesSize();
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

int CGameControllerDungeon::GetAttributeDungeonSync(const CPlayer* pPlayer, AttributeIdentifier ID) const
{
	float Percent = 0.0f;
	const AttributeGroup Type = GS()->GetAttributeInfo(ID)->GetGroup();

	// - - - - - - - - -- - - -
	// balance tanks
	if(pPlayer->m_MoodState == Mood::TANK)
	{
		const float ActiveAttribute = m_SyncDungeon / 2.0f;
		if(Type == AttributeGroup::Tank)
			Percent = 50.0f;

		// very low damage for tank
		if(Type == AttributeGroup::Hardtype && ID != AttributeIdentifier::DMG)
			return 0;

		const int AttributeSyncProcent = translate_to_percent_rest(ActiveAttribute, Percent);
		return maximum(AttributeSyncProcent, 1);
	}

	// - - - - - - - - -- - - -
	// balance healer damage divides the average attribute into the number of players
	const float ActiveAttribute = m_SyncDungeon / m_ActivePlayers;
	if(Type == AttributeGroup::Healer)
		Percent = minimum(25.0f + (m_ActivePlayers * 2.0f), 50.0f);
	else if(Type == AttributeGroup::Tank)
		Percent = 5.0f;
	else if(Type == AttributeGroup::Hardtype || Type == AttributeGroup::Dps)
		Percent = 0.1f;

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

void CGameControllerDungeon::CreateLogic(int Type, int Mode, vec2 Pos, int ParseInt)
{
	if(Type == 1)
		new CLogicWall(&GS()->m_World, Pos);

	if(Type == 2)
		new CLogicWallWall(&GS()->m_World, Pos, Mode, ParseInt);

	if(Type == 3)
		new CLogicDoorKey(&GS()->m_World, Pos, ParseInt, Mode);
}

bool CGameControllerDungeon::OnEntity(int Index, vec2 Pos)
{
	if(IGameController::OnEntity(Index, Pos))
		return true;

	return false;
}

DungeonDoor::DungeonDoor(CGameWorld* pGameWorld, vec2 Pos)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_DUNGEON_DOOR, Pos)
{
	GS()->Collision()->Wallline(32, vec2(0, -1), &m_Pos, &m_PosTo);
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
				pChar->m_DoorHit = true;
				pChar->Die(pChar->GetPlayer()->GetCID(), WEAPON_WORLD);
			}
		}
	}
}

void DungeonDoor::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient, true) || m_State >= DUNGEON_STARTED)
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