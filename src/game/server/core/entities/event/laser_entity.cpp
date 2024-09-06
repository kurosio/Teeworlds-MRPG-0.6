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

	if(GS()->GetClientVersion(SnappingClient) >= VERSION_DDNET_MULTI_LASER)
	{
		CNetObj_DDNetLaser* pObj = static_cast<CNetObj_DDNetLaser*>(Server()->SnapNewItem(NETOBJTYPE_DDNETLASER, GetID(), sizeof(CNetObj_DDNetLaser)));
		if(!pObj)
			return;

		pObj->m_ToX = (int)m_Pos.x;
		pObj->m_ToY = (int)m_Pos.y;
		pObj->m_FromX = (int)m_PosTo.x;
		pObj->m_FromY = (int)m_PosTo.y;
		pObj->m_StartTick = Server()->Tick() - m_Options.StartTickShift;
		pObj->m_Owner = -1;
		pObj->m_Type = m_Options.LaserType;
	}
	else
	{
		CNetObj_Laser* pObj = static_cast<CNetObj_Laser*>(Server()->SnapNewItem(NETOBJTYPE_LASER, GetID(), sizeof(CNetObj_Laser)));
		if(!pObj)
			return;

		pObj->m_X = (int)m_Pos.x;
		pObj->m_Y = (int)m_Pos.y;
		pObj->m_FromX = (int)m_PosTo.x;
		pObj->m_FromY = (int)m_PosTo.y;
		pObj->m_StartTick = Server()->Tick() - m_Options.StartTickShift;
	}

	CBaseEntity::Snap(SnappingClient);
}