/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "snapfull.h"

#include <game/server/gamecontext.h>

CSnapFull::CSnapFull(CGameWorld *pGameWorld, vec2 Pos, int SnapID, int ClientID, int Num, int Type, bool Changing, bool Projectile)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_SNAPEFFECT, Pos)
{
	m_Pos = Pos;
	m_ClientID = ClientID;
	m_LoadingTick = Server()->TickSpeed();
	GameWorld()->InsertEntity(this);

	m_SnapItem.clear();
	AddItem(Num, Type, Projectile, Changing, SnapID);
}

CSnapFull::~CSnapFull()
{
	for(const auto& pItems : m_SnapItem)
		Server()->SnapFreeID(pItems.m_ID);

	m_SnapItem.clear();
}

void CSnapFull::AddItem(int Value, int Type, bool Projectile, bool Dynamic, int SnapID)
{
	for(int i = 0; i < Value; i++)
	{
		SnapItem Item;
		Item.m_ID = Server()->SnapNewID();
		Item.m_Type = Type;
		Item.m_Changing = Dynamic;
		Item.m_SnapID = SnapID;
		Item.m_Projectile = Projectile;
		m_SnapItem.push_back(Item);
	}
}

void CSnapFull::RemoveItem(int Value, int SnapID, bool Effect)
{
	for (auto pItems = m_SnapItem.begin(); pItems != m_SnapItem.end(); )
	{
		if(Value <= 0)
			break;

		if(pItems->m_SnapID != SnapID)
		{
			++pItems;
			continue;
		}

		if(Effect)
		{
			CPlayer* pOwner = GS()->m_apPlayers[m_ClientID];
			if(pOwner)
				GS()->CreateDeath(m_Pos, m_ClientID);
		}
		Server()->SnapFreeID(pItems->m_ID);
		pItems = m_SnapItem.erase(pItems);
		Value--;
	}
}

void CSnapFull::Tick()
{
	CPlayer *pOwner = GS()->m_apPlayers[m_ClientID];
	if(!pOwner || !pOwner->GetCharacter())
	{
		GS()->m_World.DestroyEntity(this);
		return;
	}

	if(!m_IsBack)
	{
		m_LoadingTick--;
		if(m_LoadingTick <= 1)
			m_IsBack = true;
	}
	else
	{
		m_LoadingTick++;
		if(m_LoadingTick >= 30)
			m_IsBack = false;
	}
	m_Pos = pOwner->GetCharacter()->m_Core.m_Pos;
}

void CSnapFull::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	// skip non interactive bot
	CPlayer* pOwner = GS()->m_apPlayers[m_ClientID];
	if(pOwner->IsVisibleForClient(SnappingClient) != 2)
		return;

	const float AngleStart = (2.0f * pi * Server()->Tick()/static_cast<float>(Server()->TickSpeed()))/3.0f;
	const float AngleStep = 2.0f * pi / m_SnapItem.size();

	int idsize = 0;
	for(const auto &pItems : m_SnapItem)
	{
		const float Radius = 48.0f + (pItems.m_Changing ? m_LoadingTick : 0.0f);
		const vec2 PosStart = m_Pos + vec2(Radius * cos(AngleStart + AngleStep*idsize), Radius * sin(AngleStart + AngleStep*idsize));
		idsize++;

		if(pItems.m_Projectile)
		{
			CNetObj_Projectile *pObj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, pItems.m_ID, sizeof(CNetObj_Projectile)));
			if(!pObj)
				continue;

			pObj->m_X = (int)PosStart.x;
			pObj->m_Y = (int)PosStart.y;
			pObj->m_VelX = 0;
			pObj->m_VelY = 0;
			pObj->m_StartTick = Server()->Tick()-1;
			pObj->m_Type = pItems.m_Type;
			continue;
		}

		CNetObj_Pickup *pObj = static_cast<CNetObj_Pickup *>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, pItems.m_ID, sizeof(CNetObj_Pickup)));
		if(!pObj)
			continue;

		pObj->m_X = (int)PosStart.x;
		pObj->m_Y = (int)PosStart.y;
		pObj->m_Type = pItems.m_Type;
	}
}
