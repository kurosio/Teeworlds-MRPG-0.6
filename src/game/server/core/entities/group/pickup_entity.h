#ifndef GAME_SERVER_ENTITIES_EVENT_PICKUP_ENTITY_H
#define GAME_SERVER_ENTITIES_EVENT_PICKUP_ENTITY_H

#include "base_entity.h"

struct PickupOptions
{
	int Type = POWERUP_HEALTH;
	int Subtype = 0;
};

class CPickupEntity final : public CBaseEntity
{
	PickupOptions m_Options;

public:
	CPickupEntity(CGameWorld* pGameWorld, const std::shared_ptr<CEntityGroup>& group, int EnttypeID, vec2 Pos, int Owner, int Type = POWERUP_HEALTH, int Subtype = 0);

	void Snap(int SnappingClient) override;

	PickupOptions& GetOptions() { return m_Options; }
};

#endif