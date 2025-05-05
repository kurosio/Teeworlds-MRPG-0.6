/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_SCENARIOS_ENTITIES_PERSONAL_DOOR_H
#define GAME_SERVER_SCENARIOS_ENTITIES_PERSONAL_DOOR_H

#include <game/server/entity.h>

class CEntityPersonalDoor : public CEntity
{
public:
	CEntityPersonalDoor(CGameWorld* pGameWorld, int ClientID, vec2 Pos, vec2 Direction);

	void Tick() override;
	void Snap(int SnappingClient) override;
};

#endif
