#include "house_db.h"
#include "house_data.h"

#include <base/system.h>

namespace HouseDb
{
	Db::CResultSet ListHouses(int WorldID)
	{
		return Db::Query(Db::KindOfTable(TW_HOUSES_TABLE), "SELECT * FROM tw_houses WHERE WorldID = ?", Db::Bind(WorldID)).Result();
	}

	Db::CRequest SaveBank(int HouseID, std::string_view Bank)
	{
		return Db::Query(Db::KindOfTable(TW_HOUSES_TABLE), fmt_default("UPDATE {} SET Bank = ? WHERE ID = ?", TW_HOUSES_TABLE),
			Db::Bind(Bank, HouseID));
	}

	Db::CRequest SaveRentDays(int HouseID, int64_t RentEndTimestamp)
	{
		return Db::Query(Db::KindOfTable(TW_HOUSES_TABLE), fmt_default("UPDATE {} SET RentDays = ? WHERE ID = ?", TW_HOUSES_TABLE),
			Db::Bind(RentEndTimestamp, HouseID));
	}

	Db::CRequest ResetBankAndRent(int HouseID, int64_t RentEndTimestamp)
	{
		return Db::Query(Db::KindOfTable(TW_HOUSES_TABLE), fmt_default("UPDATE {} SET Bank = 0, RentDays = ? WHERE ID = ?", TW_HOUSES_TABLE),
			Db::Bind(RentEndTimestamp, HouseID));
	}

	Db::CRequest ResetBank(int HouseID)
	{
		return Db::Query(Db::KindOfTable(TW_HOUSES_TABLE), fmt_default("UPDATE {} SET Bank = 0 WHERE ID = ?", TW_HOUSES_TABLE),
			Db::Bind(HouseID));
	}

	namespace Shared
	{
		Db::CRequest SaveFarmzones(std::string_view Table, int HouseID, std::string_view Json)
		{
			return Db::Query(Db::KindOfTable(Table), fmt_default("UPDATE {} SET Farmzones = ? WHERE ID = ?", Table),
				Db::Bind(Json, HouseID));
		}

		Db::CResultSet ListDecorations(std::string_view Table, int WorldID, int HouseID)
		{
			return Db::Query(Db::KindOfTable(Table),
				fmt_default("SELECT * FROM {} WHERE WorldID = ? AND HouseID = ?", Table), Db::Bind(WorldID, HouseID)).Result();
		}

		Db::CRequest CreateDecoration(std::string_view Table, int ItemID, int HouseID, int PosX, int PosY, int WorldID)
		{
			return Db::Query(Db::KindOfTable(Table),
				fmt_default("INSERT INTO {} (ItemID, HouseID, PosX, PosY, WorldID) VALUES (?, ?, ?, ?, ?)", Table),
				Db::Bind(ItemID, HouseID, PosX, PosY, WorldID));
		}

		Db::CRequest DeleteDecoration(std::string_view Table, int HouseID, int ItemID, int PosX, int PosY)
		{
			return Db::Query(Db::KindOfTable(Table),
				fmt_default("DELETE FROM {} WHERE HouseID = ? AND ItemID = ? AND PosX = ? AND PosY = ?", Table),
				Db::Bind(HouseID, ItemID, PosX, PosY));
		}
	};
}
