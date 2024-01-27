/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GUILD_HOUSE_PLANTZONES_MANAGER_H
#define GAME_SERVER_COMPONENT_GUILD_HOUSE_PLANTZONES_MANAGER_H

#include "GuildHousePlantzoneData.h"

class CGS;
class CGuildHouseData;

class CGuildHousePlantzonesManager
{
	CGS* GS() const;

	CGuildHouseData* m_pHouse {};
	std::vector<CGuildHousePlantzoneData> m_vPlantzones{};

public:
	CGuildHousePlantzonesManager() = delete;
	CGuildHousePlantzonesManager(CGuildHouseData* pHouse, std::string&& JsPlantzones);

	std::vector<CGuildHousePlantzoneData>& GetContainer() { return m_vPlantzones; }

	void AddPlantzone(CGuildHousePlantzoneData&& Plantzone);
	const CGuildHousePlantzoneData* GetPlantzone(vec2 Pos) const;

private:
	void Save() const;
};
#endif