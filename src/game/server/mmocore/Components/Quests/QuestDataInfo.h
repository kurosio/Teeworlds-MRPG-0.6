/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_QUEST_DATA_INFO_H
#define GAME_SERVER_COMPONENT_QUEST_DATA_INFO_H

#include "QuestStepDataInfo.h"

using QuestIdentifier = int;

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

	void InitPlayerDefaultSteps(std::map < int, CPlayerQuestStepDataInfo >& pElem) const
	{
		for(const auto& [rStepID, rStepData] : m_StepsQuestBot)
		{
			pElem[rStepID].m_Bot = rStepData.m_Bot;
			pElem[rStepID].m_StepComplete = false;
			pElem[rStepID].m_ClientQuitting = false;
			pElem[rStepID].m_aMobProgress.clear();
		}
	}

	// steps with array bot data on active step
	std::unordered_map < int , CQuestStepDataInfo > m_StepsQuestBot;

public:
	static std::map < int, CQuestDataInfo > ms_aDataQuests;
};

#endif