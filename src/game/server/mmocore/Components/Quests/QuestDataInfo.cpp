/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "QuestDataInfo.h"

#include <algorithm>

std::string CQuestDataInfo::GetJsonFileName(int AccountID) const { return "server_data/quest_tmp/" + std::to_string(m_ID) + "-" + std::to_string(AccountID) + ".json"; }

int CQuestDataInfo::GetQuestStoryPosition() const
{
	// get position of quests storyline
	return (int)std::count_if(Data().begin(), Data().end(), [this](std::pair< const int, CQuestDataInfo>& pItem)
	{
		return str_comp(pItem.second.m_aStoryLine, m_aStoryLine) == 0 && m_ID >= pItem.first;
	});
}

int CQuestDataInfo::GetQuestStorySize() const
{
	// get size of quests storyline
	return (int)std::count_if(Data().begin(), Data().end(), [this](std::pair< const int, CQuestDataInfo>& pItem)
	{
		return str_comp(pItem.second.m_aStoryLine, m_aStoryLine) == 0;
	});
}