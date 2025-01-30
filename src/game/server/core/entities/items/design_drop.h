#ifndef GAME_SERVER_CORE_ENTITIES_ITEMS_DISIGN_DROP_H
#define GAME_SERVER_CORE_ENTITIES_ITEMS_DISIGN_DROP_H

#include <game/server/entity.h>

class CEntityDesignDrop : public CEntity
{
	vec2 m_Vel {};
	int64_t m_Mask {};
	int m_Type {};
	int m_Subtype {};
	int m_LifeSpan {};

public:
	CEntityDesignDrop(CGameWorld* pGameWorld, int LifeSpan, vec2 Pos, vec2 Vel, int Type, int Subtype, int64_t Mask);

	void Tick() override;
	void Snap(int SnappingClient) override;
};

#endif
