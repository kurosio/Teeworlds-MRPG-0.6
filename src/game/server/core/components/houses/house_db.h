#ifndef GAME_SERVER_CORE_COMPONENTS_HOUSES_HOUSE_DB_H
#define GAME_SERVER_CORE_COMPONENTS_HOUSES_HOUSE_DB_H

#include <engine/server/db/db_async.h>

namespace HouseDb
{
	// house tables
	Db::CResultSet ListHouses(int WorldID);
	Db::CRequest SaveRentDays(int HouseID, int64_t RentEndTimestamp);
	Db::CRequest ResetBankAndRent(int HouseID, int64_t RentEndTimestamp);
	Db::CRequest SaveBank(int HouseID, std::string_view Bank);
	Db::CRequest ResetBank(int HouseID);

	namespace Shared
	{
		// house and guild house same tables
		Db::CResultSet ListDecorations(std::string_view Table, int WorldID, int HouseID);
		Db::CRequest CreateDecoration(std::string_view Table, int ItemID, int HouseID, int PosX, int PosY, int WorldID);
		Db::CRequest DeleteDecoration(std::string_view Table, int HouseID, int ItemID, int PosX, int PosY);
	};
}

#endif // GAME_SERVER_COMPONENT_HOUSES_HOUSE_DB_H
