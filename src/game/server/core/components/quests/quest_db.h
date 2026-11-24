#ifndef GAME_SERVER_CORE_COMPONENTS_QUESTS_QUEST_DB_H
#define GAME_SERVER_CORE_COMPONENTS_QUESTS_QUEST_DB_H

#include <engine/server/db/db_async.h>

namespace QuestDb
{
	Db::CResultSet ListQuestDescriptions();
	Db::CResultSet ListBoards();
	Db::CResultSet ListBoardQuests();

	Db::CRequest LoadAccountQuests(int AccountID);
	Db::CRequest CreateAccountQuest(int QuestID, int AccountID, int State);
	Db::CRequest SaveAccountQuestState(int QuestID, int AccountID, int State);
	Db::CRequest DeleteAccountQuest(int QuestID, int AccountID);

	// Resets the daily quests in one statement: the id list is a placeholder run.
	Db::CRequest DeleteAccountQuests(const std::vector<int>& vQuestIDs, int AccountID);
}

#endif // GAME_SERVER_COMPONENT_QUESTS_QUEST_DB_H
