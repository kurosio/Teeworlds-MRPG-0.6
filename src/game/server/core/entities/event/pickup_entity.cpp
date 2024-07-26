#include "pickup_entity.h"
#include <game/server/gamecontext.h>

CPickupEntity::CPickupEntity(CGameWorld* pGameWorld, const std::shared_ptr<CEntityGroup>& group, vec2 Pos, int Owner, int Type, int Subtype)
	: CBaseEntity(pGameWorld, group, CGameWorld::ENTTYPE_PICKUP, Pos, Owner)
{
	m_Options.Type = Type;
	m_Options.Subtype = Subtype;
}

void CPickupEntity::Tick()
{
	if(m_ClientID != -1 && !GetPlayer())
	{
		MarkForDestroy();
		return;
	}

	CBaseEntity::Tick();
}

void CPickupEntity::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	CNetObj_Pickup* pObj = static_cast<CNetObj_Pickup*>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, GetID(), sizeof(CNetObj_Pickup)));
	if(!pObj)
		return;

	pObj->m_X = (int)m_Pos.x;
	pObj->m_Y = (int)m_Pos.y;
	pObj->m_Type = m_Options.Type;
	pObj->m_Subtype = m_Options.Subtype;

	CBaseEntity::Snap(SnappingClient);
}