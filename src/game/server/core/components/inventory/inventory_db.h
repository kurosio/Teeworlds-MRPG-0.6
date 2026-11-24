#ifndef GAME_SERVER_CORE_COMPONENTS_INVENTORY_INVENTORY_DB_H
#define GAME_SERVER_CORE_COMPONENTS_INVENTORY_INVENTORY_DB_H

#include <engine/server/db/db_async.h>

namespace InventoryDb
{
	Db::CResultSet ListItemDescriptions();
	Db::CResultSet ListAttributes();

	Db::CRequest LoadAccountItems(int AccountID);
	Db::CRequest RepairDurability(int AccountID);

	Db::CRequest DeleteAccountItem(int ItemID, int AccountID);
	Db::CRequest SaveAccountItem(int ItemID, int AccountID, int Value, int Settings, int Enchant, int Durability, time_t ExpiresAt, std::string_view Data);
}

#endif // GAME_SERVER_COMPONENT_INVENTORY_INVENTORY_DB_H
