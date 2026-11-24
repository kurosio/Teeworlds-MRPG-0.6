#include "auction_db.h"

namespace AuctionDb
{
	namespace
	{
		constexpr Db::EKind KIND = Db::KindOfTable("tw_auction_slots");
	}

	Db::CResultSet ListSlots()
	{
		return Db::Query(KIND, "SELECT * FROM tw_auction_slots").Result();
	}

	Db::CRequest ReadSlot(int SlotID)
	{
		return Db::Query(KIND, "SELECT * FROM tw_auction_slots WHERE ID = ?", Db::Bind(SlotID));
	}

	Db::CRequest ReadNextSlotID()
	{
		return Db::Query(KIND, "SELECT COALESCE(MAX(ID), 0) + 1 AS NextID FROM tw_auction_slots");
	}

	Db::CRequest CreateSlot(int SlotID, int ItemID, int Value, int Price, int Enchant, int OwnerID, std::string_view Data)
	{
		// Bind the raw bytes; the database converts only an empty value to NULL.
		return Db::Query(KIND, "INSERT INTO tw_auction_slots (ID, ItemID, Value, Price, Enchant, OwnerID, Data) "
			"VALUES (?, ?, ?, ?, ?, ?, NULLIF(?, ''))",
			Db::Bind(SlotID, ItemID, Value, Price, Enchant, OwnerID, Db::Blob(Data)));
	}

	Db::CRequest DeleteSlot(int SlotID)
	{
		return Db::Query(KIND, "DELETE FROM tw_auction_slots WHERE ID = ?", Db::Bind(SlotID));
	}
}
