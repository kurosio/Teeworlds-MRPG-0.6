#include "item_source_helper.h"

#include <game/collision.h>
#include <game/server/gamecontext.h>

#include <components/Bots/BotData.h>
#include <components/crafting/craft_data.h>
#include <components/warehouse/warehouse_data.h>

namespace
{
	std::string JoinStrings(const std::set<std::string>& Values, const char* pDelimiter)
	{
		std::string Buffer {};
		bool First = true;
		for(const auto& Value : Values)
		{
			if(!First)
				Buffer += pDelimiter;
			Buffer += Value;
			First = false;
		}
		return Buffer;
	}

	std::string JoinStrings(const std::vector<std::string>& Values, const char* pDelimiter)
	{
		std::string Buffer {};
		for(size_t i = 0; i < Values.size(); i++)
		{
			if(i > 0)
				Buffer += pDelimiter;
			Buffer += Values[i];
		}
		return Buffer;
	}
}

std::string BuildItemSourceHint(CGS* pGS, int ItemID)
{
	if(!pGS)
		return "Ask players/NPCs";

	std::vector<std::string> vHints {};

	// get info from craft
	for(const auto* pCraft : CCraftItem::Data())
	{
		if(!pCraft || pCraft->GetItem()->GetID() != ItemID)
			continue;

		vHints.emplace_back("Craft");
		break;
	}

	// get info from mobs
	std::set<std::string> vMobNames {};
	for(const auto& [MobID, MobInfo] : MobBotInfo::ms_aMobBot)
	{
		for(auto i = 0; i < MAX_DROPPED_FROM_MOBS; i++)
		{
			if(MobInfo.m_aDropItem[i] != ItemID)
				continue;

			vMobNames.emplace(MobInfo.GetName());
			break;
		}

		if(vMobNames.size() >= 2)
			break;
	}

	if(!vMobNames.empty())
		vHints.emplace_back(fmt_default("{}", JoinStrings(vMobNames, ", ")));

	// get info from warehouses
	std::set<std::string> vWarehouseNames {};
	for(const auto* pWarehouse : CWarehouse::Data())
	{
		if(!pWarehouse || pWarehouse->IsHasFlag(WF_SELL))
			continue;

		const auto& vTrades = pWarehouse->GetTradingList();
		const bool HasTrade = std::ranges::any_of(vTrades, [ItemID](const CTrade& Trade)
		{
			return Trade.GetItem()->GetID() == ItemID;
		});

		if(!HasTrade)
			continue;

		vWarehouseNames.insert(pWarehouse->GetName());
		if(vWarehouseNames.size() >= 2)
			break;
	}

	if(!vWarehouseNames.empty())
		vHints.emplace_back(fmt_default("{}", JoinStrings(vWarehouseNames, ", ")));

	// get info from nodes
	const auto* pCollision = pGS->Collision();
	if(pCollision)
	{
		auto hasItemInNodes = [ItemID](const std::unordered_map<int, GatheringNode>& vNodes) -> bool
		{
			for(const auto& [NodeID, Node] : vNodes)
			{
				(void)NodeID;
				if(Node.m_vItems.hasElement(ItemID))
					return true;
			}
			return false;
		};

		if(hasItemInNodes(pCollision->GetOreNodes()))
			vHints.emplace_back("Mining");

		if(hasItemInNodes(pCollision->GetPlantNodes()))
			vHints.emplace_back("Plant");

		if(hasItemInNodes(pCollision->GetFishNodes()))
			vHints.emplace_back("Fishing");
	}

	if(vHints.empty())
		return "Ask players/NPCs";

	// build result
	return JoinStrings(vHints, " | ");
}
