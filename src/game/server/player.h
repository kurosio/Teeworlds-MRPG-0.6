/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_PLAYER_H
#define GAME_SERVER_PLAYER_H

#include "mmocore/Components/Accounts/AccountData.h"
#include "mmocore/Components/Inventory/ItemData.h"
#include "mmocore/Components/Quests/QuestData.h"
#include "mmocore/Components/Skills/SkillData.h"

#include <game/voting.h>
#include "entities/character.h"

enum
{
	WEAPON_SELF = -2, // self die
	WEAPON_WORLD = -1, // swap world etc
};

class CPlayer
{
	MACRO_ALLOC_POOL_ID()

	struct StructLatency
	{
		int m_AccumMin;
		int m_AccumMax;
		int m_Min;
		int m_Max;
	};

	struct StructLastAction
	{
		int m_TargetX;
		int m_TargetY;
	};

	struct StructDialogNPC
	{
		int m_TalkedID;
		int m_Progress;
		bool m_FreezedProgress;
	};
	StructDialogNPC m_DialogNPC;
	char m_aFormatDialogText[512];
	std::map < int, bool > m_aHiddenMenu;

protected:
	CCharacter* m_pCharacter;
	CGS* m_pGS;

	IServer* Server() const;
	int m_ClientID;

public:
	CGS* GS() const { return m_pGS; }
	vec2 m_ViewPos;
	int m_PlayerFlags;
	int m_aPlayerTick[TickState::NUM_TICK];
	Mood m_MoodState;

	CTeeInfo m_TeeInfos;
	StructLatency m_Latency;
	StructLastAction m_LatestActivity;

	/* #########################################################################
		VAR AND OBJECTS PLAYER MMO
	######################################################################### */
	CTuningParams m_PrevTuningParams;
	CTuningParams m_NextTuningParams;

	bool m_Spawned;

	short m_aSortTabs[NUM_SORT_TAB];
	short m_OpenVoteMenu;
	short m_LastVoteMenu;

	// TODO: fixme. improve the system using the ID method, as well as the ability to implement Backpage
	CVoteOptionsCallback m_ActiveMenuOptionCallback;
	VoteCallBack m_ActiveMenuRegisteredCallback;

	/* #########################################################################
		FUNCTIONS PLAYER ENGINE
	######################################################################### */
public:
	CPlayer(CGS* pGS, int ClientID);
	virtual ~CPlayer();

	virtual int GetTeam();
	virtual bool IsBot() const { return false; }
	virtual int GetBotID() const { return -1; }
	virtual int GetBotType() const { return -1; }
	virtual int GetBotMobID() const { return -1; }
	virtual	int GetPlayerWorldID() const;

	virtual int GetStartHealth();
	int GetStartMana();
	virtual	int GetHealth() { return GetTempData().m_TempHealth; }
	virtual	int GetMana() { return GetTempData().m_TempMana; }

	virtual void HandleTuningParams();
	virtual int64 GetMaskVisibleForClients() const { return -1; };
	virtual int IsVisibleForClient(int ClientID) const { return 2; }
	virtual int GetEquippedItemID(ItemFunctional EquipID, int SkipItemID = -1) const;
	virtual int GetAttributeCount(int BonusID, bool ActiveFinalStats = false);
	int GetItemsAttributeCount(int AttributeID) const;
	virtual void UpdateTempData(int Health, int Mana);

	virtual void GiveEffect(const char* Potion, int Sec, float Chance = 100.0f);
	virtual bool IsActiveEffect(const char* Potion) const;
	virtual void ClearEffects();

	virtual void Tick();
	virtual void PostTick();
	virtual void Snap(int SnappingClient);

private:
	virtual void EffectsTick();
	void TickSystemTalk();
	virtual void TryRespawn();

public:
	CCharacter *GetCharacter() const;

	void KillCharacter(int Weapon = WEAPON_WORLD);
	void OnDisconnect();
	void OnDirectInput(CNetObj_PlayerInput *NewInput);
	void OnPredictedInput(CNetObj_PlayerInput *NewInput) const;

	int GetCID() const { return m_ClientID; }
	/* #########################################################################
		FUNCTIONS PLAYER HELPER
	######################################################################### */
	void ProgressBar(const char *Name, int MyLevel, int MyExp, int ExpNeed, int GivedExp) const;
	bool Upgrade(int Value, int *Upgrade, int *Useless, int Price, int MaximalUpgrade) const;

	/* #########################################################################
		FUNCTIONS PLAYER ACCOUNT
	######################################################################### */
	bool SpendCurrency(int Price, int ItemID = 1);
	const char* GetLanguage() const;
	void AddExp(int Exp);
	void AddMoney(int Money);

	bool GetHiddenMenu(int HideID) const;
	bool IsAuthed() const;
	int GetStartTeam() const;

	static int ExpNeed(int Level);
	void ShowInformationStats(BroadcastPriority Priority = BroadcastPriority::BASIC_STATS);

	/* #########################################################################
		FUNCTIONS PLAYER PARSING
	######################################################################### */
	bool ParseItemsF3F4(int Vote);
  	bool ParseVoteUpgrades(const char *CMD, int VoteID, int VoteID2, int Get);

	/* #########################################################################
		FUNCTIONS PLAYER ITEMS
	######################################################################### */
	CItemData& GetItem(int ItemID);
	class CSkillData* GetSkill(int SkillID);
	CQuestData& GetQuest(int QuestID);
	CAccountTempData& GetTempData() const { return CAccountTempData::ms_aPlayerTempData[m_ClientID]; }
	CAccountData& Acc() const { return CAccountData::ms_aData[m_ClientID]; }

	int GetLevelTypeAttribute(int Class);
	int GetLevelAllAttributes();

	// npc conversations
	void SetTalking(int TalkedID, bool IsStartDialogue);
	void ClearTalking();
	int GetTalkedID() const { return m_DialogNPC.m_TalkedID; }

	// dialog formating
	const char *GetDialogText() const;
	void FormatDialogText(int DataBotID, const char *pText);
	void ClearDialogText();

	virtual const char* GetStatus() const;
	virtual Mood GetMoodState() const { return Mood::NORMAL; }
	void ChangeWorld(int WorldID);
};

#endif
