#include "achievement_db.h"

namespace AchievementDb
{
	namespace
	{
		constexpr Db::EKind KIND_ACHIEVEMENTS = Db::KindOfTable("tw_achievements");
		constexpr Db::EKind KIND_PROGRESS = Db::KindOfTable("tw_accounts_achievements");
	}

	Db::CResultSet ListAchievements()
	{
		return Db::Query(KIND_ACHIEVEMENTS,
			"SELECT ID, Type, Criteria, Required, Name, Reward, AchievementPoint FROM tw_achievements").Result();
	}

	Db::CRequest LoadProgress(int AccountID)
	{
		return Db::Query(KIND_PROGRESS,
			"SELECT AchievementType, Criteria, Progress, CompletedLevel FROM tw_accounts_achievements WHERE AccountID = ?",
			Db::Bind(AccountID));
	}

	Db::CRequest SaveProgress(int AccountID, int AchievementType, int Criteria, int Progress, int CompletedLevel)
	{
		return Db::Query(KIND_PROGRESS,
			"INSERT INTO tw_accounts_achievements (AccountID, AchievementType, Criteria, Progress, CompletedLevel, UpdatedAt) "
			"VALUES (?, ?, ?, ?, ?, CURRENT_TIMESTAMP) "
			"ON DUPLICATE KEY UPDATE Progress = GREATEST(Progress, ?), CompletedLevel = GREATEST(CompletedLevel, ?), UpdatedAt = CURRENT_TIMESTAMP",
			Db::Bind(AccountID, AchievementType, Criteria, Progress, CompletedLevel, Progress, CompletedLevel));
	}

	Db::CRequest SaveCompletedLevel(int AccountID, int AchievementType, int Criteria, int CompletedLevel)
	{
		// A lower level of a chain finished, but the chain is still in progress.
		// Remember the reward grant without overwriting the progress written by
		// the last level (absolute progress can legitimately go down).
		return Db::Query(KIND_PROGRESS,
			"INSERT INTO tw_accounts_achievements (AccountID, AchievementType, Criteria, Progress, CompletedLevel, UpdatedAt) "
			"VALUES (?, ?, ?, 0, ?, CURRENT_TIMESTAMP) "
			"ON DUPLICATE KEY UPDATE CompletedLevel = GREATEST(CompletedLevel, ?), UpdatedAt = CURRENT_TIMESTAMP",
			Db::Bind(AccountID, AchievementType, Criteria, CompletedLevel, CompletedLevel));
	}
}
