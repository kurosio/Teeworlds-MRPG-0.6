/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMEMODES_DUNGEON_H
#define GAME_SERVER_GAMEMODES_DUNGEON_H

#include <game/server/entity.h>
#include <game/server/gamecontroller.h>

#include <game/server/core/components/dungeons/dungeon_data.h>

enum DungeonState
{
	DUNGEON_WAITING,
	DUNGEON_WAITING_START,
	DUNGEON_STARTED,
	DUNGEON_WAITING_FINISH,
	DUNGEON_FINISHED,
};

class DungeonDoor;
class CGameControllerDungeon : public IGameController
{
	DungeonDoor* m_DungeonDoor {};
	int m_StateDungeon {};
	int m_DungeonID {};
	int m_WorldID {};

	int m_ActivePlayers {};
	int m_TankClientID {};
	int m_SyncDungeon {};

	int m_LastStartingTick {};
	int m_StartingTick {};
	int m_FinishedTick {};
	int m_SafeTick {};
	int m_MaximumTick {};
	int m_ShiftRoundStartTick {};

public:
	CGameControllerDungeon(class CGS* pGameServer);

	void Tick() override;
	void Snap() override;

	void OnCharacterDeath(class CPlayer* pVictim, class CPlayer* pKiller, int Weapon) override;
	bool OnCharacterSpawn(class CCharacter* pChr) override;
	int GetAttributeDungeonSyncByClass(ProfessionIdentifier ProfID, AttributeIdentifier ID) const;
	int GetSyncFactor() const;
	int GetDungeonID() const { return m_DungeonID; }

private:
	int PlayersReady() const;
	int PlayersNum() const;
	int LeftMobsToWin() const;
	int CountMobs() const;

	void ChangeState(int State);
	void StateTick();
	void SetMobsSpawn(bool AllowedSpawn);
	void KillAllPlayers() const;

	void UpdateDoorKeyState();
	void ResetDoorKeyState();
};

class DungeonDoor : public CEntity
{
	int m_State {};
public:
	DungeonDoor(CGameWorld *pGameWorld, vec2 Pos);

	void SetState(int State) { m_State = State; };
	void Tick() override;
	void Snap(int SnappingClient) override;
};

#endif
