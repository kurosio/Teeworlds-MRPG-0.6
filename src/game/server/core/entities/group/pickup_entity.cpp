#include "pickup_entity.h"
#include <game/server/gamecontext.h>

CPickupEntity::CPickupEntity(CGameWorld* pGameWorld, const std::shared_ptr<CEntityGroup>& group, int EnttypeID, vec2 Pos, int Owner, int Type, int Subtype)
	: CBaseEntity(pGameWorld, group, EnttypeID, Pos, Owner)
{
	m_Options.Type = Type;
	m_Options.Subtype = Subtype;
}

void CPickupEntity::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	GS()->SnapPickup(SnappingClient, GetID(), m_Pos, m_Options.Type, m_Options.Subtype);

	CBaseEntity::Snap(SnappingClient);
}