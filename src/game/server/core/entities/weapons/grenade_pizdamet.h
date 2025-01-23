#ifndef GAME_SERVER_CORE_ENTITIES_WEAPONS_GRENADE_PIZDAMET_H
#define GAME_SERVER_CORE_ENTITIES_WEAPONS_GRENADE_PIZDAMET_H
#include <game/server/entity.h>

class CEntityGrenadePizdamet : public CEntity
{
	vec2 m_Direction {};

public:
	CEntityGrenadePizdamet(CGameWorld* pGameWorld, int OwnerCID, vec2 Pos, vec2 Direction);

	void Tick() override;
	void Snap(int SnappingClient) override;
};

#endif
