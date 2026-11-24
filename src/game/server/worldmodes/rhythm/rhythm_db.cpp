#include "rhythm_db.h"

namespace RhythmDb
{
	namespace
	{
		constexpr Db::EKind KIND_RECORDS = Db::KindOfTable("tw_rhythm_records");
	}

	Db::CRequest ImproveRecord(int AccountID, int WorldID, std::string_view Difficulty, int Points)
	{
		return Db::Query(KIND_RECORDS, "UPDATE tw_rhythm_records SET Score = ? "
			"WHERE UserID = ? AND WorldID = ? AND Difficulty = ? AND Score < ?",
			Db::Bind(Points, AccountID, WorldID, Difficulty, Points));
	}

	Db::CRequest CreateRecordIfMissing(int AccountID, int WorldID, std::string_view Difficulty, int Points)
	{
		return Db::Query(KIND_RECORDS, "INSERT INTO tw_rhythm_records (UserID, WorldID, Difficulty, Score) "
			"SELECT ?, ?, ?, ? FROM DUAL WHERE NOT EXISTS ("
			"SELECT 1 FROM tw_rhythm_records WHERE UserID = ? AND WorldID = ? AND Difficulty = ?)",
			Db::Bind(AccountID, WorldID, Difficulty, Points, AccountID, WorldID, Difficulty));
	}
}
