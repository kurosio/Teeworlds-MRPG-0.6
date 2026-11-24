#include "warehouse_db.h"

namespace WarehouseDb
{
	namespace
	{
		constexpr Db::EKind KIND = Db::KindOfTable("tw_warehouses");
	}

	Db::CResultSet ListWarehouses()
	{
		return Db::Query(KIND, "SELECT * FROM tw_warehouses").Result();
	}

	Db::CRequest SaveStorage(int WarehouseID, std::string_view Json)
	{
		return Db::Query(KIND, "UPDATE tw_warehouses SET StorageData = ? WHERE ID = ?", Db::Bind(Json, WarehouseID));
	}
}
