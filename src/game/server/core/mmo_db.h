#ifndef GAME_SERVER_CORE_MMO_DB_H
#define GAME_SERVER_CORE_MMO_DB_H

#include <engine/server/db/db_async.h>

namespace MmoDb
{
	Db::CResultSet ListLinks();

	Db::CRequest TopGuildsByLevel(int Rows);
	Db::CRequest TopGuildsByBank(int Rows);
	Db::CRequest TopAccountsByRating(int Rows);
	Db::CRequest TopAccountsByBank(int Rows);
	Db::CRequest TopDungeonRecords(int DungeonID, int Rows);
	Db::CRequest TopRhythmRecords(int WorldID, std::string_view Difficulty, int Rows);
}

#endif // GAME_SERVER_CORE_MMO_DB_H
