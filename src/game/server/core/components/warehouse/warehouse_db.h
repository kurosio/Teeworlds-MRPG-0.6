#ifndef GAME_SERVER_CORE_COMPONENTS_WAREHOUSE_WAREHOUSE_DB_H
#define GAME_SERVER_CORE_COMPONENTS_WAREHOUSE_WAREHOUSE_DB_H

#include <engine/server/db/db_async.h>

namespace WarehouseDb
{
	Db::CResultSet ListWarehouses();
	Db::CRequest SaveStorage(int WarehouseID, std::string_view Json);
}

#endif // GAME_SERVER_COMPONENT_WAREHOUSE_WAREHOUSE_DB_H
