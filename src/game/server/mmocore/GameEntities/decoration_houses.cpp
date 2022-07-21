#include "decoration_houses.h"

#include <engine/server.h>

CDecorationHouses::CDecorationHouses(CGameWorld* pGameWorld, vec2 Pos, int HouseID, int DecoID)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_DECOHOUSE, Pos)
{
	m_HouseID = HouseID;
	m_DecoID = DecoID;

	GameWorld()->InsertEntity(this);

	if (SwitchToObject(true) >= 0)
	{
		for (int i = 0; i < NUM_IDS; i++)
			m_IDs[i] = Server()->SnapNewID();
	}
}
CDecorationHouses::~CDecorationHouses()
{
	if (SwitchToObject(true) >= 0)
	{
		for (int i = 0; i < NUM_IDS; i++)
			Server()->SnapFreeID(m_IDs[i]);
	}
}

int CDecorationHouses::SwitchToObject(bool Data) const
{
	switch (m_DecoID)
	{
	case itDecoHealth: return (Data ? -1 : (int)PICKUP_HEALTH);
	case itDecoArmor: return (Data ? -1 : (int)PICKUP_ARMOR);
	case itEliteDecoHealth: return (Data ? (int)WEAPON_SHOTGUN : (int)PICKUP_HEALTH);
	case itEliteDecoNinja: return (Data ? -1 : (int)PICKUP_NINJA);
	default: break;
	}
	return -1;
}

void CDecorationHouses::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient))
		return;

	if (SwitchToObject(true) <= -1)
	{
		CNetObj_Pickup* pP = static_cast<CNetObj_Pickup*>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, GetID(), sizeof(CNetObj_Pickup)));
		if (!pP)
			return;

		pP->m_X = (int)m_Pos.x;
		pP->m_Y = (int)m_Pos.y;
		pP->m_Type = SwitchToObject(false);
		return;
	}

	float AngleStart = (2.0f * pi * (float)Server()->Tick() / (float)Server()->TickSpeed()) / 10.0f;
	float AngleStep = 2.0f * pi / BODY;
	float Radius = 30.0f;
	for (int i = 0; i < BODY; i++)
	{
		vec2 PosStart = m_Pos + vec2(Radius * cos(AngleStart + AngleStep * i), Radius * sin(AngleStart + AngleStep * i));
		CNetObj_Projectile* pObj = static_cast<CNetObj_Projectile*>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_IDs[i], sizeof(CNetObj_Projectile)));
		if (!pObj)
			return;

		pObj->m_X = (int)PosStart.x;
		pObj->m_Y = (int)PosStart.y;
		pObj->m_VelX = 0;
		pObj->m_VelY = 0;
		pObj->m_StartTick = Server()->Tick() - 1;
		pObj->m_Type = SwitchToObject(true);
	}


	CNetObj_Pickup* pP = static_cast<CNetObj_Pickup*>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, m_IDs[BODY], sizeof(CNetObj_Pickup)));
	if (!pP)
		return;

	pP->m_X = (int)m_Pos.x;
	pP->m_Y = (int)m_Pos.y;
	pP->m_Type = SwitchToObject(false);
}