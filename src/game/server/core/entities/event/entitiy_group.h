#ifndef GAME_SERVER_ENTITIES_EVENT_ENTITY_GROUP_H
#define GAME_SERVER_ENTITIES_EVENT_ENTITY_GROUP_H

#include "laser_entity.h"
#include "pickup_entity.h"

class CPlayer;
class CLaserEntity;
class CGameWorld;

class CEntityGroup : public std::enable_shared_from_this<CEntityGroup>, public mystd::CConfigurable
{
	CGameWorld* m_pWorld{};
	int m_ClientID{};

public:
	static std::shared_ptr<CEntityGroup> NewGroup(CGameWorld* pWorld, int ClientID = -1)
	{
		auto groupPtr = std::shared_ptr<CEntityGroup>(new CEntityGroup(pWorld, ClientID));
		pWorld->m_EntityGroups.insert(groupPtr);
		return groupPtr;
	}
	~CEntityGroup();

private:
	CEntityGroup(CGameWorld* pWorld, int ClientID = -1);
	void RemoveFromWorld();

public:
	void AddEntity(CBaseEntity* pEnt);
	void ForEachEntity(const std::function<void(CBaseEntity*)>& func) const;
	void RemoveEntity(CBaseEntity* pEnt);
	void Clear();

	CBaseEntity* CreateBase(vec2 Pos);
	CLaserEntity* CreateLaser(vec2 Pos, vec2 PosTo, int LaserType = LASERTYPE_RIFLE);
	CPickupEntity* CreatePickup(vec2 Pos, int Type = POWERUP_HEALTH, int Subtype = 0);

	bool IsActive() const { return (int)m_vEntities.size() > 0; }

private:
	std::vector<CBaseEntity*> m_vEntities;
};

#endif
