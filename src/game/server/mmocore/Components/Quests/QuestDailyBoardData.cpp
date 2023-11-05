/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "QuestDailyBoardData.h"

#include <game/server/player.h>

bool CQuestsDailyBoard::IsQuestsAvailable(CPlayer* pPlayer)
{
	return (bool)(std::count_if(m_DailyQuestsInfoList.begin(), m_DailyQuestsInfoList.end(), [pPlayer](const auto& p)
	{
		// Get the quest with the given ID from the player's quest list
		CPlayerQuest* pQuest = pPlayer->GetQuest(p.GetID());
		return pQuest->GetState() >= QuestState::ACCEPT;
	}) < MAX_DAILY_QUESTS_BY_BOARD);
}

int CQuestsDailyBoard::GetCountQuestsByState(CPlayer* pPlayer, QuestState State) const
{
	return (int)std::count_if(m_DailyQuestsInfoList.begin(), m_DailyQuestsInfoList.end(), [pPlayer, &State](const auto& p)
	{
		// Get the quest with the given ID from the player's quest list
		CPlayerQuest* pQuest = pPlayer->GetQuest(p.GetID());
		return pQuest->GetState() == State;
	});
}

// ResetDailyQuests function is a member function of the CQuestsDailyBoard class
void CQuestsDailyBoard::ResetDailyQuests(CPlayer* pPlayer) const
{
	// Iterate through each item in the m_DailyQuestsInfoList
	for(auto& p : m_DailyQuestsInfoList)
	{
		// Get the quest with the corresponding ID from the player's quest list and reset player quest
		CPlayerQuest* pQuest = pPlayer->GetQuest(p.GetID());
		pQuest->Reset();
	}

	// Delete entries from the tw_accounts_quests table where UserID matches pPlayer's ID,
	// and QuestID matches the QuestID in tw_quests_daily_board_list table.
	Database->Execute<DB::OTHER>("DELETE tw_accounts_quests.* FROM tw_accounts_quests JOIN tw_quests_daily_board_list ON tw_quests_daily_board_list.QuestID = tw_accounts_quests.QuestID WHERE tw_accounts_quests.UserID = %d;", pPlayer->Acc().m_ID);
}
