/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_ENTITIES_LOGIC_BASE_DOOR_H
#define GAME_SERVER_CORE_ENTITIES_LOGIC_BASE_DOOR_H

#include <game/server/entity.h>

class CEntityBaseDoor : public CEntity
{
public:
	CEntityBaseDoor(CGameWorld* pGameWorld, int Enttype, vec2 Pos, vec2 Direction);

	void Tick() override;
	void Snap(int SnappingClient) override;

private:
	void HitCharacter(CCharacter* pChar);
};

#endif
