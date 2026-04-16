/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_PLAYER_BOT_H
#define GAME_SERVER_PLAYER_BOT_H

#include "player.h"

// forward declarations
struct PathResult;

class CQuestBotMobInfo
{
public:
	int m_QuestID;
	int m_QuestStep;
	int m_MoveToStep;
	int m_AttributePower;
	int m_WorldID;
	vec2 m_Position;
	bool m_ActiveForClient[MAX_PLAYERS] {};
};

class CPlayerBot : public CPlayer
{
	MACRO_ALLOC_HEAP()

	int m_BotType {};
	int m_BotID {};
	int m_MobID {};
	int m_MaxHealth{};
	int m_Health{};
	int m_MaxMana{};
	int m_Mana{};
	bool m_DisabledBotDamage{};
	CQuestBotMobInfo m_QuestMobInfo{};
	MobBotInfo m_MobInfo {};

public:
	int m_LastPosTick{};
	std::optional<vec2> m_TargetPos{};
	vec2 m_OldTargetPos{};
	PathRequestHandle m_PathHandle{};

	CPlayerBot(CGS* pGS, int ClientID, int BotID, int MobID, int SpawnPoint);
	~CPlayerBot() override;

	void InitBotMobInfo(const MobBotInfo& elem);
	void InitQuestBotMobInfo(const CQuestBotMobInfo& elem);
	CQuestBotMobInfo& GetQuestBotMobInfo() { return m_QuestMobInfo; }
	MobBotInfo& GetMobInfo() { return m_MobInfo; }

	int GetTeam() const override { return TEAM_BLUE; }
	bool IsBot() const override { return true; }
	int GetBotID() const { return m_BotID; }
	int GetBotType() const { return m_BotType; }
	int GetBotMobID() const { return m_MobID; }
	int GetCurrentWorldID() const override;
	const CTeeInfo& GetTeeInfo() const override;

	void InitBasicStats(int StartHP, int StartMP, int MaxHP, int MaxMP);
	int GetMaxHealth() const override { return m_MaxHealth; }
	int GetMaxMana() const override { return m_MaxMana; }
	int GetHealth() const override { return m_Health; }
	int GetMana() const override { return m_Mana; }

	void HandleTuningParams() override;
	void UpdateSharedCharacterData(int Health, int Mana) override
	{
		m_Health = Health;
		m_Mana = Mana;
	}

	int64_t GetMaskVisibleForClients() const override;
	ESnappingPriority IsActiveForClient(int ClientID) const override;
	bool IsVisibleForClient(int ClientID) const;
	bool IsVisibleForClient(int ClientID, ESnappingPriority RequiredPriority) const;
	std::optional<int> GetEquippedSlotItemID(ItemType EquipID) const override;
	int GetTotalRawAttributeValue(AttributeIdentifier ID) const override;

	void Tick() override;
	void PostTick() override;
	void Snap(int SnappingClient) override;
	void FakeSnap() override;

	bool IsActive() const override;
	bool IsConversational() const;
	void PrepareRespawnTick() override;

	int CalculateAttribute(AttributeIdentifier ID, int PowerLevel, bool Boss) const;

	int m_EidolonItemID;
	CPlayer* GetEidolonOwner() const;
	bool IsDisabledBotDamage() const { return m_DisabledBotDamage; }

	CPlayerItem* GetItem(ItemIdentifier ID) override;

private:
	ska::unordered_map< int, std::unique_ptr<CPlayerItem> > m_Items {};
	ESnappingPriority GetQuestBotSnappingPriority(const CPlayer* pSnappingPlayer) const;
	ESnappingPriority GetNpcSnappingPriority(const CPlayer* pSnappingPlayer, int ClientID) const;
	bool IsValidSnappingClient(int ClientID) const;

	void GetFormatedName(char* aBuffer, int BufferSize) override;
	int GetLevel() const;
	Mood GetMoodState() const override;
	const char* GetStatus() const;

	void TryRespawn() override;

	void HandlePathFinder();
};

#endif
