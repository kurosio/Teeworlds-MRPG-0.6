/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "multiple_orbite.h"

#include <game/server/gamecontext.h>

CMultipleOrbite::CMultipleOrbite(CGameWorld* pGameWorld, CEntity* pParent)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_SNAPEFFECT, vec2(0, 0), 64.f)
{
	m_pParent = pParent;
	GameWorld()->InsertEntity(this);
}

CMultipleOrbite::~CMultipleOrbite()
{
	for(const auto& pItems : m_Items)
	{
		Server()->SnapFreeID(pItems.m_ID);
	}
	m_Items.clear();
}

void CMultipleOrbite::Add(int Value, int Type, int Subtype)
{
	for(int i = 0; i < Value; i++)
	{
		SnapItem Item;
		Item.m_ID = Server()->SnapNewID();
		Item.m_Type = Type;
		Item.m_Subtype = Subtype;
		m_Items.push_back(Item);
	}
}

void CMultipleOrbite::Tick()
{
	if(!m_pParent || m_pParent->IsMarkedForDestroy())
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	m_Pos = m_pParent->GetPos();
}

vec2 CMultipleOrbite::UtilityOrbitePos(int PosID) const
{
	float AngleStep = 2.0f * pi / (float)m_Items.size();
	float AngleStart = (2.0f * pi * (float)Server()->Tick() / (float)Server()->TickSpeed()) * 0.55f;
	float X = GetProximityRadius() * cos(AngleStart + AngleStep * (float)PosID);
	float Y = GetProximityRadius() * sin(AngleStart + AngleStep * (float)PosID);
	return { X, Y };
}

void CMultipleOrbite::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	int Pos = 0;
	for(const auto& [ID, Type, Subtype] : m_Items)
	{
		const vec2 PosStart = m_Pos + UtilityOrbitePos(Pos);
		CNetObj_Pickup* pObj = static_cast<CNetObj_Pickup*>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, ID, sizeof(CNetObj_Pickup)));
		if(!pObj)
			continue;

		pObj->m_X = (int)PosStart.x;
		pObj->m_Y = (int)PosStart.y;
		pObj->m_Type = Type;
		pObj->m_Subtype = Subtype;
		Pos++;
	}
}