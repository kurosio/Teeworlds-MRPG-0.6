/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "quest_board_data.h"

#include <game/server/player.h>

std::deque<int> CQuestsBoard::GetUnfinishedQuestsByFlag(CPlayer* pPlayer, int Flag)
{
	std::deque<int> vResult {};
	for(auto& QuestID : m_vpQuestsList)
	{
		const auto* pQuest = pPlayer->GetQuest(QuestID);
		if(!pQuest)
			continue;

		if(!pQuest->Info()->HasFlag(Flag))
			continue;

		if(pQuest->GetState() != QuestState::Finished)
		{
			vResult.push_back(QuestID);
		}
	}

	return vResult;
}