/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_QUEST_DATA_H
#define GAME_SERVER_COMPONENT_QUEST_DATA_H

#define QUEST_PREFIX_DEBUG "quest_system"

#include "QuestDataInfo.h"

class CPlayerQuest;
class QuestDatafile
{
	CPlayerQuest* m_pQuest;

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

	int m_ClientID {};
	QuestIdentifier m_ID {};
	QuestState m_State {};
	int m_Step {};

	std::map < int, CQuestStep > m_vSteps {};
	std::deque < class CStepPathFinder* > m_apEntityNPCNavigator{};

public:
	QuestDatafile m_Datafile{};

	CPlayerQuest(QuestIdentifier ID, int ClientID) : m_ClientID(ClientID) { m_ID = ID; }
	~CPlayerQuest();

	static CPlayerQuest* CreateElement(QuestIdentifier ID, int ClientID)
	{
		dbg_assert(CQuestDescription::Data().find(ID) != CQuestDescription::Data().end(), "Quest ID not found");
		const auto pData = new CPlayerQuest(ID, ClientID);
		return m_pData[ClientID][ID] = pData;
	}

	void Init(QuestState State)
	{
		m_State = State;
		Info()->PreparePlayerQuestSteps(m_ClientID, &m_vSteps);

		m_Datafile.Init(this);
		m_Datafile.Load();
	}

	CQuestDescription* Info() const;
	QuestIdentifier GetID() const { return m_ID; }
	QuestState GetState() const { return m_State; }
	bool IsCompleted() const { return m_State == QuestState::FINISHED; }
	bool IsActive() const { return m_State == QuestState::ACCEPT; }

	// steps
	int GetCurrentStepPos() const { return m_Step; }
	CQuestStep* GetStepByMob(int MobID) { return &m_vSteps[MobID]; }

	// steps path finder tools
	class CStepPathFinder* FoundEntityNPCNavigator(int SubBotID) const;
	class CStepPathFinder* AddEntityNPCNavigator(class QuestBotInfo* pBot);

	// main
	void CheckAvailableNewStep();
	bool Accept();
	void Refuse();
	void Reset();

private:
	std::string GetDataFilename() const;
	void Finish();
};


#endif