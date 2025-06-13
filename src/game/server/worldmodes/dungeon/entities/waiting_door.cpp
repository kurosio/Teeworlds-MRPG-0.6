#include "waiting_door.h"

#include <game/server/entity.h>
#include <game/server/gamecontext.h>

CEntityDungeonWaitingDoor::CEntityDungeonWaitingDoor(CGameWorld* pGameWorld, vec2 Pos)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_DUNGEON_DOOR, Pos)
{
	GS()->Collision()->FillLengthWall(vec2(0, -1), &m_Pos, &m_PosTo);
	GS()->Collision()->SetDoorFromToCollisionAt(m_Pos, m_PosTo, TILE_STOPA, 0, GetID());

	m_Closed = true;

	GameWorld()->InsertEntity(this);
}


void CEntityDungeonWaitingDoor::Tick()
{
	if(!m_Closed)
		return;

	for(auto* pChar = (CCharacter*)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
	{
		if(is_within_distance_to_segment_sq(DOOR_ACTIVATION_RADIUS_SQUARED, m_Pos, m_PosTo, pChar->m_Core.m_Pos))
			pChar->SetDoorHit(GetID());
	}
}


void CEntityDungeonWaitingDoor::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient) || !m_Closed)
		return;

	GS()->SnapLaser(SnappingClient, GetID(), m_Pos, m_PosTo, Server()->Tick() - 2, LASERTYPE_DOOR);
}