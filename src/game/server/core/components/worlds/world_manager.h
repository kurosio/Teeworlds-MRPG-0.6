/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_WORLDSWAP_CORE_H
#define GAME_SERVER_COMPONENT_WORLDSWAP_CORE_H

#include <game/server/core/tools/path_finder_vertex.h>
#include <game/server/core/mmo_component.h>

#include "world_data.h"

class CWorldManager : public MmoComponent
{
	inline static PathFinderVertex m_PathFinderBFS {};

	~CWorldManager() override;

	void OnInitWorld(const std::string& SqlQueryWhereWorld) override;
	void OnPostInit() override;

public:
	// Find a path to position by a given world ID
	std::optional<vec2> FindPosition(int WorldID, vec2 Pos) const;

	// Notify the player of unlocked zones by a leveling
	void NotifyUnlockedZonesByLeveling(CPlayer* pPlayer) const;
};

#endif