#ifndef GAME_SERVER_CORE_COMPONENTS_GROUPS_GROUP_DB_H
#define GAME_SERVER_CORE_COMPONENTS_GROUPS_GROUP_DB_H

#include <engine/server/db/db_async.h>

namespace GroupDb
{
	Db::CResultSet ListGroups();
	Db::CRequest ReadNextGroupID();
	Db::CRequest CreateGroup(int GroupID, int OwnerUID, std::string_view AccountIDs);
	Db::CRequest SaveGroup(int GroupID, std::string_view AccountIDs, int OwnerUID);
	Db::CRequest DeleteGroup(int GroupID);
}

#endif // GAME_SERVER_COMPONENT_GROUPS_GROUP_DB_H
