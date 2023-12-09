/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "WorldManager.h"

#include <game/server/gamecontext.h>

void CWorldManager::OnInitWorld(const char* pWhereLocalWorld)
{
	std::deque <CWorldSwapData> WorldSwappers;

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
			Positions = { vec2(pResSwap->getInt("TwoPositionX"), pResSwap->getInt("TwoPositionY")), vec2(pResSwap->getInt("PositionX"), pResSwap->getInt("PositionY")) };
			Worlds = { pResSwap->getInt("TwoWorldID"), pResSwap->getInt("WorldID") };
		}
		else
		{
			Positions = { vec2(pResSwap->getInt("PositionX"), pResSwap->getInt("PositionY")), vec2(pResSwap->getInt("TwoPositionX"), pResSwap->getInt("TwoPositionY")) };
			Worlds = { pResSwap->getInt("WorldID"), pResSwap->getInt("TwoWorldID") };
		}

		WorldSwappers.push_back({ Positions, Worlds });
	}

	/*
	 * init world data
	 */
	const int WorldID = GS()->GetWorldID();
	const CSqlString<32> cstrWorldName = CSqlString<32>(Server()->GetWorldName(WorldID));
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "enum_worlds", pWhereLocalWorld);
	if(pRes->next())
	{
		int RespawnWorld = pRes->getInt("RespawnWorld");
		int JailWorld = pRes->getInt("JailWorld");
		int RequiredQuestID = pRes->getInt("RequiredQuestID");

		// update name world from json
		CWorldData::CreateElement(WorldID)->Init(RespawnWorld, JailWorld, RequiredQuestID, std::move(WorldSwappers));
		Database->Execute<DB::UPDATE>("enum_worlds", "Name = '%s' WHERE WorldID = '%d'", cstrWorldName.cstr(), WorldID);
	}
	else
	{
		// create new world data
		CWorldData::CreateElement(WorldID)->Init(WorldID, WorldID, -1, std::move(WorldSwappers));
		Database->Execute<DB::INSERT>("enum_worlds", "(WorldID, Name, RespawnWorld, JailWorld) VALUES ('%d', '%s', '%d')", WorldID, cstrWorldName.cstr(), WorldID, WorldID);
	}
}

int CWorldManager::GetWorldType() const
{
	return GS()->GetDungeonID() ? WORLD_DUNGEON : WORLD_STANDARD;
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
	std::vector NodeSteps = m_PathFinderBFS.findPath(GS()->GetWorldID(), WorldID);
	if(NodeSteps.size() >= 2)
	{
		const int NextRightWorldID = NodeSteps[1];
		auto& rSwapers = CWorldData::Data()[CurrentWorldID]->GetSwappers();
		const auto Iter = std::find_if(rSwapers.begin(), rSwapers.end(), [&](const CWorldSwapData& p) { return NextRightWorldID == p.GetSecondWorldID(); });
		if(Iter != rSwapers.end())
			*OutPos = (*Iter).GetFirstSwapPosition();

		dbg_msg("cross-world pathfinder", "Found from %d to %d.", CurrentWorldID, NextRightWorldID);
	}
}

void CWorldManager::NotifyUnlockedZonesByQuest(CPlayer* pPlayer, int QuestID) const
{
	const int ClientID = pPlayer->GetCID();
	for(const auto& pData : CWorldData::Data())
	{
		if(pData->GetRequiredQuest() && pData->GetRequiredQuest()->GetID() == QuestID)
			GS()->Chat(-1, "{STR} opened zone ({STR})!", Server()->ClientName(ClientID), Server()->GetWorldName(pData->GetID()));
	}
}