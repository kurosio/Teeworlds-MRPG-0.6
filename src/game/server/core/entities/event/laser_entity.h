#ifndef GAME_SERVER_ENTITIES_EVENT_LASER_ENTITY_H
#define GAME_SERVER_ENTITIES_EVENT_LASER_ENTITY_H

#include "base_entity.h"

struct LaserOptions
{
	int LaserType = LASERTYPE_RIFLE;
	int StartTickShift = 3;
};

class CLaserEntity final : public CBaseEntity
{
	LaserOptions m_Options;

public:
	CLaserEntity(CGameWorld* pGameWorld, const std::shared_ptr<CEntityGroup>& group, vec2 Pos, vec2 PosTo, int Owner, int LaserType = LASERTYPE_RIFLE);

	void Tick() override;
	void Snap(int SnappingClient) override;

	LaserOptions& GetOptions() { return m_Options; }
};

#endif