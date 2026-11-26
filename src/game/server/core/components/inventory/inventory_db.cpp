#include "inventory_db.h"

namespace InventoryDb
{
	namespace
	{
		constexpr Db::EKind KIND_ITEMS_LIST = Db::KindOfTable("tw_items_list");
		constexpr Db::EKind KIND_ATTRIBUTES = Db::KindOfTable("tw_attributes");
		constexpr Db::EKind KIND_ACCOUNTS_ITEMS = Db::KindOfTable("tw_accounts_items");
	}

	Db::CResultSet ListItemDescriptions()
	{
		return Db::Query(KIND_ITEMS_LIST, "SELECT * FROM tw_items_list").Result();
	}

	Db::CResultSet ListAttributes()
	{
		return Db::Query(KIND_ATTRIBUTES, "SELECT * FROM tw_attributes").Result();
	}

	Db::CRequest LoadAccountItems(int AccountID)
	{
		// Read from the remote authority: inventory is part of login and has to
		// show what the last session wrote
		return Db::Query(KIND_ACCOUNTS_ITEMS, "SELECT * FROM tw_accounts_items WHERE UserID = ?", Db::Bind(AccountID));
	}

	Db::CRequest RepairDurability(int AccountID)
	{
		return Db::Query(KIND_ACCOUNTS_ITEMS, "UPDATE tw_accounts_items SET Durability = 100 WHERE UserID = ?",
			Db::Bind(AccountID));
	}

	Db::CRequest DeleteAccountItem(int ItemID, int AccountID)
	{
		return Db::Query(KIND_ACCOUNTS_ITEMS, "DELETE FROM tw_accounts_items WHERE ItemID = ? AND UserID = ?",
			Db::Bind(ItemID, AccountID));
	}

	Db::CRequest SaveAccountItem(int ItemID, int AccountID, int Value, int Settings, int Enchant, int Durability, time_t ExpiresAt, std::string_view Data)
	{
		// one statement per item: the row is created if the item is new and
		// overwritten if it is not
		return Db::Query(KIND_ACCOUNTS_ITEMS,
			"INSERT INTO tw_accounts_items (ItemID, UserID, Value, Settings, Enchant, Durability, ExpiresAt, Data) "
			"VALUES (?, ?, ?, ?, ?, ?, ?, NULLIF(?, '')) "
			"ON DUPLICATE KEY UPDATE Value = ?, Settings = ?, Enchant = ?, Durability = ?, ExpiresAt = ?, Data = NULLIF(?, '')",
			Db::Bind(ItemID, AccountID, Value, Settings, Enchant, Durability, (int64_t)ExpiresAt, Db::Blob(Data),
				Value, Settings, Enchant, Durability, (int64_t)ExpiresAt, Db::Blob(Data)), Db::SQueryOptions().FetchInsertId(false));
	}
}
