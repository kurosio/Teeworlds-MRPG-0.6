/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "QuestDataInfo.h"

#include <algorithm>

std::string CQuestDescription::GetDataFilename(int AccountID) const { return "server_data/quest_tmp/" + std::to_string(m_ID) + "-" + std::to_string(AccountID) + ".json"; }

int CQuestDescription::GetQuestStoryPosition() const
{
	// get position of quests storyline
	return (int)std::count_if(Data().begin(), Data().end(), [this](std::pair< const int, CQuestDescription*>& pItem)
	{
		return str_comp(pItem.second->m_aStoryLine, m_aStoryLine) == 0 && m_ID >= pItem.first;
	});
}

int CQuestDescription::GetQuestStorySize() const
{
	// get size of quests storyline
	return (int)std::count_if(Data().begin(), Data().end(), [this](std::pair< const int, CQuestDescription*>& pItem)
	{
		return str_comp(pItem.second->m_aStoryLine, m_aStoryLine) == 0;
	});
}

void CQuestDescription::PreparePlayerQuestSteps(int ClientID, std::map<int, CQuestStep>* pElem) const
{
	for(auto& [Step, Data] : m_StepsQuestBot)
	{
		(*pElem)[Step].m_ClientID = ClientID;
		(*pElem)[Step].m_Bot = Data.m_Bot;
		(*pElem)[Step].m_StepComplete = false;
		(*pElem)[Step].m_ClientQuitting = false;
		(*pElem)[Step].m_aMobProgress.clear();
	}
}
