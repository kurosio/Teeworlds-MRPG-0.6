/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_WORLDSWAP_CORE_H
#define GAME_SERVER_COMPONENT_WORLDSWAP_CORE_H

#include <game/server/core/utilities/pathfinder_vertex.h>
#include <game/server/core/mmo_component.h>

#include "WorldData.h"

class CWorldManager : public MmoComponent
{
	PathFinderVertex m_PathFinderBFS {};

	~CWorldManager() override
	{
		CWorldData::Data().clear();
	}

	void OnInitWorld(const char* pWhereLocalWorld) override;

public:
	// Get the type of world
	int GetWorldType() const;

	// Find the position of a world given its ID
	void FindPosition(int WorldID, vec2 Pos, vec2* OutPos);

	// Notify the player of unlocked zones by a given quest
	void NotifyUnlockedZonesByQuest(CPlayer* pPlayer, int QuestID) const;
};

#endif