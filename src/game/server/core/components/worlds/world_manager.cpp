/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "world_manager.h"
#include "world_db.h"

#include <game/server/gamecontext.h>

#include <components/achievements/achievement_data.h>
#include <components/inventory/item_info_data.h>

CWorldManager::~CWorldManager()
{
	mystd::freeContainer(CWorldData::Data());
	m_PathFinderBFS.clear();
}

void CWorldManager::InitCollectingNodes(int WorldID)
{
	// resource definitions belong to this world
	auto Nodes = WorldDb::ListCollectingNodes(WorldID);
	while(Nodes.Next())
	{
		const int ID = Nodes.GetInt("ID");
		const int SwitchID = Nodes.GetInt("SwitchID");
		const std::string Type = Nodes.GetString("Type");
		GatheringNode Node {};
		Node.Name = Nodes.GetString("Name");
		Node.Level = Nodes.GetInt("Level");
		Node.Health = Nodes.GetInt("Health");
		if(SwitchID < 1 || SwitchID > 255 || Node.Level < 1 || Node.Health < 1 || Node.Name.empty())
		{
			dbg_msg("collecting", "Invalid node #%d in world %d", ID, GS()->GetWorldID());
			continue;
		}

		// sets an exact base amount. ChanceProcessor keeps the weighted selection.
		const std::string Properties = Nodes.GetString("Properties");
		const DBSet Items(Properties);
		bool Valid = true;
		for(const auto& Entry : Items.getItems())
		{
			int ItemID = 0, Amount = 0, End = 0;
			double Chance = 0;
			const bool HasAmount = sscanf(Entry.c_str(), "[%d/%d/%lf]%n", &ItemID, &Amount, &Chance, &End) == 3 && End == (int)Entry.size();
			if(!HasAmount)
			{
				End = 0;
				if(sscanf(Entry.c_str(), "[%d/%lf]%n", &ItemID, &Chance, &End) != 2 || End != (int)Entry.size())
				{
					Valid = false;
					break;
				}
			}

			if(!CItemDescription::Data().contains(ItemID) || !std::isfinite(Chance) || Chance <= 0 || Chance > 100 ||
				(HasAmount && (Amount < 1 || Amount > 1000000)) || Node.m_vItems.hasElement(ItemID))
			{
				Valid = false;
				break;
			}

			Node.m_vItems.addElement(ItemID, Chance);
			if(HasAmount)
				Node.m_vItemAmounts[ItemID] = Amount;
		}
		if(!Valid || Node.m_vItems.isEmpty())
		{
			dbg_msg("collecting", "invalid Properties for node #%d", ID);
			continue;
		}

		// add collecting node
		Node.m_vItems.normalizeChances();
		if(!GS()->Collision()->AddCollectingNode(Type, SwitchID, std::move(Node)))
			dbg_msg("collecting", "unknown type or duplicate node #%d (%s, switch %d)", ID, Type.c_str(), SwitchID);
	}
}

void CWorldManager::OnInitWorld(int WorldID)
{
	InitCollectingNodes(WorldID);

	std::deque<CWorldSwapData> vSwappers{};
	const auto currentWorldID = GS()->GetWorldID();

	// initializing world swappers from the database
	auto Swappers = WorldDb::ListSwappers(WorldID);
	while(Swappers.Next())
	{
		std::pair<vec2, vec2> SwapperPos;
		SwapperPos.first = vec2(Swappers.GetInt("PositionX"), Swappers.GetInt("PositionY"));
		SwapperPos.second = vec2(Swappers.GetInt("TwoPositionX"), Swappers.GetInt("TwoPositionY"));

		std::pair<int, int> Worlds;
		Worlds.first = Swappers.GetInt("WorldID");
		Worlds.second = Swappers.GetInt("TwoWorldID");

		vSwappers.emplace_back(SwapperPos, Worlds);
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

		GS()->Chat(-1, "'{~}' initiated area ('{}')!", Server()->ClientName(ClientID), Server()->GetWorldName(pData->GetID()));
	}
}