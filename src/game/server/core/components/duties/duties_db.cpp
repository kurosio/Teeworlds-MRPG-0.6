#include "duties_db.h"

namespace DutiesDb
{
	namespace
	{
		constexpr Db::EKind KIND_DUNGEONS = Db::KindOfTable("tw_dungeons");
		constexpr Db::EKind KIND_DUNGEON_RECORDS = Db::KindOfTable("tw_dungeons_records");
		constexpr Db::EKind KIND_RHYTHM_RECORDS = Db::KindOfTable("tw_rhythm_records");
	}

	Db::CResultSet ListDungeons()
	{
		return Db::Query(KIND_DUNGEONS, "SELECT * FROM tw_dungeons").Result();
	}

	Db::CRequest ReadBestDungeonPlayer()
	{
		// A dungeon run faster than one tick still scores, hence the ELSE branch.
		return Db::Query(KIND_DUNGEON_RECORDS,
			"SELECT UserID, SUM(CASE WHEN Time > 0 THEN GREATEST(1, 10000 / Time) ELSE 10000 END) AS Points "
			"FROM tw_dungeons_records GROUP BY UserID ORDER BY Points DESC LIMIT 1", {}, Db::SQueryOptions().Coalesce());
	}

	Db::CRequest ReadBestRhythmPlayer()
	{
		return Db::Query(KIND_RHYTHM_RECORDS,
			"SELECT UserID, SUM(Score) AS Points FROM tw_rhythm_records GROUP BY UserID ORDER BY Points DESC LIMIT 1", {},
			Db::SQueryOptions().Coalesce());
	}
}
