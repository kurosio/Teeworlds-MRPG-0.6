#include "group_db.h"

namespace GroupDb
{
	namespace
	{
		constexpr Db::EKind KIND = Db::KindOfTable("tw_groups");
	}

	Db::CResultSet ListGroups()
	{
		return Db::Query(KIND, "SELECT * FROM tw_groups").Result();
	}

	Db::CRequest ReadNextGroupID()
	{
		return Db::Query(KIND, "SELECT COALESCE(MAX(ID), 0) + 1 AS NextID FROM tw_groups");
	}

	Db::CRequest CreateGroup(int GroupID, int OwnerUID, std::string_view AccountIDs)
	{
		return Db::Query(KIND, "INSERT INTO tw_groups (ID, OwnerUID, AccountIDs) VALUES (?, ?, ?)",
			Db::Bind(GroupID, OwnerUID, AccountIDs));
	}

	Db::CRequest SaveGroup(int GroupID, std::string_view AccountIDs, int OwnerUID)
	{
		return Db::Query(KIND, "UPDATE tw_groups SET AccountIDs = ?, OwnerUID = ? WHERE ID = ?",
			Db::Bind(AccountIDs, OwnerUID, GroupID));
	}

	Db::CRequest DeleteGroup(int GroupID)
	{
		return Db::Query(KIND, "DELETE FROM tw_groups WHERE ID = ?", Db::Bind(GroupID));
	}
}
