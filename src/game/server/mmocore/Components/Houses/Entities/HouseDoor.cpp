/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "HouseDoor.h"

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>

#include "game/server/mmocore/Components/Houses/HouseDoorData.h"

HouseDoor::HouseDoor(CGameWorld* pGameWorld, vec2 Pos, CHouseDoorData* pHouseDoor)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_PLAYER_HOUSE_DOOR, Pos), m_pHouseDoor(pHouseDoor)
{
	m_Pos.y += 30;
	m_PosTo = GS()->Collision()->FindDirCollision(100, m_PosTo, 'y', '-');
	GameWorld()->InsertEntity(this);
}

void HouseDoor::Tick()
{
	for(CCharacter* pChar = (CCharacter*)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
	{
		vec2 IntersectPos;
		if(closest_point_on_line(m_Pos, m_PosTo, pChar->m_Core.m_Pos, IntersectPos))
		{
			const float Distance = distance(IntersectPos, pChar->m_Core.m_Pos);
			if(Distance <= g_Config.m_SvDoorRadiusHit)
			{
				// skip who has access to house door
				if(m_pHouseDoor->HasAccess(pChar->GetPlayer()->Acc().GetID()))
					continue;

				// skip eidolon when owner has access
				if(pChar->GetPlayer()->IsBot())
				{
					CPlayerBot* pPlayerBot = static_cast<CPlayerBot*>(pChar->GetPlayer());
					if(pPlayerBot->GetEidolonOwner() && m_pHouseDoor->HasAccess(pPlayerBot->GetEidolonOwner()->Acc().GetID()))
						continue;
				}

				// hit door
				pChar->m_DoorHit = true;
			}
		}
	}
}

void HouseDoor::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	if(GS()->GetClientVersion(SnappingClient) >= VERSION_DDNET_MULTI_LASER)
	{
		CNetObj_DDNetLaser *pObj = static_cast<CNetObj_DDNetLaser *>(Server()->SnapNewItem(NETOBJTYPE_DDNETLASER, GetID(), sizeof(CNetObj_DDNetLaser)));
		if(!pObj)
			return;
			
		pObj->m_ToX = int(m_Pos.x);
		pObj->m_ToY = int(m_Pos.y);
		pObj->m_FromX = int(m_PosTo.x);
		pObj->m_FromY = int(m_PosTo.y);
		pObj->m_StartTick = Server()->Tick() - 2;
		pObj->m_Owner = -1;
		pObj->m_Type = LASERTYPE_DOOR;
	}
	else
	{
		CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, GetID(), sizeof(CNetObj_Laser)));
		if(!pObj)
			return;
			
		pObj->m_X = int(m_Pos.x);
		pObj->m_Y = int(m_Pos.y);
		pObj->m_FromX = int(m_PosTo.x);
		pObj->m_FromY = int(m_PosTo.y);
		pObj->m_StartTick = Server()->Tick() - 2;
	}
}