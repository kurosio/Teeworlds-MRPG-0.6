#include "object_destroy.h"

#include <game/server/gamecontext.h>

CEntityObjectDestroy::CEntityObjectDestroy(CGameWorld* pGameWorld, vec2 Pos, int NumClick)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_ACTION, Pos, 128)
{
	m_NumClick = NumClick;
	m_CurrentClick = 0;
	m_RespawnTick = 0;
	GameWorld()->InsertEntity(this);
}

void CEntityObjectDestroy::Tick()
{
	if(m_RespawnTick)
	{
		m_RespawnTick--;
		if(!m_RespawnTick)
			Spawn();
		return;
	}

	for(auto* pChar = (CCharacter*)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
	{
		if(distance(m_Pos, pChar->GetPos()) < GetRadius())
		{
			if(Server()->Input()->IsKeyClicked(pChar->GetClientID(), KEY_EVENT_FIRE_HAMMER))
			{
				m_CurrentClick++;
				if(m_CurrentClick >= m_NumClick)
				{
					m_RespawnTick = Server()->TickSpeed() * 5;
					break;
				}
			}
		}
	}
}

void CEntityObjectDestroy::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient) || !IsActive())
		return;

	GS()->SnapPickup(SnappingClient, GetID(), m_Pos, POWERUP_WEAPON, WEAPON_HAMMER);
}

void CEntityObjectDestroy::Spawn()
{
	m_CurrentClick = 0;
	m_RespawnTick = 0;
}
