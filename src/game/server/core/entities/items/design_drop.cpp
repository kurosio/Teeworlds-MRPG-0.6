#include "design_drop.h"

#include <game/server/gamecontext.h>

CEntityDesignDrop::CEntityDesignDrop(CGameWorld *pGameWorld, int LifeSpan, vec2 Pos, vec2 Vel, int Type, int Subtype, int64_t Mask)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_VISUAL, Pos, 24.f)
{
	m_Vel = Vel;
	m_Type = Type;
	m_Subtype = Subtype;
	m_LifeSpan = LifeSpan;
	m_Mask = Mask;
	GameWorld()->InsertEntity(this);
}

void CEntityDesignDrop::Tick()
{
	// life span
	m_LifeSpan--;
	if (m_LifeSpan < 0)
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	// move box
	GS()->Collision()->MovePhysicalBox(&m_Pos, &m_Vel, vec2(m_Radius, m_Radius), 0.5f);
}

void CEntityDesignDrop::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient) || !CmaskIsSet(m_Mask, SnappingClient))
		return;

	GS()->SnapPickup(SnappingClient, GetID(), m_Pos, m_Type, m_Subtype);
}