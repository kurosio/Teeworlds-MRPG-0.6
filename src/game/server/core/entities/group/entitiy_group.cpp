#include "entitiy_group.h"

#include <game/server/gamecontext.h>

CEntityGroup::CEntityGroup(CGameWorld* pWorld, int DefaultEnttypeID, int ClientID)
	: m_DefaultEnttypeID(DefaultEnttypeID), m_pWorld(pWorld), m_ClientID(ClientID) {}

void CEntityGroup::RemoveFromWorld()
{
	const auto weakPtr = weak_from_this();
	if(const auto sharedPtr = weakPtr.lock())
		m_pWorld->m_EntityGroups.erase(sharedPtr);
}

CEntityGroup::~CEntityGroup()
{
	RemoveFromWorld();
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
		RemoveFromWorld();
	}
}

void CEntityGroup::Clear()
{
	RemoveFromWorld();
	m_vEntities.clear();
}

CBaseEntity* CEntityGroup::CreateBase(vec2 Pos, std::optional<int> EnttypeID)
{
	const int currentEnttypeID = EnttypeID.value_or(m_DefaultEnttypeID);
	const auto pBase = new CBaseEntity(m_pWorld, shared_from_this(), currentEnttypeID, Pos, m_ClientID);
	return pBase;
}

void CEntityGroup::ForEachEntity(const std::function<void(CBaseEntity*)>& func) const
{
	for(auto& entity : m_vEntities)
	{
		func(entity);
	}
}

CLaserEntity* CEntityGroup::CreateLaser(vec2 Pos, vec2 PosTo, int LaserType, std::optional<int> EnttypeID)
{
	const int currentEnttypeID = EnttypeID.value_or(m_DefaultEnttypeID);
	const auto pLaser = new CLaserEntity(m_pWorld, shared_from_this(), currentEnttypeID, Pos, PosTo, m_ClientID, LaserType);
	return pLaser;
}

CPickupEntity* CEntityGroup::CreatePickup(vec2 Pos, int Type, int Subtype, std::optional<int> EnttypeID)
{
	const int currentEnttypeID = EnttypeID.value_or(m_DefaultEnttypeID);
	const auto pPickup = new CPickupEntity(m_pWorld, shared_from_this(), currentEnttypeID, Pos, m_ClientID, Type, Subtype);
	return pPickup;
}