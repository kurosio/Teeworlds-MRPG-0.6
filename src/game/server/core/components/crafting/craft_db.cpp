#include "craft_db.h"

namespace CraftDb
{
	Db::CResultSet ListCrafts(int WorldID)
	{
		return Db::Query(Db::KindOfTable("tw_crafts_list"), "SELECT * FROM tw_crafts_list WHERE WorldID = ?", Db::Bind(WorldID)).Result();
	}
}
