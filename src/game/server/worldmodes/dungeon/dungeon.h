/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_WORLDMODES_DUNGEON_DUNGEON_H
#define GAME_SERVER_WORLDMODES_DUNGEON_DUNGEON_H

#include <game/server/entity.h>
#include <game/server/worldmodes/default.h>
#include <game/server/core/components/duties/dungeon_data.h>

class CEntityDungeonWaitingDoor;

class CGameControllerDungeon : public CGameControllerDefault
{
	CDungeonData* m_pDungeon {};
	CEntityDungeonWaitingDoor* m_pEntWaitingDoor {};

	int m_ScenarioID {};
	int m_StartedPlayersNum {};
	int m_LastWarmupTick {};
	int m_WarmupTick {};
	int m_SafetyTick {};
	int m_EndTick {};
	int m_ShiftRoundStartTick {};
	bool m_EjectPlayersOnSpawn {};

public:
	CGameControllerDungeon(class CGS* pGS, CDungeonData* pDungeon);
	~CGameControllerDungeon() override;

	void Tick() override;
	void Snap() override;

	void OnCharacterDeath(class CPlayer* pVictim, class CPlayer* pKiller, int Weapon) override;
	bool OnCharacterSpawn(class CCharacter* pChr) override;

	CDungeonData* GetDungeon() const { return m_pDungeon; }
	void FinishDungeon();

private:
	int GetPlayersReadyNum() const;
	int GetPlayersNum() const;

	void ChangeState(int State);
	void Process();
	void KillAllPlayers() const;
	void SaveDungeonRecord(const class CPlayer* pPlayer) const;
};

#endif
