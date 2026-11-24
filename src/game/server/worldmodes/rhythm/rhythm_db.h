#ifndef GAME_SERVER_WORLDMODES_RHYTHM_RHYTHM_DB_H
#define GAME_SERVER_WORLDMODES_RHYTHM_RHYTHM_DB_H

#include <engine/server/db/db_async.h>

namespace RhythmDb
{
	Db::CRequest ImproveRecord(int AccountID, int WorldID, std::string_view Difficulty, int Points);
	Db::CRequest CreateRecordIfMissing(int AccountID, int WorldID, std::string_view Difficulty, int Points);
}

#endif // GAME_SERVER_WORLDMODES_RHYTHM_RHYTHM_DB_H
