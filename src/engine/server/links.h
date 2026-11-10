#ifndef GAME_SERVER_CORE_TOOLS_LINKS_H
#define GAME_SERVER_CORE_TOOLS_LINKS_H

#include "sql_connect_pool.h"

#define TW_LINKS_TABLE "tw_links"

/*
 * Universal links table: ID, Type, OwnerID, TargetID
 */
namespace Links
{
	inline void Add(const char* pType, int64_t OwnerID, int64_t TargetID)
	{
		Database->Execute<DB::INSERT>(TW_LINKS_TABLE, "(Type, OwnerID, TargetID) VALUES ('{}', '{}', '{}')", pType, OwnerID, TargetID);
	}

	inline void Remove(const char* pType, int64_t OwnerID, int64_t TargetID)
	{
		Database->Execute<DB::REMOVE>(TW_LINKS_TABLE, "WHERE Type = '{}' AND OwnerID = '{}' AND TargetID = '{}'", pType, OwnerID, TargetID);
	}
}

#endif
