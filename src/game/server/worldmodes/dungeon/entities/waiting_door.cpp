#include "waiting_door.h"

#include <game/server/entity.h>
#include <game/server/gamecontext.h>

CEntityDungeonWaitingDoor::CEntityDungeonWaitingDoor(CGameWorld* pGameWorld, vec2 Pos)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_DUNGEON_DOOR, Pos)
{
	m_Closed = true;
	GS()->Collision()->FillLengthWall(32, vec2(0, -1), &m_Pos, &m_PosTo);
	GameWorld()->InsertEntity(this);
}


void CEntityDungeonWaitingDoor::Tick()
{
	if(!m_Closed)
		return;

	for(auto* pChar = (CCharacter*)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
	{
		vec2 IntersectPos;
		if(closest_point_on_line(m_Pos, m_PosTo, pChar->m_Core.m_Pos, IntersectPos))
		{
			float Distance = distance(IntersectPos, pChar->m_Core.m_Pos);
			if(Distance <= (float)g_Config.m_SvDoorRadiusHit)
			{
				pChar->SetDoorHit(m_Pos, m_PosTo);
				pChar->Die(pChar->GetPlayer()->GetCID(), WEAPON_WORLD);
			}
		}
	}
}


void CEntityDungeonWaitingDoor::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient) || !m_Closed)
		return;

	GS()->SnapLaser(SnappingClient, GetID(), m_Pos, m_PosTo, Server()->Tick() - 2, LASERTYPE_DOOR);
}