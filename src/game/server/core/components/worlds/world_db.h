#ifndef GAME_SERVER_CORE_COMPONENTS_WORLDS_WORLD_DB_H
#define GAME_SERVER_CORE_COMPONENTS_WORLDS_WORLD_DB_H

#include <engine/server/db/db_async.h>

namespace WorldDb
{
	Db::CResultSet ListCollectingNodes(int WorldID);
	Db::CResultSet ListSwappers(int WorldID);
}

#endif // GAME_SERVER_COMPONENT_WORLDS_WORLD_DB_H
