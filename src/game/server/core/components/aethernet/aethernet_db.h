#ifndef GAME_SERVER_CORE_COMPONENTS_AETHERNET_AETHERNET_DB_H
#define GAME_SERVER_CORE_COMPONENTS_AETHERNET_AETHERNET_DB_H

#include <engine/server/db/db_async.h>

namespace AethernetDb
{
	Db::CResultSet ListAethers();
	Db::CRequest ListUnlocked(int AccountID);
	Db::CRequest Unlock(int AccountID, int AetherID);
}

#endif // GAME_SERVER_COMPONENT_AETHERNET_AETHERNET_DB_H
