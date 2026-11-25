#include "house_db.h"
#include "house_data.h"

#include <engine/server/links.h>
#include <base/system.h>

namespace HouseDb
{
	Db::CResultSet ListHouses(int WorldID)
	{
		return Db::Query(Db::KindOfTable(TW_HOUSES_TABLE), "SELECT * FROM tw_houses WHERE WorldID = ?", Db::Bind(WorldID)).Result();
	}

	Db::CRequest SaveBank(int HouseID, std::string_view Bank)
	{
		return Links::SaveStringMeta("player_house", HouseID, "bank", Bank);
	}

	Db::CRequest SaveRentDays(int HouseID, int64_t RentEndTimestamp)
	{
		return Links::SaveNumberMeta("player_house", HouseID, "rent_end", RentEndTimestamp);
	}

	Db::CRequest ResetBankAndRent(int HouseID, int64_t RentEndTimestamp)
	{
		return Links::SavePlayerHouseState(HouseID, RentEndTimestamp);
	}

	Db::CRequest ResetBank(int HouseID)
	{
		return Links::SaveStringMeta("player_house", HouseID, "bank", "0");
	}

	namespace Shared
	{
		Db::CResultSet ListDecorations(std::string_view Table, int WorldID, int HouseID)
		{
			return Db::Query(Db::KindOfTable(Table),
				fmt_default("SELECT * FROM {} WHERE WorldID = ? AND HouseID = ?", Table), Db::Bind(WorldID, HouseID)).Result();
		}

		Db::CRequest CreateDecoration(std::string_view Table, int ItemID, int HouseID, int PosX, int PosY, int WorldID)
		{
			return Db::Query(Db::KindOfTable(Table),
				fmt_default("INSERT INTO {} (ItemID, HouseID, PosX, PosY, WorldID) VALUES (?, ?, ?, ?, ?)", Table), Db::Bind(ItemID, HouseID, PosX, PosY, WorldID));
		}

		Db::CRequest DeleteDecoration(std::string_view Table, int HouseID, int ItemID, int PosX, int PosY)
		{
			return Db::Query(Db::KindOfTable(Table),
				fmt_default("DELETE FROM {} WHERE HouseID = ? AND ItemID = ? AND PosX = ? AND PosY = ?", Table), Db::Bind(HouseID, ItemID, PosX, PosY));
		}
	};
}
