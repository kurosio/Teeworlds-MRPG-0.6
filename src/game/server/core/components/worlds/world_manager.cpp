/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "world_manager.h"

#include <game/server/gamecontext.h>

#include "../achievements/achievement_data.h"

CWorldManager::~CWorldManager()
{
	mystd::freeContainer(CWorldData::Data());
	m_PathFinderBFS.clear();
}

void CWorldManager::OnInitWorld(const std::string& SqlQueryWhereWorld)
{
	std::deque<CWorldSwapData> vSwappers{};
	const auto formatWhere = fmt_default("{} OR `TwoWorldID` = '{}'", SqlQueryWhereWorld, GS()->GetWorldID());

	// initializing world swappers from the database
	ResultPtr pResSwap = Database->Execute<DB::SELECT>("*", "tw_world_swap", formatWhere.c_str());
	while(pResSwap->next())
	{
		const bool IsSecondLocalWorld = pResSwap->getInt("TwoWorldID") == GS()->GetWorldID();

		auto [pos1, pos2] = IsSecondLocalWorld
			? std::make_pair(vec2(pResSwap->getInt("TwoPositionX"), pResSwap->getInt("TwoPositionY")),
				vec2(pResSwap->getInt("PositionX"), pResSwap->getInt("PositionY")))
			: std::make_pair(vec2(pResSwap->getInt("PositionX"), pResSwap->getInt("PositionY")),
				vec2(pResSwap->getInt("TwoPositionX"), pResSwap->getInt("TwoPositionY")));

		auto [world1, world2] = IsSecondLocalWorld
			? std::make_pair(pResSwap->getInt("TwoWorldID"), pResSwap->getInt("WorldID"))
			: std::make_pair(pResSwap->getInt("WorldID"), pResSwap->getInt("TwoWorldID"));

		vSwappers.emplace_back(std::make_pair(pos1, pos2), std::make_pair(world1, world2));
	}

	// initializing world data
	for(int i = 0; i < Server()->GetWorldsSize(); ++i)
	{
		const auto* pDetail = Server()->GetWorldDetail(i);
		dbg_assert(pDetail != nullptr, "detail data inside world initialized invalid");

		CWorldData::CreateElement(i)->Init(pDetail->GetRespawnWorldID(), pDetail->GetJailWorldID(),
			pDetail->GetRequiredLevel(), std::move(vSwappers));
	}
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
		g_EventListenerManager.Notify<IEventListener::PlayerProfessionUnlockedZone>(pPlayer, pPlayer->Account()->GetActiveProfession(), pData->GetID());
	}
}