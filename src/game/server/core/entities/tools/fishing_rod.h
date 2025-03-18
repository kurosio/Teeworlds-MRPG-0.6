#ifndef GAME_SERVER_CORE_ENTITIES_TOOLS_FISHING_ROD_H
#define GAME_SERVER_CORE_ENTITIES_TOOLS_FISHING_ROD_H

#include <game/server/entity.h>

class CPlayer;
class CEntityFishingRod : public CEntity
{
public:
	CEntityFishingRod(CGameWorld* pGameWorld, int ClientID, vec2 Position);

	void Tick() override;
	void Snap(int SnappingClient) override;
};

#endif