/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "world_manager.h"

#include <game/server/gamecontext.h>

#include "../achievements/achievement_data.h"

void CWorldManager::OnInitWorld(const char* pWhereLocalWorld)
{
	std::deque<CWorldSwapData> vSwappers{};

	/*
	 *	load world swappers
	 */
	char aFormatWhere[1024];
	str_format(aFormatWhere, sizeof(aFormatWhere), "%s OR `TwoWorldID`='%d'", pWhereLocalWorld, GS()->GetWorldID());
	ResultPtr pResSwap = Database->Execute<DB::SELECT>("*", "tw_world_swap", aFormatWhere);
	while(pResSwap->next())
	{
		bool SecondLocalWorld = pResSwap->getInt("TwoWorldID") == GS()->GetWorldID();
		std::pair<vec2, vec2> Positions;
		std::pair<int, int> Worlds;

		if(SecondLocalWorld)
		{
			Positions = { vec2(pResSwap->getInt("TwoPositionX"), pResSwap->getInt("TwoPositionY")),
						  vec2(pResSwap->getInt("PositionX"), pResSwap->getInt("PositionY")) };
			Worlds = { pResSwap->getInt("TwoWorldID"), pResSwap->getInt("WorldID") };
		}
		else
		{
			Positions = { vec2(pResSwap->getInt("PositionX"), pResSwap->getInt("PositionY")),
						  vec2(pResSwap->getInt("TwoPositionX"), pResSwap->getInt("TwoPositionY")) };
			Worlds = { pResSwap->getInt("WorldID"), pResSwap->getInt("TwoWorldID") };
		}

		vSwappers.emplace_back(std::move(Positions), std::move(Worlds));
	}

	/*
	 * init world data
	 */
	for(int i = 0; i < Server()->GetWorldsSize(); i++)
	{
		CWorldDetail* pDetail = Server()->GetWorldDetail(i);
		dbg_assert(pDetail != nullptr, "detail data inside world initilized invalid");
		CWorldData::CreateElement(i)->Init(pDetail->GetRespawnWorldID(), pDetail->GetJailWorldID(), pDetail->GetRequiredLevel(), std::move(vSwappers));
	}
}

void CWorldManager::FindPosition(int WorldID, vec2 Pos, vec2* OutPos)
{
	// there's no need to search for paths between worlds
	int CurrentWorldID = GS()->GetWorldID();
	if(CurrentWorldID == WorldID)
	{
		*OutPos = Pos;
		return;
	}

	// initialize if not initialized 
	if(!m_PathFinderBFS.isInitilized())
	{
		m_PathFinderBFS.init((int)CWorldData::Data().size());
		for(const auto& pw : CWorldData::Data())
		{
			for(auto& p : pw->GetSwappers())
				m_PathFinderBFS.addEdge(p.GetFirstWorldID(), p.GetSecondWorldID());
		}
	}

	// search path and got first and second path
	std::vector vNodeSteps = m_PathFinderBFS.findPath(GS()->GetWorldID(), WorldID);
	if(vNodeSteps.size() >= 2)
	{
		const int NextRightWorldID = vNodeSteps[1];
		auto& rSwapers = CWorldData::Data()[CurrentWorldID]->GetSwappers();
		const auto Iter = std::find_if(rSwapers.begin(), rSwapers.end(), [&](const CWorldSwapData& p) { return NextRightWorldID == p.GetSecondWorldID(); });
		if(Iter != rSwapers.end())
		{
			*OutPos = (*Iter).GetFirstSwapPosition();
		}

		dbg_msg("cross-world pathfinder", "Found from %d to %d.", CurrentWorldID, NextRightWorldID);
	}
}

void CWorldManager::NotifyUnlockedZonesByLeveling(CPlayer* pPlayer, int Level) const
{
	const int ClientID = pPlayer->GetCID();
	for(const auto& pData : CWorldData::Data())
	{
		if(pPlayer->Account()->GetLevel() == Level)
		{
			GS()->Chat(-1, "{} initiated area ({})!", Server()->ClientName(ClientID), Server()->GetWorldName(pData->GetID()));
			pPlayer->UpdateAchievement(AchievementType::UnlockWorld, GS()->GetWorldID(), 1, PROGRESS_ABSOLUTE);
		}
	}
}