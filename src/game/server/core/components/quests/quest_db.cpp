#include "quest_db.h"

#include <base/system.h>

namespace QuestDb
{
	namespace
	{
		constexpr Db::EKind KIND_QUESTS_LIST = Db::KindOfTable("tw_quests_list");
		constexpr Db::EKind KIND_QUEST_BOARDS = Db::KindOfTable("tw_quest_boards");
		constexpr Db::EKind KIND_BOARD_LIST = Db::KindOfTable("tw_quests_board_list");
		constexpr Db::EKind KIND_ACCOUNTS_QUESTS = Db::KindOfTable("tw_accounts_quests");
	}

	Db::CResultSet ListQuestDescriptions()
	{
		return Db::Query(KIND_QUESTS_LIST, "SELECT * FROM tw_quests_list").Result();
	}

	Db::CResultSet ListBoards()
	{
		return Db::Query(KIND_QUEST_BOARDS, "SELECT * FROM tw_quest_boards").Result();
	}

	Db::CResultSet ListBoardQuests()
	{
		return Db::Query(KIND_BOARD_LIST, "SELECT * FROM tw_quests_board_list").Result();
	}

	Db::CRequest LoadAccountQuests(int AccountID)
	{
		return Db::Query(KIND_ACCOUNTS_QUESTS, "SELECT * FROM tw_accounts_quests WHERE UserID = ?", Db::Bind(AccountID));
	}

	Db::CRequest CreateAccountQuest(int QuestID, int AccountID, int State)
	{
		return Db::Query(KIND_ACCOUNTS_QUESTS, "INSERT INTO tw_accounts_quests (QuestID, UserID, Type) VALUES (?, ?, ?)",
			Db::Bind(QuestID, AccountID, State));
	}

	Db::CRequest SaveAccountQuestState(int QuestID, int AccountID, int State)
	{
		return Db::Query(KIND_ACCOUNTS_QUESTS, "UPDATE tw_accounts_quests SET Type = ? WHERE QuestID = ? AND UserID = ?",
			Db::Bind(State, QuestID, AccountID));
	}

	Db::CRequest DeleteAccountQuest(int QuestID, int AccountID)
	{
		return Db::Query(KIND_ACCOUNTS_QUESTS, "DELETE FROM tw_accounts_quests WHERE QuestID = ? AND UserID = ?",
			Db::Bind(QuestID, AccountID));
	}

	Db::CRequest DeleteAccountQuests(const std::vector<int>& vQuestIDs, int AccountID)
	{
		// Keep the empty-list case valid SQL and make its no-op semantics explicit.
		if(vQuestIDs.empty())
			return Db::Query(KIND_ACCOUNTS_QUESTS, "DELETE FROM tw_accounts_quests WHERE 1 = 0");

		std::string Placeholders;
		for(size_t i = 0; i < vQuestIDs.size(); ++i)
			Placeholders += i == 0 ? "?" : ", ?";

		Db::CBindings Bindings;
		for(const int QuestID : vQuestIDs)
			Bindings.Add(QuestID);
		Bindings.Add(AccountID);

		return Db::Query(KIND_ACCOUNTS_QUESTS,
			fmt_default("DELETE FROM tw_accounts_quests WHERE QuestID IN ({}) AND UserID = ?", Placeholders), Bindings);
	}
}
