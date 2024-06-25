/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_QUEST_DATA_H
#define GAME_SERVER_COMPONENT_QUEST_DATA_H

#define PRINT_QUEST_PREFIX "quest_system"

#include "datafile_progress.h"
#include "QuestStepDataInfo.h"

class CGS;
class CPlayer;
using QuestIdentifier = int;

enum
{
	QFLAG_NONE = 0,
	QFLAG_DAILY = 1 << 0,
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
	std::string m_Story {};
	CQuestReward m_Reward {};
	int m_Flags {};

public:
	CQuestDescription() = default;

	static CQuestDescription* CreateElement(QuestIdentifier ID)
	{
		const auto pData = new CQuestDescription();
		pData->m_ID = ID;
		return m_pData[ID] = pData;
	}

	void Init(const std::string& Name, const std::string& Story, int Gold, int Exp, const DBSet& flagSet)
	{
		m_Name = Name;
		m_Story = Story;
		m_Reward.Init(Exp, Gold);
		InitFlags(flagSet);
	}

	void InitFlags(const DBSet& flagSet)
	{
		if(flagSet.hasSet("Daily"))
			m_Flags |= QFLAG_DAILY;
	}

	QuestIdentifier GetID() const { return m_ID; }
	const char* GetName() const { return m_Name.c_str(); }
	const char* GetStory() const { return m_Story.c_str(); }
	int GetStoryCurrentPos() const;
	int GetStoryQuestsNum() const;

	CQuestReward& Reward() { return m_Reward; }

	void PreparePlayerSteps(int StepPos, int ClientID, std::deque<CQuestStep>* pElem);
	bool IsHasFlag(int Flag) const { return (m_Flags & Flag) != 0; }
	bool IsDaily() const { return IsHasFlag(QFLAG_DAILY); }

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