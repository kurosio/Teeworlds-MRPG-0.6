/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_HOUSE_ENTITIES_DOOR_H
#define GAME_SERVER_COMPONENT_HOUSE_ENTITIES_DOOR_H
#include <game/server/entity.h>

class HouseDoor : public CEntity
{
	class CHouseDoorData* m_pHouseDoor;

public:
	HouseDoor(CGameWorld* pGameWorld, vec2 Pos, CHouseDoorData* pHouseDoor);

	void Tick() override;
	void Snap(int SnappingClient) override;
};

#endif