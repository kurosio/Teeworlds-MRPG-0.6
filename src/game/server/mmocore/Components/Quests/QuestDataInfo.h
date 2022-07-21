/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_QUEST_DATA_INFO_H
#define GAME_SERVER_COMPONENT_QUEST_DATA_INFO_H

#include "QuestStepDataInfo.h"

class CQuestDataInfo
{
public:
	char m_aName[24];
	char m_aStoryLine[24];
	int m_QuestID;
	int m_Gold;
	int m_Exp;

	std::string GetJsonFileName(int AccountID) const;
	const char* GetName() const { return m_aName; }
	const char* GetStory() const { return m_aStoryLine; }
	int GetQuestStoryPosition() const;
	int GetQuestStorySize() const;

	std::map<int, CPlayerQuestStepDataInfo> CopyBasicSteps()
	{
		std::map<int, CPlayerQuestStepDataInfo> m_Copy;
		for(auto& pStepBot : m_StepsQuestBot)
			m_Copy[pStepBot.first] = (CPlayerQuestStepDataInfo&)pStepBot.second;

		return m_Copy;
	}
	// steps with array bot data on active step
	std::map < int , CQuestStepDataInfo > m_StepsQuestBot;

public:
	static std::map < int, CQuestDataInfo > ms_aDataQuests;
};

#endif