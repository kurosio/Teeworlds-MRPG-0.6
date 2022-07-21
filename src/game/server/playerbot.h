/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_PLAYER_BOT_H
#define GAME_SERVER_PLAYER_BOT_H

#include "player.h"

class CPlayerBot : public CPlayer
{
	MACRO_ALLOC_POOL_ID()

	int m_BotType;
	int m_BotID;
	int m_SubBotID;
	int m_BotHealth;
	int m_DungeonAllowedSpawn;
	std::map<int, vec2> m_WayPoints;

public:
	int m_LastPosTick;
	int m_PathSize;
	vec2 m_TargetPos;
	std::atomic_bool m_ThreadReadNow;

	CPlayerBot(CGS *pGS, int ClientID, int BotID, int SubBotID, int SpawnPoint);
	~CPlayerBot() override;

	vec2& GetWayPoint(int Index) { return m_WayPoints[Index]; }
	static void FindThreadPath(CGS* pGameServer, CPlayerBot* pBotPlayer, vec2 StartPos, vec2 SearchPos);
	static void GetThreadRandomWaypointTarget(CGS* pGameServer, CPlayerBot* pBotPlayer);
	void ClearWayPoint();

	int GetTeam() override { return TEAM_BLUE; }
	bool IsBot() const override { return true; }
	int GetBotID() const override { return m_BotID; }
	int GetBotType() const override { return m_BotType; }
	int GetBotSub() const override { return m_SubBotID; }
	int GetPlayerWorldID() const override;

	int GetStartHealth() override;
	int GetHealth() override { return m_BotHealth; }
	int GetMana() override { return 999; }

	void HandleTuningParams() override;
	void UpdateTempData(int Health, int Mana) override { m_BotHealth = Health; }

	int64 GetMaskVisibleForClients() const override;
	int IsVisibleForClient(int ClientID) const override;
	int GetEquippedItemID(int EquipID, int SkipItemID = -1) const override;
	int GetAttributeCount(int BonusID, bool Really = false) override;

	void GiveEffect(const char* Potion, int Sec, float Chance = 100.0f) override;
	bool IsActiveEffect(const char* Potion) const override;
	void ClearEffects() override;

	void Tick() override;
	void PostTick() override;
	void Snap(int SnappingClient) override;
	void SetDungeonAllowedSpawn(bool Spawn) { m_DungeonAllowedSpawn = Spawn; }

private:
	std::unordered_map < std::string /* effect */, int /* seconds */ > m_aEffects;
	void EffectsTick() override;
	void TryRespawn() override;

	int GetBotLevel() const;
	const char* GetStatusBot() const;
	int GetMoodState(int SnappingClient) const;
	bool IsActiveQuests(int SnapClientID) const;
	void GenerateNick(char* buffer, int size_buffer) const;

	/***********************************************************************************/
	/*  Thread path finderdon't want to secure m_TargetPos, or m_WayPoints with mutex  */
	/***********************************************************************************/
	void ThreadMobsPathFinder();
};

#endif
