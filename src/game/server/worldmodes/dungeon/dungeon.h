/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_WORLDMODES_DUNGEON_DUNGEON_H
#define GAME_SERVER_WORLDMODES_DUNGEON_DUNGEON_H

#include <game/server/entity.h>
#include <game/server/gamecontroller.h>
#include <game/server/core/components/duties/dungeon_data.h>

class CEntityDungeonWaitingDoor;
class CEntityDungeonProgressDoor;

class CGameControllerDungeon : public IGameController
{
	CDungeonData* m_pDungeon {};
	CEntityDungeonWaitingDoor* m_pEntWaitingDoor {};
	std::vector< CEntityDungeonProgressDoor* > m_vpEntLogicDoor {};

	std::map<AttributeIdentifier, int> m_vSyncFactor {};
	int m_ScenarioID {};

	int m_StartedPlayersNum {};
	int m_LastWaitingTick {};
	int m_WaitingTick {};
	int m_FinishTick {};
	int m_SafetyTick {};
	int m_EndTick {};
	int m_ShiftRoundStartTick {};

public:
	CGameControllerDungeon(class CGS* pGS, CDungeonData* pDungeon);
	~CGameControllerDungeon() override;

	void Tick() override;
	void Snap() override;

	void OnCharacterDeath(class CPlayer* pVictim, class CPlayer* pKiller, int Weapon) override;
	bool OnCharacterSpawn(class CCharacter* pChr) override;

	void PrepareSyncFactors(std::map<AttributeIdentifier, int>& vResultMap);
	int CalculateMobAttribute(AttributeIdentifier ID, int PowerLevel, float BaseFactor, int MinValue) const;
	int GetAttributeDungeonSync(AttributeIdentifier ID) const;
	void RefreshSyncAttributes();

	const std::map<AttributeIdentifier, int>& GetSyncFactor() const { return m_vSyncFactor; }
	CDungeonData* GetDungeon() const { return m_pDungeon; }

private:
	int GetPlayersReadyNum() const;
	int GetPlayersNum() const;
	int GetRemainingMobsNum() const;
	int GetTotalMobsNum() const;

	void ChangeState(int State);
	void Process();
	void SetMobsSpawn(bool AllowedSpawn);
	void KillAllPlayers() const;

	void UpdateDoorKeyState();
	void ResetDoorKeyState();
};

#endif
