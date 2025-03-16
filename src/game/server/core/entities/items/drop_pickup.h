/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_ENTITIES_ITEMS_DROP_PICKUP_H
#define GAME_SERVER_CORE_ENTITIES_ITEMS_DROP_PICKUP_H
#include <game/server/entity.h>

class CEntityDropPickup : public CEntity
{
	vec2 m_Vel;
	int m_Type;
	int m_Subtype;
	int m_Value;
	int m_LifeSpan;
	CFlashingTick m_Flash;

public:
	CEntityDropPickup(CGameWorld* pGameWorld, vec2 Pos, vec2 Vel, int Type, int Subtype, int Value);

	void Tick() override;
	void Snap(int SnappingClient) override;
};

#endif
