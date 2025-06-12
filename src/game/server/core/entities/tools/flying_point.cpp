/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.          */
#include "flying_point.h"

#include <game/server/gamecontext.h>

CEntityFlyingPoint::CEntityFlyingPoint(CGameWorld* pGameWorld, vec2 Pos, vec2 InitialVel, int ClientID, int FromID)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_TOOLS, Pos)
{
	m_Pos = Pos;
	m_InitialVel = InitialVel;
	m_InitialAmount = 1.0f;
	m_ClientID = ClientID;
	m_FromID = FromID;
	m_Type = WEAPON_HAMMER;
	m_Vel = m_InitialVel;

	GameWorld()->InsertEntity(this);
}

void CEntityFlyingPoint::Tick()
{
	// check valid player
	CPlayer* pPlayer = GS()->GetPlayer(m_ClientID);
	if(!pPlayer || !pPlayer->GetCharacter())
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	// check valid from player if exist
	CPlayer* pFromPlayer = GS()->GetPlayer(m_FromID);
	if(m_FromID != -1 && (!pFromPlayer || !pFromPlayer->GetCharacter()))
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	float Dist = distance(m_Pos, pPlayer->GetCharacter()->m_Core.m_Pos);
	if(Dist < pPlayer->GetCharacter()->ms_PhysSize)
	{
		if(m_pFunctionCollised)
		{
			CPlayer* pFrom = GS()->GetPlayer(m_FromID);
			m_pFunctionCollised(pFrom ? pFrom : pPlayer, pPlayer);
		}
		GameWorld()->DestroyEntity(this);
		return;
	}

	vec2 Dir = normalize(pPlayer->GetCharacter()->m_Core.m_Pos - m_Pos);
	m_Vel = Dir * clamp(Dist, 0.0f, 16.0f) * (1.0f - m_InitialAmount) + m_InitialVel * m_InitialAmount;
	m_Pos += m_Vel;
	m_InitialAmount *= 0.98f;
}

void CEntityFlyingPoint::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	GS()->SnapProjectile(SnappingClient, GetID(), m_Pos, m_Vel, Server()->Tick(), m_Type, m_ClientID);
}