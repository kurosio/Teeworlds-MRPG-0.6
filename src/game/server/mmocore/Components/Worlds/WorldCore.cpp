/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "WorldCore.h"

#include <game/server/gamecontext.h>

void CWorldDataCore::OnInitWorld(const char* pWhereLocalWorld)
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

		CWorldSwapPosition::ms_aWorldPositionLogic.push_back({ Worlds.first, Worlds.second, Positions.first });
		CWorldSwapPosition::ms_aWorldPositionLogic.push_back({ Worlds.second, Worlds.first, Positions.second });

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
		CWorldData::CreateDataItem(WorldID)->Init(RespawnWorld, RequiredQuestID, WorldSwappers);

		// update name world from json
		Database->Execute<DB::UPDATE>("enum_worlds", "Name = '%s' WHERE WorldID = '%d'", cstrWorldName.cstr(), WorldID);
		return;
	}

	// create new world data
	CWorldData::CreateDataItem(WorldID)->Init(WorldID, -1, WorldSwappers);
	Database->Execute<DB::INSERT>("enum_worlds", "(WorldID, Name, RespawnWorld) VALUES ('%d', '%s', '%d')", WorldID, cstrWorldName.cstr(), WorldID);
}

int CWorldDataCore::GetWorldType() const
{
	if(GS()->GetDungeonID())
		return WORLD_DUNGEON;
	return WORLD_STANDARD;
}

void CWorldDataCore::FindPosition(int WorldID, vec2 Pos, vec2* OutPos)
{
	int StartWorldID = GS()->GetWorldID(); // start path
	int EndWorldID = WorldID; // end path

	// it's not search by world swappers : only set pos
	if(StartWorldID == WorldID)
	{
		*OutPos = Pos;
		return;
	}

	// nodes for check
	struct CNodeWorlds
	{
		int m_Base{};
		int m_Find{};
	};
	std::vector<CNodeWorlds> StablePath;
	std::vector LastWorlds { StartWorldID };
	std::map <int, bool> Checked { {StartWorldID, true} };

	// search active nodes
	while(StartWorldID != EndWorldID)
	{
		bool HasAction = false;

		for(auto& [BaseID, FindID, Pos] : CWorldSwapPosition::ms_aWorldPositionLogic)
		{
			if(BaseID == StartWorldID && Checked.find(FindID) == Checked.end())
			{
				LastWorlds.push_back(BaseID);
				StartWorldID = FindID;
				Checked[StartWorldID] = true;

				HasAction = true;
				StablePath.push_back({ BaseID, FindID });
				dbg_msg("test", "CHECK: %d -> %d", BaseID, FindID);

				// end foreach : found
				if(StartWorldID == EndWorldID)
					break;
			}
		}

		if(!HasAction)
		{
			// get failed node
			auto Iter = std::find_if(StablePath.begin(), StablePath.end(), [LastWorlds, StartWorldID](CNodeWorlds& p)
			{
				return p.m_Base == LastWorlds.back() && p.m_Find == StartWorldID;
			});
			if(Iter == StablePath.end())
			{
				dbg_msg("test", "there is no path");
				break;
			}

			// erase failed node
			dbg_msg("test", "fail check tree erase");
			StablePath.erase(Iter);
			StartWorldID = LastWorlds.back();
			LastWorlds.pop_back();
		}
	}

	// get current node pos
	auto Iter = std::find_if(CWorldSwapPosition::ms_aWorldPositionLogic.begin(), CWorldSwapPosition::ms_aWorldPositionLogic.end(), [&](const CWorldSwapPosition& pItem)
	{
		const CNodeWorlds NodeNearby = (int)StablePath.empty() ? CNodeWorlds() : StablePath.front();
		return NodeNearby.m_Base == pItem.m_BaseWorldID && NodeNearby.m_Find == pItem.m_FindWorldID;
	});
	if(Iter != CWorldSwapPosition::ms_aWorldPositionLogic.end())
		*OutPos = (*Iter).m_Position;
}

void CWorldDataCore::CheckQuestingOpened(CPlayer* pPlayer, int QuestID) const
{
	const int ClientID = pPlayer->GetCID();
	for(const auto& pData : CWorldData::Data())
	{
		if(pData->GetRequiredQuest() && pData->GetRequiredQuest()->m_QuestID == QuestID)
			GS()->Chat(-1, "{STR} opened zone ({STR})!", Server()->ClientName(ClientID), Server()->GetWorldName(pData->GetID()));
	}
}