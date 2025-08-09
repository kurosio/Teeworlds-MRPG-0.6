/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "world_manager.h"

#include <game/server/gamecontext.h>

#include <components/achievements/achievement_data.h>

CWorldManager::~CWorldManager()
{
	mystd::freeContainer(CWorldData::Data());
	m_PathFinderBFS.clear();
}

void CWorldManager::OnInitWorld(const std::string& SqlQueryWhereWorld)
{
	std::deque<CWorldSwapData> vSwappers{};
	const auto currentWorldID = GS()->GetWorldID();
//	const auto formatWhere = fmt_default("{} OR `TwoWorldID` = '{}'", SqlQueryWhereWorld, currentWorldID);

	// initializing world swappers from the database
	ResultPtr pResSwap = Database->Execute<DB::SELECT>("*", "tw_world_swap", SqlQueryWhereWorld.c_str());
	while(pResSwap->next())
	{
        std::pair<vec2, vec2> SwapperPos;
        SwapperPos.first = vec2(pResSwap->getInt("PositionX"), pResSwap->getInt("PositionY"));
        SwapperPos.second = vec2(pResSwap->getInt("TwoPositionX"), pResSwap->getInt("TwoPositionY"));

        std::pair<int, int> Worlds;
        Worlds.first = pResSwap->getInt("WorldID") ;
        Worlds.second = pResSwap->getInt("TwoWorldID");

		vSwappers.emplace_back(SwapperPos, Worlds);
//        if(Worlds.second == currentWorldID) // two-sided
//        {
//            std::swap(SwapperPos.first, SwapperPos.second);
//            std::swap(Worlds.first, Worlds.second);
//            vSwappers.emplace_back(SwapperPos, Worlds);
//            dbg_msg("SWAPPERS", "[%d](%0.2f;0.2f) -> [%d](%0.2f;0.2f)",
//                    Worlds.first, SwapperPos.first.x, SwapperPos.first.y,
//                    Worlds.second, SwapperPos.second.x, SwapperPos.second.y
//            );
//        }
	}

	// initializing world data
	const auto* pDetail = Server()->GetWorldDetail(currentWorldID);
	dbg_assert(pDetail != nullptr, "detail data inside world initialized invalid");
	CWorldData::CreateElement(currentWorldID)->Init(pDetail->GetRespawnWorldID(), pDetail->GetJailWorldID(),
		pDetail->GetRequiredLevel(), std::move(vSwappers));
}

void CWorldManager::OnPostInit()
{
	// initialize bfs edges
	m_PathFinderBFS.init((int)CWorldData::Data().size());
	for(const auto& pw : CWorldData::Data())
	{
		for(auto& p : pw->GetSwappers())
		{
			m_PathFinderBFS.addEdge(p.GetFirstWorldID(), p.GetSecondWorldID());
		}
	}
}

std::optional<vec2> CWorldManager::FindPosition(int WorldID, vec2 Pos) const
{
	// default path
	int CurrentWorldID = GS()->GetWorldID();
	if(CurrentWorldID == WorldID)
		return Pos;

	// search path between worlds
	const auto vNodeSteps = m_PathFinderBFS.findPath(CurrentWorldID, WorldID);
	if(vNodeSteps.size() >= 2)
	{
		const int NextRightWorldID = vNodeSteps[1];
		auto& rSwappers = CWorldData::Data()[CurrentWorldID]->GetSwappers();

		// search path
		if(const auto Iter = std::ranges::find_if(rSwappers, [&](const CWorldSwapData& p)
		{
			return NextRightWorldID == p.GetSecondWorldID();
		}); Iter != rSwappers.end())
		{
			dbg_msg("cross-world pathfinder", "Found from %d to %d.", CurrentWorldID, NextRightWorldID);
			return Iter->GetFirstSwapPosition();
		}
	}

	return std::nullopt;
}

void CWorldManager::NotifyUnlockedZonesByLeveling(CPlayer* pPlayer) const
{
	const int ClientID = pPlayer->GetCID();
	const int PlayerLevel = pPlayer->Account()->GetLevel();

	for(const auto& pData : CWorldData::Data())
	{
		const int RequiredLevel = pData->GetRequiredLevel();
		if(PlayerLevel != RequiredLevel)
			continue;

		GS()->Chat(-1, "'{}' initiated area ('{}')!", Server()->ClientName(ClientID), Server()->GetWorldName(pData->GetID()));
	}
}