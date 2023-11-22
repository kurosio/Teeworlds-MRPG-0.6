/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_PLAYER_BOT_H
#define GAME_SERVER_PLAYER_BOT_H

#include "player.h"

#include "mmocore/PathFinderData.h"

class CPlayerBot : public CPlayer
{
	MACRO_ALLOC_POOL_ID()

	int m_BotType;
	int m_BotID;
	int m_MobID;
	int m_BotHealth;
	int m_BotStartHealth;
	bool m_BotActive;
	int m_DungeonAllowedSpawn;

	struct CQuestBotMobInfo
	{
		int m_QuestID;
		int m_QuestStep;
		int m_MoveToStep;
		int m_AttributePower;
		int m_AttributeSpread;
		int m_WorldID;
		vec2 m_Position;

		bool m_ActiveForClient[MAX_PLAYERS]{};
		bool m_CompleteClient[MAX_PLAYERS]{};
	} m_QuestMobInfo;

public:
	int m_LastPosTick;
	vec2 m_TargetPos;
	vec2 m_OldTargetPos;
	CPathFinderPrepared m_PathFinderData;

	CPlayerBot(CGS* pGS, int ClientID, int BotID, int SubBotID, int SpawnPoint);
	~CPlayerBot() override;

	void InitQuestBotMobInfo(CQuestBotMobInfo elem);
	CQuestBotMobInfo& GetQuestBotMobInfo() { return m_QuestMobInfo; }

	int GetTeam() override { return TEAM_BLUE; }
	bool IsBot() const override { return true; }
	int GetBotID() const override { return m_BotID; }
	int GetBotType() const override { return m_BotType; }
	int GetBotMobID() const override { return m_MobID; }
	int GetPlayerWorldID() const override;
	CTeeInfo& GetTeeInfo() const override;

	int GetStartHealth() override { return m_BotStartHealth; };
	int GetHealth() override { return m_BotHealth; }
	int GetMana() override { return 999; }

	void HandleTuningParams() override;
	void UpdateTempData(int Health, int Mana) override { m_BotHealth = Health; }

	int64 GetMaskVisibleForClients() const override;
	int IsVisibleForClient(int ClientID) const override;
	int GetEquippedItemID(ItemFunctional EquipID, int SkipItemID = -1) const override;
	int GetAttributeSize(AttributeIdentifier ID) override;

	void GiveEffect(const char* Potion, int Sec, float Chance = 100.0f) override;
	bool IsActiveEffect(const char* Potion) const override;
	void ClearEffects() override;

	void Tick() override;
	void PostTick() override;
	void Snap(int SnappingClient) override;
	void FakeSnap() override;

	bool IsActive() const override { return m_BotActive; };
	void ResetRespawnTick();

	void SetDungeonAllowedSpawn(bool Spawn) { m_DungeonAllowedSpawn = Spawn; }

	int m_EidolonItemID;
	class CPlayer* GetEidolonOwner() const;

	class CPlayerItem* GetItem(ItemIdentifier ID) override;

private:
	ska::unordered_map< int, std::unique_ptr<CPlayerItem> > m_Items {};
	ska::unordered_map < std::string /* effect */, int /* seconds */ > m_aEffects;
	void HandleEffects() override;
	void TryRespawn() override;

	int GetBotLevel() const;
	const char* GetStatus() const;
	Mood GetMoodState() const override;
	bool IsActiveQuests(int SnapClientID) const;

	/***********************************************************************************/
	/*  Thread path finderdon't want to secure m_TargetPos, or m_WayPoints with mutex  */
	/***********************************************************************************/
	void HandlePathFinder();
};

#endif
