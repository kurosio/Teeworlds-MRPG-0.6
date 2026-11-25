#ifndef GAME_SERVER_CORE_TOOLS_LINKS_H
#define GAME_SERVER_CORE_TOOLS_LINKS_H

#include <engine/server/db/db_async.h>

#define TW_LINKS_TABLE "tw_links"

namespace Links
{
	inline constexpr Db::EKind KIND = Db::KindOfTable(TW_LINKS_TABLE);

	inline Db::CRequest Add(const char* pType, int64_t OwnerID, int64_t TargetID, std::string_view InitialMeta = "{}")
	{
		return Db::Query(KIND,
			"INSERT INTO tw_links (Type, OwnerID, TargetID, MetaJson) VALUES (?, ?, ?, ?) "
			"ON DUPLICATE KEY UPDATE OwnerID = IF(VALUES(OwnerID) > 0, VALUES(OwnerID), OwnerID)",
			Db::Bind(pType, OwnerID, TargetID, InitialMeta));
	}

	inline Db::CRequest Remove(const char* pType, int64_t OwnerID, int64_t TargetID)
	{
		return Db::Query(KIND,
			"UPDATE tw_links SET OwnerID = 0 WHERE Type = ? AND OwnerID = ? AND TargetID = ?",
			Db::Bind(pType, OwnerID, TargetID));
	}

	// save a single key-value pair in the MetaJson column, creating the row if it does not exist yet
	inline Db::CRequest SaveStringMeta(const char* pType, int64_t TargetID, const char* pKey, std::string_view Value)
	{
		return Db::Query(KIND,
			"INSERT INTO tw_links (Type, OwnerID, TargetID, MetaJson) VALUES (?, 0, ?, JSON_OBJECT(?, ?)) "
			"ON DUPLICATE KEY UPDATE MetaJson = JSON_SET(COALESCE(MetaJson, JSON_OBJECT()), ?, ?)",
			Db::Bind(pType, TargetID, pKey, Value, std::string("$.") + pKey, Value));
	}

	inline Db::CRequest SaveNumberMeta(const char* pType, int64_t TargetID, const char* pKey, int64_t Value)
	{
		return Db::Query(KIND,
			"INSERT INTO tw_links (Type, OwnerID, TargetID, MetaJson) VALUES (?, 0, ?, JSON_OBJECT(?, ?)) "
			"ON DUPLICATE KEY UPDATE MetaJson = JSON_SET(COALESCE(MetaJson, JSON_OBJECT()), ?, ?)",
			Db::Bind(pType, TargetID, pKey, Value, std::string("$.") + pKey, Value));
	}

	inline Db::CRequest SaveJsonMeta(const char* pType, int64_t TargetID, const char* pKey, std::string_view Json)
	{
		return Db::Query(KIND,
			"INSERT INTO tw_links (Type, OwnerID, TargetID, MetaJson) VALUES (?, 0, ?, JSON_OBJECT(?, JSON_EXTRACT(?, '$'))) "
			"ON DUPLICATE KEY UPDATE MetaJson = JSON_SET(COALESCE(MetaJson, JSON_OBJECT()), ?, JSON_EXTRACT(?, '$'))",
			Db::Bind(pType, TargetID, pKey, Json, std::string("$.") + pKey, Json));
	}

	inline Db::CRequest SavePlayerHouseState(int64_t TargetID, int64_t RentEndTimestamp)
	{
		return Db::Query(KIND,
			"INSERT INTO tw_links (Type, OwnerID, TargetID, MetaJson) VALUES ('player_house', 0, ?, JSON_OBJECT('bank', '0', 'rent_end', ?)) "
			"ON DUPLICATE KEY UPDATE MetaJson = JSON_SET(COALESCE(MetaJson, JSON_OBJECT()), '$.bank', '0', '$.rent_end', ?)",
			Db::Bind(TargetID, RentEndTimestamp, RentEndTimestamp));
	}
}

#endif
