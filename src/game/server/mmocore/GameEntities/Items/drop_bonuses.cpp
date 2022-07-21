/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "drop_bonuses.h"

#include <game/server/gamecontext.h>

CDropBonuses::CDropBonuses(CGameWorld *pGameWorld, vec2 Pos, vec2 Vel, float AngleForce, int Type, int Value)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_DROPBONUS, Pos, 24)
{
	m_Pos = Pos;
	m_Vel = Vel;
	m_Angle = 0.0f;
	m_AngleForce = AngleForce;

	m_Value = Value;
	m_Type = Type;
	m_FlashTimer = 0;
	m_Flashing = false;
	m_LifeSpan = Server()->TickSpeed() * 15;
	GameWorld()->InsertEntity(this);
}

void CDropBonuses::Tick()
{
	m_LifeSpan--;
	if (m_LifeSpan < 0)
	{
		GS()->CreatePlayerSpawn(m_Pos);
		GS()->m_World.DestroyEntity(this);
		return;
	}

	// flashing
	if (m_LifeSpan < 150)
	{
		m_FlashTimer--;
		if (m_FlashTimer > 5)
			m_Flashing = true;
		else
		{
			m_Flashing = false;
			if (m_FlashTimer <= 0)
				m_FlashTimer = 10;
		}
	}

	// physic
	vec2 ItemSize = vec2(GetProximityRadius(), GetProximityRadius());
	GS()->Collision()->MovePhysicalAngleBox(&m_Pos, &m_Vel, ItemSize, &m_Angle, &m_AngleForce, 0.5f);

	// interactive
	CCharacter *pChar = (CCharacter*)GameWorld()->ClosestEntity(m_Pos, 16.0f, CGameWorld::ENTTYPE_CHARACTER, 0);
	if(pChar && pChar->GetPlayer() && !pChar->GetPlayer()->IsBot())
	{
		if(m_Type == PICKUP_HEALTH)
		{
			GS()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH);
		}
		// experience
		if(m_Type == PICKUP_ARMOR)
		{
			pChar->GetPlayer()->AddExp(m_Value);
			GS()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR);
		}

		GS()->m_World.DestroyEntity(this);
		return;
	}
}

void CDropBonuses::Snap(int SnappingClient)
{
	if(m_Flashing || NetworkClipped(SnappingClient))
		return;

	// client verification if the check is not drawn anymore
	if(GS()->IsMmoClient(SnappingClient))
	{
		CNetObj_MmoPickup* pObj = static_cast<CNetObj_MmoPickup*>(Server()->SnapNewItem(NETOBJTYPE_MMOPICKUP, GetID(), sizeof(CNetObj_MmoPickup)));
		if (!pObj)
			return;

		pObj->m_X = (int)m_Pos.x;
		pObj->m_Y = (int)m_Pos.y;
		pObj->m_Type = MMO_PICKUP_EXPERIENCE;
		pObj->m_Angle = (int)(m_Angle * 256.0f);
		return;
	}

	CNetObj_Pickup *pP = static_cast<CNetObj_Pickup *>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, GetID(), sizeof(CNetObj_Pickup)));
	if(!pP)
		return;

	pP->m_X = (int)m_Pos.x;
	pP->m_Y = (int)m_Pos.y;
	pP->m_Type = m_Type;
}