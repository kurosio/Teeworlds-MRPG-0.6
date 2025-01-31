/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_DROPINGITEMS_H
#define GAME_SERVER_ENTITIES_DROPINGITEMS_H
#include <game/server/entity.h>

#include <game/server/core/components/Inventory/ItemData.h>

class CDropItem : public CEntity
{
	CItem m_DropItem;
	int m_OwnerID;
	vec2 m_Vel;
	int m_LifeSpan;
	CFlashingTick m_Flash;

public:
	CDropItem(class CGameWorld *pGameWorld, vec2 Pos, vec2 Vel, float AngleForce, CItem DropItem, int OwnerID);

	void Tick() override;
	void Snap(int SnappingClient) override;

	void SetLifetime(int Lifetime)
	{
		m_LifeSpan = Lifetime;
	};
	bool TakeItem(int ClientID);
};

#endif
