/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMEMODES_DUNGEON_H
#define GAME_SERVER_GAMEMODES_DUNGEON_H

#include <game/server/entity.h>
#include <game/server/gamecontroller.h>

#include <game/server/mmocore/Components/Dungeons/DungeonData.h>

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
	DungeonDoor *m_DungeonDoor;
	int m_StateDungeon;
	int m_DungeonID;
	int m_WorldID;

	int m_ActivePlayers;
	int m_TankClientID;
	int m_SyncDungeon;

	int m_LastStartingTick;
	int m_StartingTick;
	int m_FinishedTick;
	int m_SafeTick;
	int m_MaximumTick;

	CPlayerDungeonRecord m_Records[MAX_PLAYERS];

public:
	CGameControllerDungeon(class CGS* pGameServer);

	void Tick() override;
	bool OnEntity(int Index, vec2 Pos) override;
	void OnCharacterDamage(CPlayer* pFrom, CPlayer* pTo, int Damage) override;
	void OnCharacterDeath(class CCharacter* pVictim, class CPlayer* pKiller, int Weapon) override;
	bool OnCharacterSpawn(class CCharacter* pChr) override;
	void CreateLogic(int Type, int Mode, vec2 Pos, int ParseID) override;
	int GetAttributeDungeonSync(class CPlayer* pPlayer, int BonusID) const;

private:
	int PlayersReady() const;
	int PlayersNum() const;
	int LeftMobsToWin() const;
	int CountMobs() const;

	void ChangeState(int State);
	void StateTick();
	void SetMobsSpawn(bool AllowedSpawn);
	void KillAllPlayers();

	void UpdateDoorKeyState();
	void ResetDoorKeyState();
	void SelectTankPlayer();

	int GetSyncFactor() const;
};

class DungeonDoor : public CEntity
{
	int m_State;
public:
	DungeonDoor(CGameWorld *pGameWorld, vec2 Pos);

	void SetState(int State) { m_State = State; };
	void Tick() override;
	void Snap(int SnappingClient) override;
};

#endif
