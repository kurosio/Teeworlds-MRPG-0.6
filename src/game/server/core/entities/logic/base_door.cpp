#include "base_door.h"

#include <game/server/gamecontext.h>

CEntityBaseDoor::CEntityBaseDoor(CGameWorld* pGameWorld, int Enttype, vec2 Pos, vec2 Direction)
	: CEntity(pGameWorld, Enttype, Pos, 64)
{
	GS()->Collision()->FillLengthWall(Direction, &m_Pos, &m_PosTo);
	GS()->Collision()->SetDoorFromToCollisionAt(m_Pos, m_PosTo, TILE_STOPA, 0, GetID());

	GameWorld()->InsertEntity(this);
}

void CEntityBaseDoor::HitCharacter(CCharacter* pChar)
{
	pChar->SetDoorHit(GetID());
}

void CEntityBaseDoor::Tick()
{
	for(auto* pChar = (CCharacter*)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
	{
		if(is_within_distance_to_segment_sq(DOOR_ACTIVATION_RADIUS_SQUARED, m_Pos, m_PosTo, pChar->GetPos()))
			HitCharacter(pChar);
	}
}

void CEntityBaseDoor::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	GS()->SnapLaser(SnappingClient, GetID(), m_Pos, m_PosTo, Server()->Tick(), LASERTYPE_DOOR);
}
