#include "personal_door.h"

#include <game/server/gamecontext.h>

CEntityPersonalDoor::CEntityPersonalDoor(CGameWorld* pGameWorld, int ClientID, vec2 Pos, vec2 Direction)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_BOT_WALL, Pos, 0, ClientID)
{
	GS()->Collision()->FillLengthWall(32, Direction, &m_Pos, &m_PosTo);
	GameWorld()->InsertEntity(this);
}

void CEntityPersonalDoor::HitCharacter(CCharacter* pChar)
{
	vec2 IntersectPos;
	if(closest_point_on_line(m_Pos, m_PosTo, pChar->m_Core.m_Pos, IntersectPos))
	{
		const float Distance = distance(IntersectPos, pChar->m_Core.m_Pos);
		if(Distance <= g_Config.m_SvDoorRadiusHit)
		{
			pChar->SetDoorHit(m_Pos, m_PosTo);
		}
	}
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
		HitCharacter(pChar);
	}
}

void CEntityPersonalDoor::Snap(int SnappingClient)
{
	if(m_ClientID != SnappingClient)
		return;

	if(NetworkClipped(SnappingClient))
		return;

	GS()->SnapLaser(SnappingClient, GetID(), m_Pos, m_PosTo, Server()->Tick(), LASERTYPE_FREEZE, LASERTYPE_DOOR);
}
