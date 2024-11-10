/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "AccountMiningManager.h"

#include <engine/server.h>
#include <game/server/entity_manager.h>
#include <game/server/gamecontext.h>

std::map < int , CAccountMiningManager::MiningPoint > CAccountMiningManager::ms_vmMiningPoints;

void CAccountMiningManager::OnInitWorld(const std::string& SqlQueryWhereWorld)
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_positions_mining", SqlQueryWhereWorld.c_str());
	while (pRes->next())
	{
		// initialize variables
		MiningPoint Point;
		Point.m_ItemID = pRes->getInt("ItemID");
		Point.m_Position = vec2(pRes->getInt("PositionX"), pRes->getInt("PositionY"));
		Point.m_Distance = pRes->getInt("Distance");
		Point.m_WorldID = pRes->getInt("WorldID");

		// insert to game base
		const int ID = pRes->getInt("ID");
		ms_vmMiningPoints[ID] = std::move(Point);
	}
}

CItemDescription* CAccountMiningManager::GetMiningItemInfoByPos(vec2 Pos) const
{
	// search ore by position
	auto Iter = std::find_if(ms_vmMiningPoints.begin(), ms_vmMiningPoints.end(), [Pos](auto& p)
	{
		return distance(p.second.m_Position, Pos) < p.second.m_Distance;
	});
	return (Iter != ms_vmMiningPoints.end()) ? GS()->GetItemInfo(Iter->second.m_ItemID) : nullptr;;
}

bool CAccountMiningManager::InsertItemsDetailVotes(CPlayer* pPlayer, int WorldID) const
{
	bool Found = false;
	const int ClientID = pPlayer->GetCID();

	for(const auto& [ID, Ore] : ms_vmMiningPoints)
	{
		if(WorldID != Ore.m_WorldID)
			continue;

		const vec2 Pos = Ore.m_Position / 32.0f;
		VoteWrapper VOres(ClientID, VWF_UNIQUE | VWF_STYLE_SIMPLE, "Ore {}", GS()->GetItemInfo(Ore.m_ItemID)->GetName());
		VOres.MarkList().Add("Location:");
		{
			VOres.BeginDepth();
			VOres.Add(Instance::Localize(ClientID, Instance::Server()->GetWorldName(WorldID)));
			VOres.Add("x{} y{}", (int)Pos.x, (int)Pos.y);
			VOres.EndDepth();
		}
		VOres.AddLine();
		VOres.MarkList().Add("Description");
		{
			VOres.BeginDepth();
			//VOres.Add("Level: {}", Ore.Level);
			//VOres.Add("Health: {}P", Ore.m_StartHealth);
			VOres.Add("Distance of distribution: {}P", Ore.m_Distance);
			VOres.EndDepth();
		}
		VOres.AddLine();
		Found = true;
	}

	return Found;
}