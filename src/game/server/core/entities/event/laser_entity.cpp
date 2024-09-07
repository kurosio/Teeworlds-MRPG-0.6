#include "laser_entity.h"
#include <game/server/gamecontext.h>

CLaserEntity::CLaserEntity(CGameWorld* pGameWorld, const std::shared_ptr<CEntityGroup>& group, vec2 Pos, vec2 PosTo, int Owner, int LaserType)
	: CBaseEntity(pGameWorld, group, CGameWorld::ENTTYPE_LASER, Pos, Owner)
{
	m_PosTo = PosTo;
	m_Options.LaserType = LaserType;
}

void CLaserEntity::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient) && NetworkClipped(SnappingClient, m_PosTo))
		return;

	if(!GS()->SnapLaser(SnappingClient, GetID(), m_Pos, m_PosTo, Server()->Tick() - m_Options.StartTickShift, 
		m_Options.LaserType, m_Options.LaserSubtype, m_ClientID, m_Options.LaserFlags))
		return;

	CBaseEntity::Snap(SnappingClient);
}