/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "quest_board_data.h"

#include <game/server/player.h>

int CQuestsBoard::CountAvailableDailyQuests(CPlayer* pPlayer)
{
	return (int)std::ranges::count_if(m_vpQuests, [&pPlayer](const auto& p)
	{
		if(p->HasFlag(QUEST_FLAG_TYPE_DAILY))
		{
			CPlayerQuest* pQuest = pPlayer->GetQuest(p->GetID());
			return pQuest->GetState() != QuestState::FINISHED;
		}
		return false;
	});
}

int CQuestsBoard::CountAvailableWeeklyQuests(CPlayer* pPlayer)
{
	return (int)std::ranges::count_if(m_vpQuests, [&pPlayer](const auto& p)
	{
		if(p->HasFlag(QUEST_FLAG_TYPE_WEEKLY))
		{
			CPlayerQuest* pQuest = pPlayer->GetQuest(p->GetID());
			return pQuest->GetState() != QuestState::FINISHED;
		}
		return false;
	});
}


int CQuestsBoard::CountAvailableRepeatableQuests(CPlayer* pPlayer)
{
	return (int)std::ranges::count_if(m_vpQuests, [&pPlayer](const auto& p)
	{
		if(p->HasFlag(QUEST_FLAG_TYPE_REPEATABLE))
		{
			CPlayerQuest* pQuest = pPlayer->GetQuest(p->GetID());
			return pQuest->GetState() != QuestState::FINISHED;
		}
		return false;
	});
}

int CQuestsBoard::CountAvailableSideQuests(CPlayer* pPlayer)
{
	return (int)std::ranges::count_if(m_vpQuests, [&pPlayer](const auto& p)
	{
		if(p->HasFlag(QUEST_FLAG_TYPE_SIDE))
		{
			CPlayerQuest* pQuest = pPlayer->GetQuest(p->GetID());
			return pQuest->GetState() != QuestState::FINISHED;
		}
		return false;
	});
}
