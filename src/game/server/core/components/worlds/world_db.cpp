#include "world_db.h"

namespace WorldDb
{
	namespace
	{
		constexpr Db::EKind KIND_NODES = Db::KindOfTable("tw_nodes_collecting");
		constexpr Db::EKind KIND_SWAP = Db::KindOfTable("tw_world_swap");
	}

	Db::CResultSet ListCollectingNodes(int WorldID)
	{
		return Db::Query(KIND_NODES, "SELECT * FROM tw_nodes_collecting WHERE WorldID = ?", Db::Bind(WorldID)).Result();
	}

	Db::CResultSet ListSwappers(int WorldID)
	{
		return Db::Query(KIND_SWAP, "SELECT * FROM tw_world_swap WHERE WorldID = ?", Db::Bind(WorldID)).Result();
	}
}
