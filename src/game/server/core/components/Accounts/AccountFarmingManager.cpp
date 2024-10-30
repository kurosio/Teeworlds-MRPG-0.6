/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "AccountFarmingManager.h"

#include <game/server/entity_manager.h>
#include <game/server/gamecontext.h>

#include <game/server/core/components/Inventory/InventoryManager.h>

std::map < int , CAccountFarmingManager::FarmingPoint > CAccountFarmingManager::ms_vmFarmingPoints;

void CAccountFarmingManager::OnInitWorld(const char* pWhereLocalWorld)
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_positions_farming", pWhereLocalWorld);
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

bool CAccountFarmingManager::InsertItemsDetailVotes(CPlayer* pPlayer, int WorldID)
{
	bool Found = false;
	const int ClientID = pPlayer->GetCID();

	for(const auto& [ID, Point] : ms_vmFarmingPoints)
	{
		if(WorldID != Point.m_WorldID)
			continue;

		const vec2 Pos = Point.m_Position / 32.0f;
		VoteWrapper VInfo(ClientID, VWF_UNIQUE | VWF_STYLE_SIMPLE, "Farm point {}", GS()->GetItemInfo(Point.m_ItemID)->GetName());
		VInfo.MarkList().Add("Location:");
		{
			VInfo.BeginDepth();
			VInfo.Add(Instance::Localize(ClientID, Instance::Server()->GetWorldName(WorldID)));
			VInfo.Add("x{} y{}", (int)Pos.x, (int)Pos.y);
			VInfo.EndDepth();
		}
		VInfo.AddLine();
		VInfo.MarkList().Add("Description");
		{
			VInfo.BeginDepth();
			VInfo.Add("Distance of distribution: {}P", Point.m_Distance);
			VInfo.EndDepth();
		}
		VInfo.AddLine();
		Found = true;
	}

	return Found;
}