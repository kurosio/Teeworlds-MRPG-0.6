#ifndef GAME_SERVER_CORE_TOOLS_LINKS_H
#define GAME_SERVER_CORE_TOOLS_LINKS_H

#include <engine/server/db/db_async.h>

#define TW_LINKS_TABLE "tw_links"

namespace Links
{
	inline constexpr Db::EKind KIND = Db::KindOfTable(TW_LINKS_TABLE);

	inline void Add(const char* pType, int64_t OwnerID, int64_t TargetID)
	{
		Db::Query(KIND, "INSERT INTO tw_links (Type, OwnerID, TargetID) VALUES (?, ?, ?)",
			Db::Bind(pType, OwnerID, TargetID));
	}

	inline void Remove(const char* pType, int64_t OwnerID, int64_t TargetID)
	{
		Db::Query(KIND, "DELETE FROM tw_links WHERE Type = ? AND OwnerID = ? AND TargetID = ?",
			Db::Bind(pType, OwnerID, TargetID));
	}
}

#endif
