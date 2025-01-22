/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "AccountFarmingManager.h"

#include <game/server/entity_manager.h>
#include <game/server/gamecontext.h>

#include <game/server/core/components/Inventory/InventoryManager.h>

std::map < int , CAccountFarmingManager::FarmingPoint > CAccountFarmingManager::ms_vmFarmingPoints;

void CAccountFarmingManager::OnInitWorld(const std::string& SqlQueryWhereWorld)
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_positions_farming", SqlQueryWhereWorld.c_str());
	while(pRes->next())
	{
		// initialize variables
		FarmingPoint Point;
		Point.m_ItemID = pRes->getInt("ItemID");
		Point.m_Position = vec2(pRes->getInt("PositionX"), pRes->getInt("PositionY"));
		Point.m_Distance = pRes->getInt("Distance");
		Point.m_WorldID = pRes->getInt("WorldID");

		// insert to game base
		const int ID = pRes->getInt("ID");
		ms_vmFarmingPoints[ID] = Point;
	}
}

CItemDescription* CAccountFarmingManager::GetFarmingItemInfoByPos(vec2 Pos) const
{
	// search farming item by position
	auto Iter = std::ranges::find_if(ms_vmFarmingPoints, [Pos](auto& p)
	{
		return distance(p.second.m_Position, Pos) < p.second.m_Distance;
	});
	return (Iter != ms_vmFarmingPoints.end()) ? GS()->GetItemInfo(Iter->second.m_ItemID) : nullptr;;
}