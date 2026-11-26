#ifndef GAME_SERVER_CORE_COMPONENTS_ACHIEVEMENTS_ACHIEVEMENT_DB_H
#define GAME_SERVER_CORE_COMPONENTS_ACHIEVEMENTS_ACHIEVEMENT_DB_H

#include <engine/server/db/db_async.h>

namespace AchievementDb
{
	Db::CResultSet ListAchievements();
	Db::CRequest LoadProgress(int AccountID);
	Db::CRequest SaveProgress(int AccountID, int AchievementType, int Criteria, int Progress, int CompletedLevel);
	Db::CRequest SaveCompletedLevel(int AccountID, int AchievementType, int Criteria, int CompletedLevel);
}

#endif // GAME_SERVER_CORE_COMPONENTS_ACHIEVEMENTS_ACHIEVEMENT_DB_H
