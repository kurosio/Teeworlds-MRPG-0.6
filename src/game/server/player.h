/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_PLAYER_H
#define GAME_SERVER_PLAYER_H

#include "core/components/accounts/account_data.h"
#include "core/components/inventory/item_data.h"
#include "core/components/quests/quest_data.h"
#include "core/components/skills/skill_data.h"

#include "entities/character.h"
#include "core/tools/cooldown.h"
#include "core/tools/effect_manager.h"
#include "core/tools/motd_menu.h"
#include "core/tools/vote_wrapper.h"

class CPlayerBot;
class CPlayer
{
	MACRO_ALLOC_POOL_ID()

	struct StructLatency
	{
		int m_Accum;
		int m_AccumMin;
		int m_AccumMax;
		int m_Avg;
		int m_Min;
		int m_Max;
	};

	struct StructLastAction
	{
		int m_TargetX;
		int m_TargetY;
	};

	int m_ShowHealthNicknameTick;

protected:
	IServer* Server() const;

	CGS* m_pGS {};
	int m_ClientID {};
	bool m_MarkForDestroy {};
	CCharacter* m_pCharacter {};
	bool m_Afk {};
	bool m_LastInputInit {};
	int64_t m_LastPlaytime {};
	FixedViewCam m_FixedView {};
	std::map<AttributeIdentifier, int> m_aStats {};

public:
	CGS* GS() const { return m_pGS; }
	FixedViewCam& LockedView() { return m_FixedView; }

	vec2 m_ViewPos{};
	int m_PlayerFlags{};
	int m_aPlayerTick[NUM_TICK]{};
	char m_aRotateClanBuffer[128]{};
	char m_aInitialClanBuffer[128]{};
	Mood m_MoodState{};
	int m_ActiveCraftGroupID{};
	CRandomItem m_CurrentRandomItem {};
	std::unique_ptr<MotdMenu> m_pMotdMenu{};
	std::optional<ItemGroup> m_InventoryItemGroupFilter{};
	std::optional<ItemType> m_InventoryItemTypeFilter{};
	std::optional<int> m_GroupFilter{};
	std::optional<int> m_SubgroupFilter {};


	char m_aLastMsg[256]{};
	StructLatency m_Latency;
	StructLastAction m_LatestActivity;

	/* ==========================================================
		VAR AND OBJECTS PLAYER MMO
	========================================================== */
	CCooldown m_Cooldown {};
	CVotePlayerData m_VotesData {};
	CMotdPlayerData m_MotdData {};
	CPlayerDialog m_Dialog;
	CEffectManager m_Effects {};
	CTuningParams m_PrevTuningParams;
	CTuningParams m_NextTuningParams;

	bool m_WantSpawn;
	bool m_ActivatedGroupColour;
	int m_TickActivatedGroupColour;
	std::optional<int> m_EidolonCID;

	/* ==========================================================
		FUNCTIONS PLAYER ENGINE
	========================================================== */
public:
	CNetObj_PlayerInput* m_pLastInput;

	CPlayer(CGS* pGS, int ClientID);
	virtual ~CPlayer();

	bool IsAfk() const { return m_Afk; }
	int64_t GetAfkTime() const;
	void MarkForDestroy() { m_MarkForDestroy = true; }
	bool IsMarkedForDestroy() const { return m_MarkForDestroy; }

	virtual bool IsBot() const { return false; }
	virtual int GetTeam();
	virtual	int GetCurrentWorldID() const;
	virtual const CTeeInfo& GetTeeInfo() const;
	virtual int GetMaxHealth() const;
	virtual int GetMaxMana() const;
	virtual	int GetHealth() const { return GetSharedData().m_Health; }
	virtual	int GetMana() const { return GetSharedData().m_Mana; }

	virtual void HandleTuningParams();
	virtual int64_t GetMaskVisibleForClients() const { return -1; }
	virtual ESnappingPriority IsActiveForClient(int ClientID) const { return ESnappingPriority::High; }
	virtual std::optional<int> GetEquippedSlotItemID(ItemType EquipID) const;
	virtual bool IsEquippedSlot(ItemType EquipID) const;
	virtual int GetTotalRawAttributeValue(AttributeIdentifier ID) const;
	std::optional<float> GetTotalAttributeChance(AttributeIdentifier ID) const;
	virtual void UpdateSharedCharacterData(int Health, int Mana);

	int GetTotalAttributeValue(AttributeIdentifier AttributeID) const;
	void UpdateTotalAttributeValue(AttributeIdentifier AttributeID, int Value) { m_aStats[AttributeID] = Value; }
	void FormatBroadcastBasicStats(char* pBuffer, int Size, const char* pAppendStr = "\0") const;

	virtual void Tick();
	virtual void PostTick();
	virtual void Snap(int SnappingClient);
	virtual void FakeSnap();
	virtual bool IsActive() const { return true; }
	virtual void PrepareRespawnTick();
	virtual Mood GetMoodState() const { return Mood::Normal; }

	void RefreshClanTagString();

	CPlayerBot* GetEidolon() const;
	void TryCreateEidolon();
	void TryRemoveEidolon();

private:
	virtual void GetFormatedName(char* aBuffer, int BufferSize);
	virtual void TryRespawn();
	void HandleScoreboardColors();

public:
	int GetCID() const { return m_ClientID; }
	CCharacter* GetCharacter() const;
	void KillCharacter(int Weapon = WEAPON_WORLD);
	void OnDisconnect();
	void OnDirectInput(CNetObj_PlayerInput* pNewInput);
	void OnPredictedInput(CNetObj_PlayerInput* pNewInput) const;

	void ProgressBar(const char* pType, int Level, uint64_t Exp, uint64_t ExpNeeded, uint64_t GainedExp) const;
	const char* GetLanguage() const;
	bool IsAuthed() const;
	bool ParseVoteOptionResult(int Vote);

	CPlayerItem* GetItem(const CItem& Item) { return GetItem(Item.GetID()); }
	virtual CPlayerItem* GetItem(ItemIdentifier ID);
	CSkill* GetSkill(int SkillID) const;
	CPlayerQuest* GetQuest(QuestIdentifier ID) const;
	CAccountSharedData& GetSharedData() const { return CAccountSharedData::ms_aPlayerSharedData[m_ClientID]; }
	CAccountData* Account() const { return &CAccountData::ms_aData[m_ClientID]; }

	void ShowHealthNickname(int Sec);
	bool IsSameMotdMenu(int Menulist) const { return m_pMotdMenu && m_pMotdMenu->GetMenulist() == Menulist; }
	void CloseMotdMenu() { m_pMotdMenu->ClearMotd(); }

	void ChangeWorld(int WorldID, std::optional<vec2> newWorldPosition = std::nullopt);
	void StartUniversalScenario(const std::string& ScenarioData, int ScenarioID);
};

#endif
