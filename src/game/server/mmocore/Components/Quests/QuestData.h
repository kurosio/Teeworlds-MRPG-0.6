/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_QUEST_DATA_H
#define GAME_SERVER_COMPONENT_QUEST_DATA_H
#include "QuestDataInfo.h"

class CQuestData
{
public:
	int m_QuestID;
	CPlayer* m_pPlayer;

	int m_State;
	int m_Step;

	std::string GetJsonFileName() const;
	CQuestDataInfo& Info() const;
	bool IsComplected() const { return m_State == QuestState::QUEST_FINISHED; }
	int GetState() const { return m_State; }

	// steps
	void InitSteps();
	void LoadSteps();
	void SaveSteps();
	void ClearSteps();
	std::map < int, CPlayerQuestStepDataInfo > m_StepsQuestBot;

	// main
	void CheckaAvailableNewStep();
	bool Accept();
private:
	void Finish();

public:
	static std::map < int, std::map <int, CQuestData > > ms_aPlayerQuests;
};

#endif