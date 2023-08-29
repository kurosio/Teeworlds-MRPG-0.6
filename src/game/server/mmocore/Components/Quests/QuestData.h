/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_QUEST_DATA_H
#define GAME_SERVER_COMPONENT_QUEST_DATA_H
#include "QuestDataInfo.h"

class CQuestData : public MultiworldIdentifiableStaticData< std::map < int, std::map <int, CQuestData > > >
{
	int m_ClientID {};
	QuestIdentifier m_ID {};
	QuestState m_State {};
	int m_Step {};

	class CGS* GS() const;
	class CPlayer* GetPlayer() const;

public:
	CQuestData() = default;
	CQuestData(QuestIdentifier ID, int ClientID) : m_ClientID(ClientID) { m_ID = ID; }

	void Init(QuestState State)
	{
		m_State = State;
		CQuestData::m_pData[m_ClientID][m_ID] = *this;
		CQuestData::m_pData[m_ClientID][m_ID].LoadSteps();
	}

	CQuestDataInfo& Info() const;
	std::string GetJsonFileName() const;

	int GetStep() const { return m_Step; }
	QuestIdentifier GetID() const { return m_ID; }
	QuestState GetState() const { return m_State; }
	bool IsComplected() const { return m_State == QuestState::FINISHED; }

	// steps
	void InitSteps();
	void LoadSteps();
	void SaveSteps();
	void ClearSteps();
	std::map < int, CPlayerQuestStepDataInfo > m_aPlayerSteps{};

	// main
	void CheckAvailableNewStep();
	bool Accept();
private:
	void Finish();
};

#endif