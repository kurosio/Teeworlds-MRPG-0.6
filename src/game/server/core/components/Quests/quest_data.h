/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_QUEST_DATA_H
#define GAME_SERVER_COMPONENT_QUEST_DATA_H

#define QUEST_PREFIX_DEBUG "quest_system"

#include "quest_desc_data.h"

class CPlayerQuest;
class QuestDatafile
{
	CPlayerQuest* m_pQuest{};

public:
	void Init(CPlayerQuest* pQuest) { m_pQuest = pQuest; }
	void Create();
	void Load();
	bool Save();
	void Delete();
};

class CGS;
class CPlayer;
class CPlayerQuest : public MultiworldIdentifiableStaticData< std::map < int, std::map <int, CPlayerQuest* > > >
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
	std::string GetDataFilename() const;
};


#endif