#include "bot_db.h"

namespace BotDb
{
	namespace
	{
		constexpr Db::EKind KIND_INFO = Db::KindOfTable("tw_bots_info");
		constexpr Db::EKind KIND_QUEST = Db::KindOfTable("tw_bots_quest");
		constexpr Db::EKind KIND_NPC = Db::KindOfTable("tw_bots_npc");
		constexpr Db::EKind KIND_MOBS = Db::KindOfTable("tw_bots_mobs");
	}

	Db::CResultSet ListInfo()
	{
		return Db::Query(KIND_INFO, "SELECT * FROM tw_bots_info").Result();
	}

	Db::CResultSet ListQuestBots(int WorldID)
	{
		return Db::Query(KIND_QUEST, "SELECT * FROM tw_bots_quest WHERE WorldID = ?", Db::Bind(WorldID)).Result();
	}

	Db::CResultSet ListNpcBots(int WorldID)
	{
		return Db::Query(KIND_NPC, "SELECT * FROM tw_bots_npc WHERE WorldID = ?", Db::Bind(WorldID)).Result();
	}

	Db::CResultSet ListMobBots(int WorldID)
	{
		return Db::Query(KIND_MOBS, "SELECT * FROM tw_bots_mobs WHERE WorldID = ?", Db::Bind(WorldID)).Result();
	}

	Db::CRequest FindByName(std::string_view Name)
	{
		return Db::Query(KIND_INFO, "SELECT ID FROM tw_bots_info WHERE Name = ?", Db::Bind(Name));
	}

	Db::CRequest SaveTeeInfo(int BotID, std::string_view Json)
	{
		return Db::Query(KIND_INFO, "UPDATE tw_bots_info SET JsonTeeInfo = ? WHERE ID = ?", Db::Bind(Json, BotID));
	}

	Db::CRequest Create(std::string_view Name, std::string_view Json)
	{
		return Db::Query(KIND_INFO, "INSERT INTO tw_bots_info (Name, JsonTeeInfo) VALUES (?, ?)", Db::Bind(Name, Json));
	}
}
