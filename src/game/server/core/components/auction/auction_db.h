#ifndef GAME_SERVER_CORE_COMPONENTS_AUCTION_AUCTION_DB_H
#define GAME_SERVER_CORE_COMPONENTS_AUCTION_AUCTION_DB_H

#include <engine/server/db/db_async.h>

namespace AuctionDb
{
	Db::CResultSet ListSlots();
	Db::CRequest ReadSlot(int SlotID);
	Db::CRequest ReadNextSlotID();
	Db::CRequest CreateSlot(int SlotID, int ItemID, int Value, int Price, int Enchant, int OwnerID, std::string_view Data);
	Db::CRequest DeleteSlot(int SlotID);
}

#endif // GAME_SERVER_COMPONENT_AUCTION_AUCTION_DB_H
