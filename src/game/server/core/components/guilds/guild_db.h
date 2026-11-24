#ifndef GAME_SERVER_CORE_COMPONENTS_GUILDS_GUILD_DB_H
#define GAME_SERVER_CORE_COMPONENTS_GUILDS_GUILD_DB_H

#include <engine/server/db/db_async.h>

namespace GuildDb
{
	// --- tw_guilds ----------------------------------------------------------
	Db::CResultSet ListGuilds();
	Db::CRequest ReadGuild(int GuildID);
	Db::CRequest FindGuildByName(std::string_view Name);
	Db::CRequest ReadNextGuildID();
	Db::CRequest CreateGuild(int GuildID, std::string_view Name, int LeaderUID, std::string_view Members);
	Db::CRequest DeleteGuild(int GuildID);

	// The implementation maps the requested upgrade name to a fixed column allowlist.
	Db::CRequest SaveUpgradeField(int GuildID, std::string_view FieldName, int Value);
	Db::CRequest SaveLevelAndExp(int GuildID, int Level, int Experience);
	Db::CRequest SaveLeaderUID(int GuildID, int LeaderUID);
	Db::CRequest SaveBank(int GuildID, std::string_view Bank);
	Db::CRequest SaveLogFlag(int GuildID, int LogFlag);
	Db::CRequest SaveDefaultRankAndMembers(int GuildID, int DefaultRankID, std::string_view Members);

	// --- tw_guilds_ranks ----------------------------------------------------
	Db::CResultSet ListRanks(int GuildID);
	Db::CRequest ReadNextRankID();
	Db::CRequest CreateRank(int RankID, int64_t Rights, int GuildID, std::string_view Name);
	Db::CRequest SaveRankName(int RankID, std::string_view Name);
	Db::CRequest SaveRankRights(int RankID, int64_t Rights);
	Db::CRequest DeleteRank(int RankID);
	Db::CRequest DeleteRanksOfGuild(int GuildID);

	// --- tw_guilds_history --------------------------------------------------
	Db::CRequest AddHistoryEntry(int GuildID, std::string_view Text, std::string_view Timestamp);
	Db::CResultSet ListHistory(int GuildID, int Limit);
	Db::CRequest DeleteHistoryOfGuild(int GuildID);

	// --- tw_guilds_invites --------------------------------------------------
	Db::CResultSet ListInvites(int GuildID);
	Db::CRequest CreateInvite(int GuildID, int UserID);
	Db::CRequest DeleteInvite(int GuildID, int UserID);
	Db::CRequest DeleteInvitesOfGuild(int GuildID);

	// --- tw_guilds_houses ---------------------------------------------------
	Db::CResultSet ListHouses(int WorldID);
	Db::CRequest SaveHouseRentDays(int64_t RentEndTimestamp, int HouseID);
}

#endif // GAME_SERVER_COMPONENT_GUILDS_GUILD_DB_H
