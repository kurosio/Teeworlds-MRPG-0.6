/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "drop_bonuses.h"

#include <game/server/gamecontext.h>

CDropBonuses::CDropBonuses(CGameWorld *pGameWorld, vec2 Pos, vec2 Vel, int Type, int Value)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_DROPBONUS, Pos, 24)
{
	m_Pos = Pos;
	m_Vel = Vel;

	m_Value = Value;
	m_Type = Type;
	m_Flash.InitFlashing(&m_LifeSpan);
	m_LifeSpan = Server()->TickSpeed() * 15;
	GameWorld()->InsertEntity(this);
}

void CDropBonuses::Tick()
{
	m_LifeSpan--;
	if (m_LifeSpan < 0)
	{
		GS()->CreatePlayerSpawn(m_Pos);
		GameWorld()->DestroyEntity(this);
		return;
	}

	// flashing
	m_Flash.OnTick();

	// physic
	GS()->Collision()->MovePhysicalBox(&m_Pos, &m_Vel, vec2(GetProximityRadius(), GetProximityRadius()), 0.5f);

	// interactive
	CCharacter *pChar = (CCharacter*)GameWorld()->ClosestEntity(m_Pos, 16.0f, CGameWorld::ENTTYPE_CHARACTER, 0);
	if(pChar && !pChar->GetPlayer()->IsBot())
	{
		if(m_Type == POWERUP_HEALTH)
		{
			GS()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH);
		}

		// experience
		if(m_Type == POWERUP_ARMOR)
		{
			pChar->GetPlayer()->AddExp(m_Value);
			GS()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR);
		}

		GameWorld()->DestroyEntity(this);
	}
}

void CDropBonuses::Snap(int SnappingClient)
{
	if(m_Flash.IsFlashing() || NetworkClipped(SnappingClient))
		return;

	CNetObj_Pickup *pP = static_cast<CNetObj_Pickup *>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, GetID(), sizeof(CNetObj_Pickup)));
	if(!pP)
		return;

	pP->m_X = (int)m_Pos.x;
	pP->m_Y = (int)m_Pos.y;
	pP->m_Type = m_Type;
	pP->m_Subtype = 0;
}