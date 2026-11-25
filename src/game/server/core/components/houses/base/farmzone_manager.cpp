#include "farmzone_manager.h"

#include <engine/server/links.h>
#include <game/server/core/entities/items/gathering_node.h>

#include <game/server/gamecontext.h>

CFarmzone::CFarmzone(CFarmzonesManager* pManager, const std::string& Name, const DBSet& ItemsSet, vec2 Pos, float Radius)
	: m_pManager(pManager)
{
	// initialize variables
	m_pManager = pManager;
	m_Node.Name = Name;
	m_Node.Level = 1;
	m_Node.Health = 100;
	m_Pos = Pos;
	m_Radius = Radius;

	// load items from node
	for(auto& Str : ItemsSet.getItems())
	{
		try
		{
			int itemID = std::stoi(Str);
			AddItemToNode(itemID);
		}
		catch(const std::invalid_argument& e)
		{
			dbg_msg("house_farmzone", "%s", e.what());
		}
	}
}

void CFarmzone::Add(CGS* pGS, const vec2& Pos)
{
	m_vFarms.push_back(new CEntityGatheringNode(&pGS->m_World, &m_Node, Pos, CEntityGatheringNode::GATHERING_NODE_PLANT));
}

void CFarmzone::NormalizeHealth()
{
	if(m_Node.m_vItems.isEmpty())
		return;

	// initialize variables
	struct NormalizeInfoElement
	{
		int Health {};
		double Chance {};
	};

	const auto& vPlantNodes = m_pManager->m_pHouse->GS()->Collision()->GetPlantNodes();
	const auto& vFarmItems = m_Node.m_vItems;
	std::unordered_map<int, NormalizeInfoElement> vResult;
	vResult.reserve(vFarmItems.size());

	// collect all best chances from nodes
	for(const auto& [nodeId, node] : vPlantNodes)
	{
		for(const auto& item : vFarmItems)
		{
			auto pElement = node.m_vItems.getElement(item.Element);
			if(!pElement)
				continue;

			if(!vResult.contains(item.Element) ||
				vResult[item.Element].Chance < pElement->Chance)
			{
				vResult[item.Element].Health = node.Health;
				vResult[item.Element].Chance = pElement->Chance;
			}
		}
	}

	// calculation result health for house node
	int ResultHealth = 0;
	double TargetChancePerPlant = 100.0 / vFarmItems.size();
	for(const auto& [itemId, node] : vResult)
	{
		double HealthScaleFactor = TargetChancePerPlant / node.Chance;
		auto NormalizedHealth = round_to_int(static_cast<double>(node.Health) * HealthScaleFactor);

		ResultHealth += NormalizedHealth;
	}

	// result
	m_Node.Health = maximum(10, ResultHealth);
	for(auto& pEnt : m_vFarms)
		pEnt->Reset();
}

void CFarmzone::AddItemToNode(int ItemID)
{
	// add new node
	m_Node.m_vItems.addElement(ItemID, 100.f);
	m_Node.m_vItems.setEqualChance(100.f);
	m_Node.m_vItems.normalizeChances();
	NormalizeHealth();
}


bool CFarmzone::RemoveItemFromNode(int ItemID)
{
	const bool Removed = m_Node.m_vItems.removeElement(ItemID);
	if(Removed)
	{
		m_Node.m_vItems.setEqualChance(100.f);
		m_Node.m_vItems.normalizeChances();
		NormalizeHealth();
	}
	return Removed;
}

void CFarmzone::SetItems(const std::vector<int>& Items)
{
	std::vector<int> Existing;
	for(const auto& Item : m_Node.m_vItems)
		Existing.push_back(Item.Element);

	for(const int ItemID : Existing)
		m_Node.m_vItems.removeElement(ItemID);

	for(const int ItemID : Items)
	{
		if(ItemID > 0)
			m_Node.m_vItems.addElement(ItemID, 100.f);
	}

	if(!m_Node.m_vItems.isEmpty())
	{
		m_Node.m_vItems.setEqualChance(100.f);
		m_Node.m_vItems.normalizeChances();
		NormalizeHealth();
	}
}


CFarmzonesManager::CFarmzonesManager(IHouse* pHouse, const std::string& JsonFarmzones)
{
	// initialize variables
	m_pHouse = pHouse;

	// load farmzones from json
	mystd::json::parse(JsonFarmzones, [this](nlohmann::json& pJson)
	{
		for(const auto& pFarmzone : pJson)
		{
			auto Farmname = pFarmzone.value("name", "");
			auto Position = pFarmzone.value("position", vec2());
			auto ItemsSet = DBSet(pFarmzone.value("items", ""));
			auto Radius = (float)pFarmzone.value("radius", 100);
			AddFarmzone(Farmname, ItemsSet, Position, Radius);
		}
	});
}


CFarmzonesManager::~CFarmzonesManager()
{
	m_vFarmzones.clear();
}


void CFarmzonesManager::AddFarmzone(const std::string& Farmname, const DBSet& ItemSet, vec2 Pos, float Radius)
{
	CFarmzone Zone(this, Farmname, ItemSet, Pos, Radius);
	m_vFarmzones.emplace(m_vFarmzones.size() + 1, Zone);
}


CFarmzone* CFarmzonesManager::GetFarmzoneByPos(vec2 Pos)
{
	for(auto& p : m_vFarmzones)
	{
		if(distance(p.second.GetPos(), Pos) <= p.second.GetRadius())
			return &p.second;
	}

	return nullptr;
}


CFarmzone* CFarmzonesManager::GetFarmzoneByID(int ID)
{
	const auto it = m_vFarmzones.find(ID);
	return it != m_vFarmzones.end() ? &it->second : nullptr;
}

void CFarmzonesManager::LoadItems(std::string_view JsonItems)
{
	const auto ItemsByZone = nlohmann::json::parse(std::string(JsonItems), nullptr, false);
	if(!ItemsByZone.is_object())
		return;

	for(auto It = ItemsByZone.begin(); It != ItemsByZone.end(); ++It)
	{
		const int ZoneID = std::atoi(It.key().c_str());
		auto* pZone = GetFarmzoneByID(ZoneID);
		if(!pZone || !It.value().is_array())
			continue;

		std::vector<int> Items;
		Items.reserve(It.value().size());
		for(const auto& Item : It.value())
		{
			if(Item.is_number_integer())
				Items.push_back(Item.get<int>());
		}
		pZone->SetItems(Items);
	}
}

std::string CFarmzonesManager::GetItemsJson() const
{
	nlohmann::json ItemsByZone = nlohmann::json::object();
	for(const auto& [ZoneID, Zone] : m_vFarmzones)
	{
		auto& Items = ItemsByZone[std::to_string(ZoneID)] = nlohmann::json::array();
		for(const auto& Item : Zone.m_Node.m_vItems)
			Items.push_back(Item.Element);
	}
	return ItemsByZone.dump();
}

void CFarmzonesManager::Save() const
{
	const char* pLinkType = m_pHouse->GetHouseType() == IHouse::Type::Guild ? "guild_house" : "player_house";
	Links::SaveJsonMeta(pLinkType, m_pHouse->GetID(), "farm_items", GetItemsJson());
}