#ifndef GAME_SERVER_CORE_COMPONENTS_DUTIES_DUTIES_DB_H
#define GAME_SERVER_CORE_COMPONENTS_DUTIES_DUTIES_DB_H

#include <engine/server/db/db_async.h>

namespace DutiesDb
{
	Db::CResultSet ListDungeons();
	Db::CRequest ReadBestDungeonPlayer();
	Db::CRequest ReadBestRhythmPlayer();
}

#endif // GAME_SERVER_COMPONENT_DUTIES_DUTIES_DB_H
