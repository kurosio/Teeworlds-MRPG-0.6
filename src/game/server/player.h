/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_PLAYER_H
#define GAME_SERVER_PLAYER_H

#include "core/components/Accounts/AccountData.h"
#include "core/components/Inventory/ItemData.h"
#include "core/components/Quests/quest_data.h"
#include "core/components/skills/skill_data.h"

#include "entities/character.h"

#include "class_data.h"
#include "vote_event_optional.h"
#include "core/utilities/cooldown.h"
#include "core/utilities/vote_wrapper.h"

enum
{
	WEAPON_SELF = -2, // self die
	WEAPON_WORLD = -1, // swap world etc
};

enum StateSnapping
{
	STATE_SNAPPING_NONE = 0,
	STATE_SNAPPING_ONLY_CHARACTER,
	STATE_SNAPPING_FULL,
};

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

	int m_SnapHealthNicknameTick;

protected:
	CCharacter* m_pCharacter;
	CGS* m_pGS;

	IServer* Server() const;
	int m_ClientID;

	// lastest afk state
	bool m_Afk;
	bool m_LastInputInit;
	int64_t m_LastPlaytime;
	CClassData m_Class {};

public:
	CGS* GS() const { return m_pGS; }
	CClassData* GetClass() { return &m_Class; }
	const CClassData* GetClass() const { return &m_Class; }

	vec2 m_ViewPos;
	int m_PlayerFlags;
	int m_aPlayerTick[NUM_TICK];
	char m_aRotateClanBuffer[128];
	Mood m_MoodState;
	CCooldown m_Cooldown {};
	CVotePlayerData m_VotesData;

	char m_aLastMsg[256];
	int m_TutorialStep;

	StructLatency m_Latency;
	StructLastAction m_LatestActivity;

	/* ==========================================================
		VAR AND OBJECTS PLAYER MMO
	========================================================== */
	CTuningParams m_PrevTuningParams;
	CTuningParams m_NextTuningParams;
	CPlayerDialog m_Dialog;

	bool m_WantSpawn;
	bool m_RequestChangeNickname;
	int m_EidolonCID;
	bool m_ActivedGroupColors;
	int m_TickActivedGroupColors;

	/* ==========================================================
		FUNCTIONS PLAYER ENGINE
	========================================================== */
public:
	CNetObj_PlayerInput* m_pLastInput;

	CPlayer(CGS* pGS, int ClientID);
	virtual ~CPlayer();

	virtual int GetTeam();
	virtual bool IsBot() const { return false; }
	virtual int GetBotID() const { return -1; }
	virtual int GetBotType() const { return -1; }
	virtual int GetBotMobID() const { return -1; }
	virtual	int GetPlayerWorldID() const;
	virtual CTeeInfo& GetTeeInfo() const;

	virtual int GetStartHealth() const;
	int GetStartMana() const;
	virtual	int GetHealth() const { return GetTempData().m_TempHealth; }
	virtual	int GetMana() const { return GetTempData().m_TempMana; }
	bool IsAfk() const { return m_Afk; }
	int64_t GetAfkTime() const;

	void FormatBroadcastBasicStats(char* pBuffer, int Size, const char* pAppendStr = "\0");

	virtual void HandleTuningParams();
	virtual int64_t GetMaskVisibleForClients() const { return -1; }
	virtual StateSnapping IsActiveForClient(int ClientID) const { return STATE_SNAPPING_FULL; }
	virtual int GetEquippedItemID(ItemFunctional EquipID, int SkipItemID = -1) const;
	virtual int GetAttributeSize(AttributeIdentifier ID) const;
	float GetAttributePercent(AttributeIdentifier ID) const;
	virtual void UpdateTempData(int Health, int Mana);

	virtual bool GiveEffect(const char* Potion, int Sec, float Chance = 100.0f);
	virtual bool IsActiveEffect(const char* Potion) const;
	virtual void ClearEffects();

	virtual void Tick();
	virtual void PostTick();
	virtual void Snap(int SnappingClient);
	virtual void FakeSnap();

	void RefreshClanString();

	virtual bool IsActive() const { return true; }
	virtual void PrepareRespawnTick();
	class CPlayerBot* GetEidolon() const;
	void TryCreateEidolon();
	void TryRemoveEidolon();

private:
	virtual void GetFormatedName(char* aBuffer, int BufferSize);
	virtual void HandleEffects();
	virtual void TryRespawn();
	void HandleScoreboardColors();
	void HandlePrison();

public:
	CCharacter* GetCharacter() const;

	void KillCharacter(int Weapon = WEAPON_WORLD);
	void OnDisconnect();
	void OnDirectInput(CNetObj_PlayerInput* pNewInput);
	void OnPredictedInput(CNetObj_PlayerInput* pNewInput) const;

	int GetCID() const { return m_ClientID; }
	/* ==========================================================
		FUNCTIONS PLAYER HELPER
	========================================================== */
	void ProgressBar(const char* Name, int MyLevel, int MyExp, int ExpNeed, int GivedExp) const;
	bool Upgrade(int Value, int* Upgrade, int* Useless, int Price, int MaximalUpgrade) const;

	/* ==========================================================
		FUNCTIONS PLAYER ACCOUNT
	========================================================== */
	const char* GetLanguage() const;

	bool IsAuthed() const;
	int GetStartTeam() const;

	/* ==========================================================
		FUNCTIONS PLAYER PARSING
	========================================================== */
	bool ParseVoteOptionResult(int Vote);
	bool IsClickedKey(int KeyID) const;

	/* ==========================================================
		FUNCTIONS PLAYER ITEMS
	========================================================== */
	class CPlayerItem* GetItem(const CItem& Item) { return GetItem(Item.GetID()); }
	virtual class CPlayerItem* GetItem(ItemIdentifier ID);
	class CSkill* GetSkill(SkillIdentifier ID);
	class CPlayerQuest* GetQuest(QuestIdentifier ID) const;
	CAccountTempData& GetTempData() const { return CAccountTempData::ms_aPlayerTempData[m_ClientID]; }
	CAccountData* Account() const { return &CAccountData::ms_aData[m_ClientID]; }

	int GetTypeAttributesSize(AttributeGroup Type);
	int GetAttributesSize();

	void SetSnapHealthTick(int Sec);

	virtual Mood GetMoodState() const { return Mood::NORMAL; }
	void ChangeWorld(int WorldID);

	/* ==========================================================
	   VOTING OPTIONAL EVENT
	========================================================== */
private:
	// Function: RunEventOptional
	// Parameters:
	//    - Option: an integer value representing the selected option for the vote event
	//    - pOptional: a pointer to a CVoteEventOptional object representing the vote event
	// Description:
	//    - Runs the selected optional vote event
	void RunEventOptional(int Option, CVoteEventOptional* pOptional);

	// Function: HandleVoteOptionals
	// Description:
	//    - Handles all the optional vote events
	void HandleVoteOptionals() const;
};

#endif
