/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_NPCWALL_H
#define GAME_SERVER_ENTITIES_NPCWALL_H

#include <game/server/entity.h>

class CEntityPersonalDoor : public CEntity
{
public:
	CEntityPersonalDoor(CGameWorld* pGameWorld, int ClientID, vec2 Pos, vec2 Direction);
	void HitCharacter(CCharacter* pChar);

	void Tick() override;
	void Snap(int SnappingClient) override;
};

#endif
