#include "aethernet_db.h"

namespace AethernetDb
{
	namespace
	{
		constexpr Db::EKind KIND_AETHERS = Db::KindOfTable("tw_aethers");
		constexpr Db::EKind KIND_ACCOUNTS_AETHERS = Db::KindOfTable("tw_accounts_aethers");
	}

	Db::CResultSet ListAethers()
	{
		return Db::Query(KIND_AETHERS, "SELECT * FROM tw_aethers").Result();
	}

	Db::CRequest ListUnlocked(int AccountID)
	{
		// Read from the remote authority so a recently unlocked aether is visible.
		return Db::Query(KIND_ACCOUNTS_AETHERS, "SELECT AetherID FROM tw_accounts_aethers WHERE UserID = ?",
			Db::Bind(AccountID));
	}

	Db::CRequest Unlock(int AccountID, int AetherID)
	{
		return Db::Query(KIND_ACCOUNTS_AETHERS, "INSERT INTO tw_accounts_aethers (UserID, AetherID) VALUES (?, ?)",
			Db::Bind(AccountID, AetherID));
	}
}
