/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_PLAYER_BOT_H
#define GAME_SERVER_PLAYER_BOT_H

#include "player.h"

// forward declarations
struct PathResult;

class CPlayerBot : public CPlayer
{
	MACRO_ALLOC_POOL_ID()

	int m_BotType;
	int m_BotID;
	int m_MobID;
	int m_BotHealth;
	int m_BotStartHealth;
	bool m_DisabledBotDamage;
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

	PathRequestHandle m_PathHandle;

	CPlayerBot(CGS* pGS, int ClientID, int BotID, int MobID, int SpawnPoint);
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

	int GetStartHealth() const override { return m_BotStartHealth; };
	int GetHealth() const override { return m_BotHealth; }
	int GetMana() const override { return 999; }

	void HandleTuningParams() override;
	void UpdateTempData(int Health, int Mana) override { m_BotHealth = Health; }

	int64_t GetMaskVisibleForClients() const override;
	StateSnapping IsActiveForClient(int ClientID) const override;
	std::optional<int> GetEquippedItemID(ItemFunctional EquipID, int SkipItemID = -1) const override;
	int GetTotalAttributeValue(AttributeIdentifier ID) const override;

	bool GiveEffect(const char* Potion, int Sec, float Chance = 100.0f) override;
	bool IsActiveEffect(const char* Potion) const override;
	void ClearEffects() override;

	void Tick() override;
	void PostTick() override;
	void Snap(int SnappingClient) override;
	void FakeSnap() override;

	bool IsActive() const override;
	bool IsConversational() const;
	void PrepareRespawnTick() override;

	void SetDungeonAllowedSpawn(bool Spawn) { m_DungeonAllowedSpawn = Spawn; }

	int m_EidolonItemID;
	class CPlayer* GetEidolonOwner() const;
	bool IsDisabledBotDamage() const { return m_DisabledBotDamage; }

	class CPlayerItem* GetItem(ItemIdentifier ID) override;

private:
	ska::unordered_map< int, std::unique_ptr<CPlayerItem> > m_Items {};
	ska::unordered_map < std::string /* effect */, int /* seconds */ > m_aEffects;

	void GetFormatedName(char* aBuffer, int BufferSize) override;
	int GetBotLevel() const;
	Mood GetMoodState() const override;
	const char* GetStatus() const;

	void TryRespawn() override;
	void HandleEffects() override;

	/***********************************************************************************/
	/*  Thread path finderdon't want to secure m_TargetPos, or m_WayPoints with mutex  */
	/***********************************************************************************/
	void HandlePathFinder();
};

#endif
