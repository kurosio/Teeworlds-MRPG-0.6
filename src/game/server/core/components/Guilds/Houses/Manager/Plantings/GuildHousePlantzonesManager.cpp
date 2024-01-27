/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildHousePlantzonesManager.h"

#include <game/server/core/components/Guilds/Houses/GuildHouseData.h>

#include <game/server/gamecontext.h>

CGS* CGuildHousePlantzonesManager::GS() const { return m_pHouse->GS(); }

CGuildHousePlantzonesManager::CGuildHousePlantzonesManager(CGuildHouseData* pHouse, std::string&& JsPlantzones) : m_pHouse(pHouse)
{
	// Parse the JSON string
	Tools::Json::parseFromString(JsPlantzones, [this](nlohmann::json& pJson)
	{
		for(const auto& pPlantzone : pJson)
		{
			std::string Plantname = pPlantzone.value("name", "");
			vec2 Position = vec2(pPlantzone.value("x", 0), pPlantzone.value("y", 0));
			int ItemID = pPlantzone.value("item_id", 0);
			float Radius = pPlantzone.value("radius", 100);
			AddPlantzone({this, Plantname.c_str(), ItemID, Position, Radius });
		}
	});
}

// Destructor for the CHouseDoorsController class
CGuildHousePlantzonesManager::~CGuildHousePlantzonesManager()
{
	m_vPlantzones.clear();
}

void CGuildHousePlantzonesManager::AddPlantzone(CGuildHousePlantzoneData&& Plantzone)
{
	m_vPlantzones.push_back(std::forward<CGuildHousePlantzoneData>(Plantzone));
}

const CGuildHousePlantzoneData* CGuildHousePlantzonesManager::GetPlantzone(vec2 Pos) const
{
	for(const auto& p : m_vPlantzones)
	{
		if(distance(p.GetPos(), Pos) <= p.GetRadius())
			return &p;
	}
	return nullptr;
}

void CGuildHousePlantzonesManager::Save() const
{
	// Create a JSON object to store plant zones data
	nlohmann::json Plantzones;
	for(auto& p : m_vPlantzones)
	{
		// Create a JSON object to store data for each plant zone
		nlohmann::json plantzoneData;
		plantzoneData["name"] = p.GetName();
		plantzoneData["x"] = round_to_int(p.GetPos().x);
		plantzoneData["y"] = round_to_int(p.GetPos().y);
		plantzoneData["item_id"] = p.GetItemID();
		plantzoneData["radius"] = round_to_int(p.GetRadius());
		Plantzones.push_back(plantzoneData);
	}

	Database->Execute<DB::UPDATE>(TW_GUILDS_HOUSES, "Plantzones = '%s' WHERE ID = '%d'", Plantzones.dump().c_str(), m_pHouse->GetID());
}
