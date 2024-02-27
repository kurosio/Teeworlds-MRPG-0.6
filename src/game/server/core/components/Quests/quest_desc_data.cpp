/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "quest_desc_data.h"

#include <algorithm>

int CQuestDescription::GetStoryQuestPosition() const
{
	// get position of quests storyline
	return (int)std::count_if(Data().begin(), Data().end(), [this](std::pair< const int, CQuestDescription*>& pItem)
	{
		return str_comp(pItem.second->m_aStoryLine, m_aStoryLine) == 0 && m_ID >= pItem.first;
	});
}

int CQuestDescription::GetStoryQuestsNum() const
{
	// get size of quests storyline
	return (int)std::count_if(Data().begin(), Data().end(), [this](std::pair< const int, CQuestDescription*>& pItem)
	{
		return str_comp(pItem.second->m_aStoryLine, m_aStoryLine) == 0;
	});
}

void CQuestDescription::PreparePlayerSteps(int StepPos, int ClientID, std::deque<CQuestStep>* pElem)
{
	// clear old steps
	if(!(*pElem).empty())
		(*pElem).clear();

	// prepare new steps
	for(const auto& Step : m_vSteps[StepPos])
	{
		CQuestStep Base;
		Base.m_ClientID = ClientID;
		Base.m_Bot = Step.m_Bot;
		Base.m_StepComplete = false;
		Base.m_ClientQuitting = false;
		Base.m_aMobProgress.clear();
		(*pElem).push_back(std::move(Base));
	}
}
