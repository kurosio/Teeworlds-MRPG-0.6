#include "farmzone_manager.h"

#include <game/server/core/entities/items/gathering_node.h>
#include <game/server/gamecontext.h>

void CFarmzone::Add(CGS* pGS, const vec2& Pos)
{
	m_vFarms.push_back(new CEntityGatheringNode(&pGS->m_World, &m_Node, Pos, CEntityGatheringNode::GATHERING_NODE_PLANT));
}


void CFarmzone::AddItemToNode(int ItemID)
{
	// add new node
	m_Node.m_vItems.addElement(ItemID, 100.f);
	m_Node.m_vItems.setEqualChance(100.f);
	m_Node.m_vItems.normalizeChances();
}


bool CFarmzone::RemoveItemFromNode(int ItemID)
{
	// try remove
	bool Removed = m_Node.m_vItems.removeElement(ItemID);
	if(Removed)
	{
		m_Node.m_vItems.setEqualChance(100.f);
		m_Node.m_vItems.normalizeChances();
	}

	return Removed;
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


void CFarmzonesManager::Save() const
{
	// prepare json data
	nlohmann::json Farmzones;
	for(auto& p : m_vFarmzones)
	{
		nlohmann::json farmzoneData;
		farmzoneData["name"] = p.second.GetName();
		farmzoneData["position"] = p.second.GetPos();
		farmzoneData["radius"] = round_to_int(p.second.GetRadius());

		std::string Items {};
		for(const auto& Item : p.second.m_Node.m_vItems)
		{
			Items += std::to_string(Item.Element);
			Items += ',';
		}
		if(!Items.empty())
			Items.pop_back();

		farmzoneData["items"] = Items;
		Farmzones.push_back(farmzoneData);
	}

	// save to database
	Database->Execute<DB::UPDATE>(m_pHouse->GetTableName(),
		"Farmzones = '{}' WHERE ID = '{}'",
		Farmzones.dump(4), m_pHouse->GetID());
}