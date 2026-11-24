#ifndef GAME_SERVER_CORE_COMPONENTS_BOTS_BOT_DB_H
#define GAME_SERVER_CORE_COMPONENTS_BOTS_BOT_DB_H

#include <engine/server/db/db_async.h>

namespace BotDb
{
	Db::CResultSet ListInfo();
	Db::CResultSet ListQuestBots(int WorldID);
	Db::CResultSet ListNpcBots(int WorldID);
	Db::CResultSet ListMobBots(int WorldID);

	Db::CRequest FindByName(std::string_view Name);
	Db::CRequest SaveTeeInfo(int BotID, std::string_view Json);
	Db::CRequest Create(std::string_view Name, std::string_view Json);
}

#endif // GAME_SERVER_CORE_COMPONENTS_BOTS_BOT_DB_H
