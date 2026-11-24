#include "achievement_db.h"

namespace AchievementDb
{
	Db::CResultSet ListAchievements()
	{
		return Db::Query(Db::KindOfTable("tw_achievements"), "SELECT * FROM tw_achievements").Result();
	}
}
