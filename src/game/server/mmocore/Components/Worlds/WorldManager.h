/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_WORLDSWAP_CORE_H
#define GAME_SERVER_COMPONENT_WORLDSWAP_CORE_H

#include <game/server/mmocore/Utils/PathFinderDoubleNode.h>

#include <game/server/mmocore/MmoComponent.h>

#include "WorldData.h"

class CWorldManager : public MmoComponent
{
	~CWorldManager() override
	{
		CWorldData::Data().clear();
	}

	PathFinderDoubleNode m_PathFinderBFS{};

	void OnInitWorld(const char* pWhereLocalWorld) override;

public:
	int GetWorldType() const;
	void FindPosition(int WorldID, vec2 Pos, vec2* OutPos);
	void NotifyUnlockedZonesByQuest(CPlayer* pPlayer, int QuestID) const;
};

#endif