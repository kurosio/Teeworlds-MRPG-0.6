#include "mmo_db.h"

namespace MmoDb
{
	namespace
	{
		constexpr Db::EKind KIND_LINKS = Db::KindOfTable("tw_links");
		constexpr Db::EKind KIND_GUILDS = Db::KindOfTable("tw_guilds");
		constexpr Db::EKind KIND_ACCOUNTS_DATA = Db::KindOfTable("tw_accounts_data");
		constexpr Db::EKind KIND_DUNGEON_RECORDS = Db::KindOfTable("tw_dungeons_records");
		constexpr Db::EKind KIND_RHYTHM_RECORDS = Db::KindOfTable("tw_rhythm_records");
	}

	Db::CResultSet ListLinks()
	{
		return Db::Query(KIND_LINKS, "SELECT * FROM tw_links").Result();
	}

	Db::CRequest TopGuildsByLevel(int Rows)
	{
		return Db::Query(KIND_GUILDS, "SELECT Name, Level, Exp FROM tw_guilds ORDER BY Level DESC, Exp DESC LIMIT ?",
			Db::Bind(Rows), Db::SQueryOptions().Coalesce());
	}

	Db::CRequest TopGuildsByBank(int Rows)
	{
		// the bank is a decimal stored as text, so the order is numeric via + 0
		return Db::Query(KIND_GUILDS, "SELECT Name, Bank FROM tw_guilds ORDER BY Bank + 0 DESC LIMIT ?",
			Db::Bind(Rows), Db::SQueryOptions().Coalesce());
	}

	Db::CRequest TopAccountsByRating(int Rows)
	{
		return Db::Query(KIND_ACCOUNTS_DATA, "SELECT Nick, Rating FROM tw_accounts_data ORDER BY Rating DESC LIMIT ?",
			Db::Bind(Rows), Db::SQueryOptions().Coalesce());
	}

	Db::CRequest TopAccountsByBank(int Rows)
	{
		return Db::Query(KIND_ACCOUNTS_DATA, "SELECT Nick, Bank FROM tw_accounts_data ORDER BY Bank + 0 DESC LIMIT ?",
			Db::Bind(Rows), Db::SQueryOptions().Coalesce());
	}

	Db::CRequest TopDungeonRecords(int DungeonID, int Rows)
	{
		return Db::Query(KIND_DUNGEON_RECORDS,
			"SELECT UserID, Time FROM tw_dungeons_records WHERE DungeonID = ? ORDER BY Time ASC LIMIT ?",
			Db::Bind(DungeonID, Rows), Db::SQueryOptions().Coalesce());
	}

	Db::CRequest TopRhythmRecords(int WorldID, std::string_view Difficulty, int Rows)
	{
		return Db::Query(KIND_RHYTHM_RECORDS,
			"SELECT UserID, Score FROM tw_rhythm_records WHERE WorldID = ? AND Difficulty = ? ORDER BY Score DESC LIMIT ?",
			Db::Bind(WorldID, Difficulty, Rows), Db::SQueryOptions().Coalesce());
	}
}
