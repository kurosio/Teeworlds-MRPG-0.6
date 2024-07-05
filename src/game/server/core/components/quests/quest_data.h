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

	QUEST_FLAG_GRANTED_FROM_NPC = 1 << 10,
	QUEST_FLAG_GRANTED_FROM_BOARD = 1 << 11,
	QUEST_FLAG_GRANTED_FROM_AUTO = 1 << 12,
};

// quest reward class
class CQuestReward
{
	int m_Experience{};
	int m_Gold{};

public:
	CQuestReward() = default;
	void Init(int Experience, int Gold)
	{
		m_Experience = Experience;
		m_Gold = Gold;
	}
	int GetExperience() const { return m_Experience; }
	int GetGold() const { return m_Gold; }
	void ApplyReward(CPlayer* pPlayer) const;
};

// quest description
class CQuestDescription : public MultiworldIdentifiableData < std::map< int, CQuestDescription* > >
{
	QuestIdentifier m_ID {};
	std::string m_Name {};
	CQuestReward m_Reward {};
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

	void Init(const std::string& Name, int Gold, int Exp, std::optional<int> NextQuestID, const DBSet& FlagSet)
	{
		m_Name = Name;
		m_NextQuestID = NextQuestID;
		m_Reward.Init(Exp, Gold);
		InitFlags(FlagSet);
	}

	void InitPrevousQuestID(int QuestID)
	{
		m_PreviousQuestID = QuestID;
	}

	void InitFlags(const DBSet& FlagSet)
	{
		// initialize type flags
		if(FlagSet.hasSet("Type daily"))
			m_Flags |= QUEST_FLAG_TYPE_DAILY;
		else if(FlagSet.hasSet("Type weekly"))
			m_Flags |= QUEST_FLAG_TYPE_WEEKLY;
		else if(FlagSet.hasSet("Type repeatable"))
			m_Flags |= QUEST_FLAG_TYPE_REPEATABLE;
		else
			m_Flags |= QUEST_FLAG_TYPE_MAIN;

		// initialize other flags
		if(m_NextQuestID.has_value())
			m_Flags |= QUEST_FLAG_GRANTED_FROM_AUTO;
	}

	void AddFlag(int Flag)
	{
		m_Flags |= Flag;
	}

	QuestIdentifier GetID() const { return m_ID; }
	const char* GetName() const { return m_Name.c_str(); }
	std::optional<int> GetNextQuestID() const { return m_NextQuestID; }
	std::optional<int> GetPreviousQuestID() const { return m_PreviousQuestID; }
	CQuestReward& Reward() { return m_Reward; }

	void PreparePlayerSteps(int StepPos, int ClientID, std::deque<CQuestStep>* pElem);
	bool HasFlag(int Flag) const { return (m_Flags & Flag) != 0; }
	bool IsGranted() const { return HasFlag(QUEST_FLAG_GRANTED_FROM_AUTO) || HasFlag(QUEST_FLAG_GRANTED_FROM_NPC) || HasFlag(QUEST_FLAG_GRANTED_FROM_BOARD); }

	// steps with array bot data on active step
	std::map < int, std::deque<CQuestStepBase> > m_vSteps;
};

class CPlayerQuest : public MultiworldIdentifiableData< std::map < int, std::map <int, CPlayerQuest* > > >
{
	friend class QuestDatafile;
	friend class CQuestManager;

	CGS* GS() const;
	CPlayer* GetPlayer() const;

	std::deque<CQuestStep> m_vSteps {};
	int m_ClientID {};
	QuestIdentifier m_ID {};
	QuestState m_State {};
	int m_Step {};

public:
	CPlayerQuest(QuestIdentifier ID, int ClientID) : m_ClientID(ClientID), m_Step(1) { m_ID = ID; }
	~CPlayerQuest();

	/*
	 * Datafile for the quest
	 */
	QuestDatafile m_Datafile {};

	/*
	 * Create a new instance of the quest
	 *
	 */
	static CPlayerQuest* CreateElement(QuestIdentifier ID, int ClientID)
	{
		dbg_assert(CQuestDescription::Data().find(ID) != CQuestDescription::Data().end(), "Quest ID not found");
		const auto pData = new CPlayerQuest(ID, ClientID);
		pData->m_Datafile.Init(pData);
		return m_pData[ClientID][ID] = pData;
	}

	/*
	 * Initialize the quest
	 *
	 */
	void Init(QuestState State)
	{
		m_State = State;
		m_Datafile.Load();
	}

	/*
	 * Get the quest description
	 *
	 */
	CQuestDescription* Info() const;

	/*
	 * Get the quest identifier
	 *
	 */
	QuestIdentifier GetID() const { return m_ID; }

	/*
	 * Get the quest state
	 *
	 */
	QuestState GetState() const { return m_State; }

	/*
	 * Check if the quest is completed
	 *
	 */
	bool IsCompleted() const { return m_State == QuestState::FINISHED; }

	/*
	 * Check if the quest is accepted
	 *
	 */
	bool IsAccepted() const { return m_State == QuestState::ACCEPT; }

	/*
	 * Check the quest has unfinished steps on the current position
	 *
	 */
	bool HasUnfinishedSteps() const;

	/*
	 * Get the current step position
	 *
	 */
	int GetStepPos() const { return m_Step; }

	/*
	 * Get the step by quest mob id
	 *
	 */
	CQuestStep* GetStepByMob(int MobID);

	/*
	 * Update the quest and all steps
	 *
	 */
	void Update();

	/*
	 * Accept the quest and set the state to 'Accepted'
	 *
	 */
	bool Accept();

	/*
	 * Reset all datas of the quest and refuse it to state 'No accepted'
	 *
	 */
	void Refuse();

	/*
	 * Reset all datas of the quest save state
	 *	 
	 */
	void Reset();

private:
	void UpdateStepPosition();
};


#endif