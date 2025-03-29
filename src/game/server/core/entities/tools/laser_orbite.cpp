/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "laser_orbite.h"

#include <game/server/gamecontext.h>

CEntityLaserOrbite::CEntityLaserOrbite(CGameWorld* pGameWorld, int ClientID, CEntity* pEntParent, int Amount, LaserOrbiteType Type, float Speed, float Radius, int LaserType, int64_t Mask)
	: CEntity(pGameWorld, CGameWorld::ENTYPE_LASER_ORBITE, vec2(0.f, 0.f), (int)Radius)
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
		m_Pos = pEntParent->GetPos() + (Type == LaserOrbiteType::InsideOrbiteRandom ? m_AppendPos : vec2(0.f, 0.f));
	}
	GameWorld()->InsertEntity(this);

	m_IDs.set_size(Amount);
	for(int i = 0; i < m_IDs.size(); i++)
		m_IDs[i] = Server()->SnapNewID();
}

CEntityLaserOrbite::~CEntityLaserOrbite()
{
	for(int i = 0; i < m_IDs.size(); i++)
		Server()->SnapFreeID(m_IDs[i]);
	m_IDs.clear();
}

void CEntityLaserOrbite::Tick()
{
	auto* pPlayer = GetOwner();
	if((m_ClientID >= 0 && !pPlayer) || (m_pEntParent && !GameWorld()->ExistEntity(m_pEntParent)))
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	if(m_pEntParent)
	{
		if(m_Type == LaserOrbiteType::InsideOrbiteRandom)
			m_Pos = m_pEntParent->GetPos() + m_AppendPos;
		else
			m_Pos = m_pEntParent->GetPos();
	}
	else if(m_ClientID >= 0 && pPlayer)
	{
		m_Pos = pPlayer->GetCharacter()->m_Core.m_Pos;
	}
}

vec2 CEntityLaserOrbite::UtilityOrbitePos(int PosID) const
{
	float AngleStart = 2.0f * pi;
	float AngleStep = 2.0f * pi / (float)m_IDs.size();
	if(m_Type == LaserOrbiteType::MoveLeft)
		AngleStart = -(AngleStart * (float)Server()->Tick() / (float)Server()->TickSpeed()) * m_MoveSpeed;
	else if(m_Type == LaserOrbiteType::MoveRight)
		AngleStart = (AngleStart * (float)Server()->Tick() / (float)Server()->TickSpeed()) * m_MoveSpeed;

	float DynamicRadius = m_Radius * (1.0f + 0.05f * sin((float)Server()->Tick() / (float)Server()->TickSpeed()));
	float X = DynamicRadius * cos(AngleStart + AngleStep * (float)PosID);
	float Y = DynamicRadius * sin(AngleStart + AngleStep * (float)PosID);
	return { X, Y };
}

void CEntityLaserOrbite::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient, m_Pos, m_Radius + 1000.f) || !CmaskIsSet(m_Mask, SnappingClient))
		return;

	vec2 LastPosition = m_Pos + UtilityOrbitePos(m_IDs.size() - 1);
	for(int i = 0; i < m_IDs.size(); i++)
	{
		vec2 PosStart = m_Pos + UtilityOrbitePos(i);

		GS()->SnapLaser(SnappingClient, m_IDs[i], LastPosition, PosStart, Server()->Tick() - 4, m_LaserType, 0, m_ClientID);
		LastPosition = PosStart;
	}
}

void CEntityLaserOrbite::AddClientMask(int ClientID)
{
		m_Mask |= CmaskOne(ClientID);
}

void CEntityLaserOrbite::RemoveClientMask(int ClientID)
{
		m_Mask &= ~CmaskOne(ClientID);
}


