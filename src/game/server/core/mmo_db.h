#ifndef GAME_SERVER_CORE_MMO_DB_H
#define GAME_SERVER_CORE_MMO_DB_H

#include <engine/server/db/db_async.h>

namespace MmoDb
{
	Db::CResultSet ListLinks();

	Db::CRequest TopGuildsByLevel(int Rows, std::string_view Tag = {});
	Db::CRequest TopGuildsByBank(int Rows, std::string_view Tag = {});
	Db::CRequest TopAccountsByRating(int Rows, std::string_view Tag = {});
	Db::CRequest TopAccountsByBank(int Rows, std::string_view Tag = {});
	Db::CRequest TopDungeonRecords(int DungeonID, int Rows, std::string_view Tag = {});
	Db::CRequest TopRhythmRecords(int WorldID, std::string_view Difficulty, int Rows, std::string_view Tag = {});
}

#endif // GAME_SERVER_CORE_MMO_DB_H
