#ifndef GAME_SERVER_CORE_ENTITIES_TOOLS_FISHING_ROD_H
#define GAME_SERVER_CORE_ENTITIES_TOOLS_FISHING_ROD_H

#include "../physic_tools/rope.h"
#include <game/server/entity.h>

class CPlayer;
class CEntityFishingRod : public CEntity
{
	enum
	{
		ROD = 0,
		NUM_ROD_POINTS = 3,

		ROPE,
		NUM_ROPE_POINTS = 10,
	};

	vec2 m_LastPoint {};
	RopePhysic m_Rope {};

public:
	CEntityFishingRod(CGameWorld* pGameWorld, int ClientID, vec2 Position, vec2 Force);

	void Tick() override;
	void Snap(int SnappingClient) override;
};

#endif