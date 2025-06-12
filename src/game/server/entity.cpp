/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "entity.h"
#include "gamecontext.h"

CEntity::CEntity(CGameWorld* pGameWorld, int ObjType, vec2 Pos, int Radius, int ClientID)
{
	m_pGameWorld = pGameWorld;
	m_pPrevTypeEntity = nullptr;
	m_pNextTypeEntity = nullptr;
	m_ObjType = ObjType;
	m_ClientID = ClientID;
	m_Radius = Radius;
	m_MarkedForDestroy = false;
	m_HasPlayersInView = true;
	m_NextCheckSnappingPriority = ESnappingPriority::High;
	m_ID = Server()->SnapNewID();
	m_Pos = Pos;
	m_PosTo = Pos;
}

CEntity::~CEntity()
{
	Server()->SnapFreeID(m_ID);
	for(auto const& [GroupID, vIds] : m_vGroupIds)
	{
		for(const auto& id : vIds)
			Server()->SnapFreeID(id);
	}
	m_vGroupIds.clear();
	GameWorld()->RemoveEntity(this);
}

int CEntity::NetworkClippedByPriority(int SnappingClient, ESnappingPriority Priority)
{
	m_NextCheckSnappingPriority = Priority;
	int Result = NetworkClipped(SnappingClient, m_Pos);
	m_NextCheckSnappingPriority = ESnappingPriority::High;
	return Result;
}

int CEntity::NetworkClipped(int SnappingClient)
{
	return NetworkClipped(SnappingClient, m_Pos);
}

int CEntity::NetworkClipped(int SnappingClient, vec2 CheckPos)
{
	return NetworkClipped(SnappingClient, CheckPos, 0.0f);
}

int CEntity::NetworkClipped(int SnappingClient, vec2 CheckPos, float Radius)
{
	if(SnappingClient == -1)
	{
		m_HasPlayersInView = true;
		return 0;
	}

	const CPlayer* pPlayer = GS()->GetPlayer(SnappingClient);
	if(!pPlayer)
		return 1;

	const float dx = pPlayer->m_ViewPos.x - CheckPos.x;
	const float dy = pPlayer->m_ViewPos.y - CheckPos.y;
	const float radiusOffset = Radius / 2.f;

	if(std::abs(dx) > (1000.0f + radiusOffset) || std::abs(dy) > (800.0f + radiusOffset))
		return 1;

	if(distance(pPlayer->m_ViewPos, CheckPos) > (1100.0f + radiusOffset) || !IsValidSnappingState(SnappingClient))
		return 1;

	m_HasPlayersInView = true;
	return 0;
}

bool CEntity::IsValidSnappingState(int SnappingClient) const
{
	if(m_ClientID >= 0 && m_ClientID < MAX_CLIENTS)
	{
		auto* pPlayer = GS()->GetPlayer(m_ClientID);
		if(pPlayer && pPlayer->IsActiveForClient(SnappingClient) < m_NextCheckSnappingPriority)
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

CPlayer* CEntity::GetOwner() const
{
	return GS()->GetPlayer(m_ClientID);
}

CCharacter* CEntity::GetOwnerChar() const
{
	return GS()->GetPlayerChar(m_ClientID);
}

void CEntity::AddSnappingGroupIds(int GroupID, int NumIds)
{
	m_vGroupIds[GroupID].resize(NumIds);
	for(auto& id : m_vGroupIds[GroupID])
		id = Server()->SnapNewID();
}

void CEntity::RemoveSnappingGroupIds(int GroupID)
{
	if(m_vGroupIds.contains(GroupID))
	{
		for(const auto& id : m_vGroupIds.at(GroupID))
			Server()->SnapFreeID(id);
		m_vGroupIds.erase(GroupID);
	}
}