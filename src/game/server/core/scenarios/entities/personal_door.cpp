#include "personal_door.h"

#include <game/server/gamecontext.h>

CEntityPersonalDoor::CEntityPersonalDoor(CGameWorld* pGameWorld, int ClientID, vec2 Pos, vec2 Direction)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_BOT_DOOR, Pos, 0, ClientID)
{
	GS()->Collision()->FillLengthWall(Direction, &m_Pos, &m_PosTo);
	GS()->Collision()->SetDoorFromToCollisionAt(m_Pos, m_PosTo, TILE_STOPA, 0);

	GameWorld()->InsertEntity(this);
}

void CEntityPersonalDoor::Tick()
{
	const auto* pPlayer = GS()->GetPlayer(m_ClientID);
	if(!pPlayer)
	{
		MarkForDestroy();
		return;
	}

	if(auto* pChar = pPlayer->GetCharacter())
	{
		vec2 IntersectPos;
		if(closest_point_on_line(m_Pos, m_PosTo, pChar->m_Core.m_Pos, IntersectPos))
		{
			if(distance(IntersectPos, pChar->GetPos()) < 128.f)
				pChar->SetDoorHit();
		}
	}
}

void CEntityPersonalDoor::Snap(int SnappingClient)
{
	if(m_ClientID != SnappingClient)
		return;

	if(NetworkClipped(SnappingClient))
		return;

	GS()->SnapLaser(SnappingClient, GetID(), m_Pos, m_PosTo, Server()->Tick(), LASERTYPE_DOOR);
}
