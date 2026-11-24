#ifndef GAME_SERVER_WORLDMODES_DUNGEON_DUNGEON_DB_H
#define GAME_SERVER_WORLDMODES_DUNGEON_DUNGEON_DB_H

#include <engine/server/db/db_async.h>

namespace DungeonDb
{
	Db::CRequest ImproveRecord(int AccountID, int DungeonID, int FinishTime);
	Db::CRequest CreateRecordIfMissing(int AccountID, int DungeonID, int FinishTime);
}

#endif // GAME_SERVER_WORLDMODES_DUNGEON_DUNGEON_DB_H
