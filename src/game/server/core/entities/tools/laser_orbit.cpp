/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "laser_orbit.h"

#include <game/server/gamecontext.h>

CEntityLaserOrbit::CEntityLaserOrbit(CGameWorld* pGameWorld, int ClientID, CEntity* pEntParent, int Amount, LaserOrbitType Type, float Speed, float Radius, int LaserType, int64_t Mask)
	: CEntity(pGameWorld, CGameWorld::ENTYPE_LASER_ORBIT, vec2(0.f, 0.f), (int)Radius)
{
	m_Type = Type;
	m_ClientID = ClientID;
	m_pEntParent = pEntParent;
	m_MoveSpeed = Speed;
	m_Mask = Mask;
	m_LaserType = LaserType;

	if(m_pEntParent)
	{
		m_AppendPos = vec2(centrelized_frandom(0.f, m_Radius / 1.5f), centrelized_frandom(0.f, m_Radius / 1.5f));
		m_Pos = pEntParent->GetPos() + (Type == LaserOrbitType::InsideOrbitRandom ? m_AppendPos : vec2(0.f, 0.f));
	}
	GameWorld()->InsertEntity(this);

	m_IDs.set_size(Amount);
	for(int i = 0; i < m_IDs.size(); i++)
		m_IDs[i] = Server()->SnapNewID();
}

CEntityLaserOrbit::~CEntityLaserOrbit()
{
	for(int i = 0; i < m_IDs.size(); i++)
		Server()->SnapFreeID(m_IDs[i]);
	m_IDs.clear();
}

void CEntityLaserOrbit::Tick()
{
	auto* pPlayer = GetOwner();
	if((m_ClientID >= 0 && !pPlayer) || (m_pEntParent && !GameWorld()->ExistEntity(m_pEntParent)))
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	if(m_pEntParent)
	{
		if(m_Type == LaserOrbitType::InsideOrbitRandom)
			m_Pos = m_pEntParent->GetPos() + m_AppendPos;
		else
			m_Pos = m_pEntParent->GetPos();
	}
	else if(m_ClientID >= 0 && pPlayer)
	{
		m_Pos = pPlayer->GetCharacter()->m_Core.m_Pos;
	}
}

vec2 CEntityLaserOrbit::UtilityOrbitPos(int PosID) const
{
	float AngleStart = 2.0f * pi;
	float AngleStep = 2.0f * pi / (float)m_IDs.size();
	if(m_Type == LaserOrbitType::MoveLeft)
		AngleStart = -(AngleStart * (float)Server()->Tick() / (float)Server()->TickSpeed()) * m_MoveSpeed;
	else if(m_Type == LaserOrbitType::MoveRight)
		AngleStart = (AngleStart * (float)Server()->Tick() / (float)Server()->TickSpeed()) * m_MoveSpeed;

	float DynamicRadius = m_Radius * (1.0f + 0.05f * sin((float)Server()->Tick() / (float)Server()->TickSpeed()));
	float X = DynamicRadius * cos(AngleStart + AngleStep * (float)PosID);
	float Y = DynamicRadius * sin(AngleStart + AngleStep * (float)PosID);
	return { X, Y };
}

void CEntityLaserOrbit::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient, m_Pos, m_Radius + 1000.f) || !CmaskIsSet(m_Mask, SnappingClient))
		return;

	vec2 LastPosition = m_Pos + UtilityOrbitPos(m_IDs.size() - 1);
	for(int i = 0; i < m_IDs.size(); i++)
	{
		vec2 PosStart = m_Pos + UtilityOrbitPos(i);

		GS()->SnapLaser(SnappingClient, m_IDs[i], LastPosition, PosStart, Server()->Tick() - 4, m_LaserType, 0, m_ClientID);
		LastPosition = PosStart;
	}
}

void CEntityLaserOrbit::AddClientMask(int ClientID)
{
		m_Mask |= CmaskOne(ClientID);
}

void CEntityLaserOrbit::RemoveClientMask(int ClientID)
{
		m_Mask &= ~CmaskOne(ClientID);
}


