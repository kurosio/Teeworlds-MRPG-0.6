#include "entitiy_group.h"

#include <game/server/gamecontext.h>

CEntityGroup::CEntityGroup(CGameWorld* pWorld, int ClientID)
	: m_pWorld(pWorld), m_ClientID(ClientID) {}

CEntityGroup::~CEntityGroup()
{
	const auto weakPtr = weak_from_this();
	if(const auto sharedPtr = weakPtr.lock())
	{
		m_pWorld->m_EntityGroups.erase(sharedPtr);
	}
	m_vEntities.clear();
}

void CEntityGroup::AddEntity(CBaseEntity* pEnt)
{
	m_vEntities.push_back(pEnt);
}

void CEntityGroup::RemoveEntity(CBaseEntity* pEnt)
{
	m_vEntities.erase(std::ranges::remove(m_vEntities, pEnt).begin(), m_vEntities.end());
	if(m_vEntities.empty())
	{
		m_pWorld->m_EntityGroups.erase(shared_from_this());
	}
}

void CEntityGroup::Clear()
{
	m_pWorld->m_EntityGroups.erase(shared_from_this());
	m_vEntities.clear();
}

void CEntityGroup::ForEachEntity(const std::function<void(CBaseEntity*)>& func) const
{
	for(auto& entity : m_vEntities)
	{
		func(entity);
	}
}

CLaserEntity* CEntityGroup::CreateLaser(vec2 Pos, vec2 PosTo, int LaserType)
{
	const auto pLaser = new CLaserEntity(m_pWorld, shared_from_this(), Pos, PosTo, m_ClientID, LaserType);
	return pLaser;
}

CPickupEntity* CEntityGroup::CreatePickup(vec2 Pos, int Type, int Subtype)
{
	const auto pPickup = new CPickupEntity(m_pWorld, shared_from_this(), Pos, m_ClientID, Type, Subtype);
	return pPickup;
}