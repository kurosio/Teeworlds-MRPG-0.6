/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "entity.h"
#include "gamecontext.h"

CEntity::CEntity(CGameWorld* pGameWorld, int ObjType, vec2 Pos, int ProximityRadius, int ClientID)
{
	m_pGameWorld = pGameWorld;

	m_pPrevTypeEntity = nullptr;
	m_pNextTypeEntity = nullptr;

	m_ID = Server()->SnapNewID();
	m_ObjType = ObjType;

	m_ClientID = ClientID;
	m_ProximityRadius = ProximityRadius;
	m_MarkedForDestroy = false;

	m_Pos = Pos;
	m_PosTo = Pos;
}

CEntity::~CEntity()
{
	GameWorld()->RemoveEntity(this);
	Server()->SnapFreeID(m_ID);
}

int CEntity::NetworkClipped(int SnappingClient, bool FreezeUnsnapped)
{
	return NetworkClipped(SnappingClient, m_Pos, FreezeUnsnapped);
}

int CEntity::NetworkClipped(int SnappingClient, vec2 CheckPos, bool FreezeUnsnapped)
{
	return NetworkClipped(SnappingClient, CheckPos, 0.f, FreezeUnsnapped);
}

int CEntity::NetworkClipped(int SnappingClient, vec2 CheckPos, float Radius, bool FreezeUnsnapped)
{
	if(SnappingClient == -1)
		return NetworkClippedResultImpl<0>(FreezeUnsnapped);

	const CPlayer* pPlayer = GS()->m_apPlayers[SnappingClient];
	const float dx = pPlayer->m_ViewPos.x - CheckPos.x;
	const float dy = pPlayer->m_ViewPos.y - CheckPos.y;
	const float radiusOffset = Radius / 2.f;

	if(absolute(dx) > (1000.0f + radiusOffset) || absolute(dy) > (800.0f + radiusOffset))
		return NetworkClippedResultImpl<1>(FreezeUnsnapped);

	if(distance(pPlayer->m_ViewPos, CheckPos) > (1100.0f + radiusOffset) || !IsClientEntityFullSnapping(SnappingClient))
		return NetworkClippedResultImpl<1>(FreezeUnsnapped);

	return NetworkClippedResultImpl<0>(FreezeUnsnapped);
}

bool CEntity::IsClientEntityFullSnapping(int SnappingClient) const
{
	// Check if the client ID is within the valid range
	if(m_ClientID >= 0 && m_ClientID < MAX_CLIENTS)
	{
		// Get the player object corresponding to the client ID
		CPlayer* pPlayer = GS()->m_apPlayers[m_ClientID];
		if(pPlayer->IsActiveForClient(SnappingClient) != STATE_SNAPPING_FULL)
			return false;
	}

	return true;
}

bool CEntity::GameLayerClipped(vec2 CheckPos) const
{
	const int rx = round_to_int(CheckPos.x) / 32;
	const int ry = round_to_int(CheckPos.y) / 32;
	return (rx < -200 || rx >= GS()->Collision()->GetWidth() + 200)
		|| (ry < -200 || ry >= GS()->Collision()->GetHeight() + 200);
}
