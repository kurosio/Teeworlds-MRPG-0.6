#include "dungeon_db.h"

namespace DungeonDb
{
	namespace
	{
		constexpr Db::EKind KIND_RECORDS = Db::KindOfTable("tw_dungeons_records");
	}

	Db::CRequest ImproveRecord(int AccountID, int DungeonID, int FinishTime)
	{
		return Db::Query(KIND_RECORDS, "UPDATE tw_dungeons_records SET Time = ? "
			"WHERE UserID = ? AND DungeonID = ? AND Time > ?",
			Db::Bind(FinishTime, AccountID, DungeonID, FinishTime));
	}

	Db::CRequest CreateRecordIfMissing(int AccountID, int DungeonID, int FinishTime)
	{
		return Db::Query(KIND_RECORDS, "INSERT INTO tw_dungeons_records (UserID, DungeonID, Time) "
			"SELECT ?, ?, ? FROM DUAL WHERE NOT EXISTS ("
			"SELECT 1 FROM tw_dungeons_records WHERE UserID = ? AND DungeonID = ?)",
			Db::Bind(AccountID, DungeonID, FinishTime, AccountID, DungeonID));
	}
}
