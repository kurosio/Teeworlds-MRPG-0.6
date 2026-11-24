#include "guild_db.h"

#include "guild_data.h"

#include <components/houses/guild_house_data.h>

#include <base/system.h>

namespace GuildDb
{
	namespace
	{
		// Endpoints of the tables this file writes (resolved once, see Db::KindOfTable).
		constexpr Db::EKind KIND_GUILDS = Db::KindOfTable(TW_GUILDS_TABLE);
		constexpr Db::EKind KIND_GUILDS_HOUSES = Db::KindOfTable(TW_GUILDS_HOUSES);
		constexpr Db::EKind KIND_GUILDS_HISTORY = Db::KindOfTable(TW_GUILDS_HISTORY_TABLE);
		constexpr Db::EKind KIND_GUILDS_RANKS = Db::KindOfTable(TW_GUILDS_RANKS_TABLE);
		constexpr Db::EKind KIND_GUILDS_INVITES = Db::KindOfTable(TW_GUILDS_INVITES_TABLE);
	}

	// ---------------------------------------------------------------------------
	// tw_guilds
	// ---------------------------------------------------------------------------
	Db::CResultSet ListGuilds()
	{
		return Db::Query(KIND_GUILDS, "SELECT * FROM tw_guilds").Result();
	}

	Db::CRequest ReadGuild(int GuildID)
	{
		return Db::Query(KIND_GUILDS, "SELECT * FROM tw_guilds WHERE ID = ?", Db::Bind(GuildID));
	}

	Db::CRequest FindGuildByName(std::string_view Name)
	{
		return Db::Query(KIND_GUILDS, "SELECT ID FROM tw_guilds WHERE Name = ?", Db::Bind(Name));
	}

	Db::CRequest ReadNextGuildID()
	{
		return Db::Query(KIND_GUILDS, "SELECT COALESCE(MAX(ID), 0) + 1 AS NextID FROM tw_guilds", {});
	}

	Db::CRequest CreateGuild(int GuildID, std::string_view Name, int LeaderUID, std::string_view Members)
	{
		return Db::Query(KIND_GUILDS, "INSERT INTO tw_guilds (ID, Name, LeaderUID, Members) VALUES (?, ?, ?, ?)",
			Db::Bind(GuildID, Name, LeaderUID, Members));
	}

	Db::CRequest DeleteGuild(int GuildID)
	{
		return Db::Query(KIND_GUILDS, "DELETE FROM tw_guilds WHERE ID = ?", Db::Bind(GuildID));
	}

	Db::CRequest SaveUpgradeField(int GuildID, std::string_view FieldName, int Value)
	{
		// Identifiers cannot be parameterized. Restrict the column to the schema's
		// fixed upgrade fields rather than trusting a caller-provided string.
		const char* pField = "__invalid_upgrade_field__";
		if(FieldName == "AvailableSlots")
			pField = "AvailableSlots";
		else if(FieldName == "ChairLevel")
			pField = "ChairLevel";
		else if(FieldName == "DecorationSlots")
			pField = "DecorationSlots";
		return Db::Query(KIND_GUILDS, fmt_default("UPDATE tw_guilds SET {} = ? WHERE ID = ?", pField),
			Db::Bind(Value, GuildID));
	}

	Db::CRequest SaveLevelAndExp(int GuildID, int Level, int Experience)
	{
		return Db::Query(KIND_GUILDS, "UPDATE tw_guilds SET Level = ?, Exp = ? WHERE ID = ?",
			Db::Bind(Level, Experience, GuildID));
	}

	Db::CRequest SaveLeaderUID(int GuildID, int LeaderUID)
	{
		return Db::Query(KIND_GUILDS, "UPDATE tw_guilds SET LeaderUID = ? WHERE ID = ?", Db::Bind(LeaderUID, GuildID));
	}

	Db::CRequest SaveBank(int GuildID, std::string_view Bank)
	{
		return Db::Query(KIND_GUILDS, "UPDATE tw_guilds SET Bank = ? WHERE ID = ?", Db::Bind(Bank, GuildID));
	}

	Db::CRequest SaveLogFlag(int GuildID, int LogFlag)
	{
		return Db::Query(KIND_GUILDS, "UPDATE tw_guilds SET LogFlag = ? WHERE ID = ?", Db::Bind(LogFlag, GuildID));
	}

	Db::CRequest SaveDefaultRankAndMembers(int GuildID, int DefaultRankID, std::string_view Members)
	{
		// A member list is rebuilt often; the delay keeps the last write instead of
		// queueing every intermediate state.
		return Db::Query(KIND_GUILDS, "UPDATE tw_guilds SET DefaultRankID = ?, Members = ? WHERE ID = ?",
			Db::Bind(DefaultRankID, Members, GuildID), Db::SQueryOptions().Delay(300));
	}

	// ---------------------------------------------------------------------------
	// tw_guilds_ranks
	// ---------------------------------------------------------------------------
	Db::CResultSet ListRanks(int GuildID)
	{
		return Db::Query(KIND_GUILDS_RANKS, "SELECT * FROM tw_guilds_ranks WHERE GuildID = ? ORDER BY ID",
			Db::Bind(GuildID)).Result();
	}

	Db::CRequest ReadNextRankID()
	{
		return Db::Query(KIND_GUILDS_RANKS, "SELECT COALESCE(MAX(ID), 0) + 1 AS NextID FROM tw_guilds_ranks");
	}

	Db::CRequest CreateRank(int RankID, int64_t Rights, int GuildID, std::string_view Name)
	{
		return Db::Query(KIND_GUILDS_RANKS, "INSERT INTO tw_guilds_ranks (ID, Rights, GuildID, Name) VALUES (?, ?, ?, ?)",
			Db::Bind(RankID, Rights, GuildID, Name));
	}

	Db::CRequest SaveRankName(int RankID, std::string_view Name)
	{
		return Db::Query(KIND_GUILDS_RANKS, "UPDATE tw_guilds_ranks SET Name = ? WHERE ID = ?", Db::Bind(Name, RankID));
	}

	Db::CRequest SaveRankRights(int RankID, int64_t Rights)
	{
		return Db::Query(KIND_GUILDS_RANKS, "UPDATE tw_guilds_ranks SET Rights = ? WHERE ID = ?", Db::Bind(Rights, RankID));
	}

	Db::CRequest DeleteRank(int RankID)
	{
		return Db::Query(KIND_GUILDS_RANKS, "DELETE FROM tw_guilds_ranks WHERE ID = ?", Db::Bind(RankID));
	}

	Db::CRequest DeleteRanksOfGuild(int GuildID)
	{
		return Db::Query(KIND_GUILDS_RANKS, "DELETE FROM tw_guilds_ranks WHERE GuildID = ?", Db::Bind(GuildID));
	}

	// ---------------------------------------------------------------------------
	// tw_guilds_history
	// ---------------------------------------------------------------------------
	Db::CRequest AddHistoryEntry(int GuildID, std::string_view Text, std::string_view Timestamp)
	{
		return Db::Query(KIND_GUILDS_HISTORY, "INSERT INTO tw_guilds_history (GuildID, Text, Time) VALUES (?, ?, ?)",
			Db::Bind(GuildID, Text, Timestamp));
	}

	Db::CResultSet ListHistory(int GuildID, int Limit)
	{
		return Db::Query(KIND_GUILDS_HISTORY,
			"SELECT * FROM tw_guilds_history WHERE GuildID = ? ORDER BY ID DESC LIMIT ?", Db::Bind(GuildID, Limit)).Result();
	}

	Db::CRequest DeleteHistoryOfGuild(int GuildID)
	{
		return Db::Query(KIND_GUILDS_HISTORY, "DELETE FROM tw_guilds_history WHERE GuildID = ?", Db::Bind(GuildID));
	}

	// ---------------------------------------------------------------------------
	// tw_guilds_invites
	// ---------------------------------------------------------------------------
	Db::CResultSet ListInvites(int GuildID)
	{
		return Db::Query(KIND_GUILDS_INVITES, "SELECT * FROM tw_guilds_invites WHERE GuildID = ?",
			Db::Bind(GuildID)).Result();
	}

	Db::CRequest CreateInvite(int GuildID, int UserID)
	{
		return Db::Query(KIND_GUILDS_INVITES, "INSERT INTO tw_guilds_invites (GuildID, UserID) VALUES (?, ?)",
			Db::Bind(GuildID, UserID));
	}

	Db::CRequest DeleteInvite(int GuildID, int UserID)
	{
		return Db::Query(KIND_GUILDS_INVITES, "DELETE FROM tw_guilds_invites WHERE GuildID = ? AND UserID = ?",
			Db::Bind(GuildID, UserID));
	}

	Db::CRequest DeleteInvitesOfGuild(int GuildID)
	{
		return Db::Query(KIND_GUILDS_INVITES, "DELETE FROM tw_guilds_invites WHERE GuildID = ?", Db::Bind(GuildID));
	}

	// ---------------------------------------------------------------------------
	// tw_guilds_houses
	// ---------------------------------------------------------------------------
	Db::CResultSet ListHouses(int WorldID)
	{
		return Db::Query(KIND_GUILDS_HOUSES, "SELECT * FROM tw_guilds_houses WHERE WorldID = ?", Db::Bind(WorldID)).Result();
	}

	Db::CRequest SaveHouseRentDays(int64_t RentEndTimestamp, int HouseID)
	{
		return Db::Query(KIND_GUILDS_HOUSES, "UPDATE tw_guilds_houses SET RentDays = ? WHERE ID = ?",
			Db::Bind(RentEndTimestamp, HouseID));
	}
}
