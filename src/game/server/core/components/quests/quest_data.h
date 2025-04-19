/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_QUEST_DATA_H
#define GAME_SERVER_COMPONENT_QUEST_DATA_H

#define PRINT_QUEST_PREFIX "quest_system"

#include "datafile_progress.h"
#include "quest_step_data.h"

class CGS;
class CPlayer;
using QuestIdentifier = int;

enum
{
	QUEST_FLAG_TYPE_MAIN = 1 << 0,
	QUEST_FLAG_TYPE_SIDE = 1 << 1,
	QUEST_FLAG_TYPE_DAILY = 1 << 2,
	QUEST_FLAG_TYPE_WEEKLY = 1 << 3,
	QUEST_FLAG_TYPE_REPEATABLE = 1 << 4,

	QUEST_FLAG_CANT_REFUSE = 1 << 5,
	QUEST_FLAG_NO_ACTIVITY_POINT = 1 << 6,

	QUEST_FLAG_GRANTED_FROM_CHAIN = 1 << 9,
	QUEST_FLAG_GRANTED_FROM_NPC = 1 << 10,
	QUEST_FLAG_GRANTED_FROM_BOARD = 1 << 11,
};

// quest description
class CQuestDescription : public MultiworldIdentifiableData < std::map< int, CQuestDescription* > >
{
public:
	/* -------------------------------------
	 * Quest reward impl
	 * ------------------------------------- */
	class CReward
	{
		int m_Experience {};
		int m_Gold {};

	public:
		CReward() = default;
		void Init(int Experience, int Gold)
		{
			m_Experience = Experience;
			m_Gold = Gold;
		}
		int GetExperience() const { return m_Experience; }
		int GetGold() const { return m_Gold; }
		void ApplyReward(CPlayer* pPlayer) const;
	};

private:
	// variables
	QuestIdentifier m_ID {};
	std::string m_Name {};
	CReward m_Reward {};
	int m_Flags {};
	std::optional<int> m_NextQuestID {};
	std::optional<int> m_PreviousQuestID {};

public:
	CQuestDescription() = default;

	static CQuestDescription* CreateElement(QuestIdentifier ID)
	{
		const auto pData = new CQuestDescription();
		pData->m_ID = ID;
		return m_pData[ID] = pData;
	}

	void Init(const std::string& Name, int Gold, int Exp, std::optional<int> NextQuestID)
	{
		m_Name = Name;
		m_NextQuestID = NextQuestID;
		m_Reward.Init(Exp, Gold);
	}

	void InitPrevousQuestID(int QuestID)
	{
		m_PreviousQuestID = QuestID;
	}

	void InitFlags(const DBSet& FlagSet)
	{
		// initialize type flags
		if(FlagSet.hasSet("Type main"))
			m_Flags |= QUEST_FLAG_TYPE_MAIN;
		else if(FlagSet.hasSet("Type side"))
			m_Flags |= QUEST_FLAG_TYPE_SIDE;
		else if(FlagSet.hasSet("Type daily"))
			m_Flags |= QUEST_FLAG_TYPE_DAILY;
		else if(FlagSet.hasSet("Type weekly"))
			m_Flags |= QUEST_FLAG_TYPE_WEEKLY;
		else if(FlagSet.hasSet("Type repeatable"))
			m_Flags |= QUEST_FLAG_TYPE_REPEATABLE;

		// initialize other flagss
		if(m_NextQuestID.has_value() || m_PreviousQuestID.has_value())
			m_Flags |= QUEST_FLAG_GRANTED_FROM_CHAIN;
		if(FlagSet.hasSet("Can't refuse"))
			m_Flags |= QUEST_FLAG_CANT_REFUSE;
		if(FlagSet.hasSet("No activity point"))
			m_Flags |= QUEST_FLAG_NO_ACTIVITY_POINT;
	}

	void AddFlag(int Flag) { m_Flags |= Flag; }
	void SetFlags(int Flag) { m_Flags = Flag; }
	int GetFlags() const { return m_Flags; }
	bool HasFlag(int Flag) const { return (m_Flags & Flag) != 0; }

	QuestIdentifier GetID() const { return m_ID; }
	const char* GetName() const { return m_Name.c_str(); }
	int GetChainLength() const;
	int GetCurrentChainPos() const;
	CQuestDescription* GetNextQuest() const;
	CQuestDescription* GetPreviousQuest() const;
	CReward& Reward() { return m_Reward; }

	bool HasObjectives(int Step);

	void PreparePlayerObjectives(int Step, int ClientID, std::deque<CQuestStep*>& pElem);
	void ResetPlayerObjectives(std::deque<CQuestStep*>& pElem);

	bool CanBeGrantedByChain() const { return HasFlag(QUEST_FLAG_GRANTED_FROM_CHAIN); }
	bool CanBeGrantedByNPC() const { return HasFlag(QUEST_FLAG_GRANTED_FROM_NPC); }
	bool CanBeGrantedByBoard() const { return HasFlag(QUEST_FLAG_GRANTED_FROM_BOARD); }
	bool CanBeGranted() const { return CanBeGrantedByChain() || CanBeGrantedByNPC() || CanBeGrantedByBoard(); }
	bool CanBeAcceptOrRefuse() const { return CanBeGrantedByBoard(); }

	// steps with array bot data on active step
	std::map < int, std::deque<CQuestStepBase> > m_vObjectives;
};

class CPlayerQuest : public MultiworldIdentifiableData< std::map < int, std::map <int, CPlayerQuest* > > >
{
	friend class QuestDatafile;
	friend class CQuestManager;

	CGS* GS() const;
	CPlayer* GetPlayer() const;

	int m_ClientID {};
	QuestIdentifier m_ID {};
	QuestState m_State {};
	int m_Step {};
	std::deque<CQuestStep*> m_vObjectives {};
	QuestDatafile m_Datafile {};

public:
	CPlayerQuest(QuestIdentifier ID, int ClientID) : m_ClientID(ClientID), m_Step(1) { m_ID = ID; }
	~CPlayerQuest();

	static CPlayerQuest* CreateElement(QuestIdentifier ID, int ClientID)
	{
		dbg_assert(CQuestDescription::Data().find(ID) != CQuestDescription::Data().end(), "Quest ID not found");
		const auto pData = new CPlayerQuest(ID, ClientID);
		pData->m_Datafile.Init(pData);
		return m_pData[ClientID][ID] = pData;
	}

	void Init(QuestState State)
	{
		m_State = State;
		m_Datafile.Load();
	}

	QuestDatafile& Datafile() { return m_Datafile; }
	CQuestDescription* Info() const;
	QuestIdentifier GetID() const { return m_ID; }
	QuestState GetState() const { return m_State; }
	bool IsCompleted() const { return m_State == QuestState::Finished; }
	bool IsAccepted() const { return m_State == QuestState::Accepted; }
	bool HasUnfinishedObjectives() const;
	int GetStepPos() const { return m_Step; }
	CQuestStep* GetStepByMob(int MobID);

	void Update();
	bool Accept();
	void Refuse();
	void Reset();

private:
	void UpdateStepProgress();
	void SetNewState(QuestState State);
};


#endif