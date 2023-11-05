/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_QUEST_DATA_INFO_H
#define GAME_SERVER_COMPONENT_QUEST_DATA_INFO_H

#include <game/server/mmocore/Utils/DBSet.h>
#include "QuestStepDataInfo.h"

using QuestIdentifier = int;

class CQuestDescription : public MultiworldIdentifiableStaticData < std::map< int, CQuestDescription > >
{
	QuestIdentifier m_ID{};
	char m_aName[24]{};
	char m_aStoryLine[24]{};
	int m_Gold{};
	int m_Exp{};
	DBSet m_Types{};

public:
	CQuestDescription() = default;
	CQuestDescription(QuestIdentifier ID) : m_ID(ID) {}

	void Init(const std::string& Name, const std::string& Story, std::string& Types, int Gold, int Exp)
	{
		str_copy(m_aName, Name.c_str(), sizeof(m_aName));
		str_copy(m_aStoryLine, Story.c_str(), sizeof(m_aStoryLine));
		m_Gold = Gold;
		m_Exp = Exp;
		m_Types = std::move(Types);
		m_pData[m_ID] = *this;
	}

	QuestIdentifier GetID() const { return m_ID; }
	std::string GetJsonFileName(int AccountID) const;
	const char* GetName() const { return m_aName; }
	const char* GetStory() const { return m_aStoryLine; }
	int GetQuestStoryPosition() const;
	int GetQuestStorySize() const;
	int GetRewardGold() const { return m_Gold; }
	int GetRewardExp() const { return m_Exp; }
	bool IsDailyQuest() const { return m_Types.hasSet("Daily"); }

	void InitPlayerDefaultSteps(int OwnerCID, std::map < int, CPlayerQuestStep >& pElem) const
	{
		for(const auto& [rStepID, rStepData] : m_StepsQuestBot)
		{
			pElem[rStepID].m_ClientID = OwnerCID;
			pElem[rStepID].m_Bot = rStepData.m_Bot;
			pElem[rStepID].m_StepComplete = false;
			pElem[rStepID].m_ClientQuitting = false;
			pElem[rStepID].m_aMobProgress.clear();
		}
	}

	// steps with array bot data on active step
	std::unordered_map < int , CQuestStepDescription > m_StepsQuestBot;
};

#endif