#ifndef GAME_SERVER_CORE_COMPONENTS_CRAFTING_CRAFT_DB_H
#define GAME_SERVER_CORE_COMPONENTS_CRAFTING_CRAFT_DB_H

#include <engine/server/db/db_async.h>

namespace CraftDb
{
	Db::CResultSet ListCrafts(int WorldID);
}

#endif // GAME_SERVER_COMPONENT_CRAFTING_CRAFT_DB_H
