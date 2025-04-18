/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_ENTITIES_ITEMS_DROP_ITEMS_H
#define GAME_SERVER_CORE_ENTITIES_ITEMS_DROP_ITEMS_H
#include <game/server/entity.h>

#include <game/server/core/components/inventory/item_data.h>

class CEntityDropItem : public CEntity
{
	CItem m_DropItem {};
	vec2 m_Vel {};
	int m_LifeSpan {};
	bool m_IsCurrency {};
	CFlashingTick m_Flash {};

public:
	CEntityDropItem(class CGameWorld *pGameWorld, vec2 Pos, vec2 Vel, float AngleForce, CItem DropItem, int OwnerID);

	void Tick() override;
	void Snap(int SnappingClient) override;

	void SetLifetime(int Lifetime)
	{
		m_LifeSpan = Lifetime;
	};
	bool TakeItem(int ClientID);
};

#endif
