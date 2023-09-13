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
		std::pair Positions = {
			SecondLocalWorld ? vec2(pResSwap->getInt("TwoPositionX"), pResSwap->getInt("TwoPositionY")) : vec2(pResSwap->getInt("PositionX"), pResSwap->getInt("PositionY")),
			SecondLocalWorld ? vec2(pResSwap->getInt("PositionX"), pResSwap->getInt("PositionY")) : vec2(pResSwap->getInt("TwoPositionX"), pResSwap->getInt("TwoPositionY"))
		};
		std::pair Worlds = {
			SecondLocalWorld ? pResSwap->getInt("TwoWorldID") : pResSwap->getInt("WorldID"),
			SecondLocalWorld ? pResSwap->getInt("WorldID") : pResSwap->getInt("TwoWorldID")
		};

		WorldIdentifier ID = pResSwap->getInt("ID");
		WorldSwappers.push_back({ ID, Positions, Worlds });
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
		int RequiredQuestID = pRes->getInt("RequiredQuestID");
		CWorldData::CreateElement(WorldID)->Init(RespawnWorld, RequiredQuestID, WorldSwappers);

		// update name world from json
		Database->Execute<DB::UPDATE>("enum_worlds", "Name = '%s' WHERE WorldID = '%d'", cstrWorldName.cstr(), WorldID);
		return;
	}

	// create new world data
	CWorldData::CreateElement(WorldID)->Init(WorldID, -1, WorldSwappers);
	Database->Execute<DB::INSERT>("enum_worlds", "(WorldID, Name, RespawnWorld) VALUES ('%d', '%s', '%d')", WorldID, cstrWorldName.cstr(), WorldID);
}

int CWorldManager::GetWorldType() const
{
	if(GS()->GetDungeonID())
		return WORLD_DUNGEON;
	return WORLD_STANDARD;
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
	if(std::vector NodeSteps = m_PathFinderBFS.findPath(GS()->GetWorldID(), WorldID); NodeSteps.size() >= 2)
	{
		const int CurrWorldID = NodeSteps[0];
		const int NextRightWorldID = NodeSteps[1];
		auto& rSwapers = CWorldData::Data()[CurrWorldID]->GetSwappers();

		if(const auto Iter = std::find_if(rSwapers.begin(), rSwapers.end(), [&](const CWorldSwapData& p)
		{ return NextRightWorldID == p.GetSecondWorldID(); }); Iter != rSwapers.end())
			*OutPos = (*Iter).GetFirstSwapPosition();

		dbg_msg("cross-world pathfinder", "Found from %d to %d.", CurrWorldID, NextRightWorldID);
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