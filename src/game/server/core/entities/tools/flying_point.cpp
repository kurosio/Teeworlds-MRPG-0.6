/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "flying_point.h"

#include <game/server/gamecontext.h>

CEntityFlyingPoint::CEntityFlyingPoint(CGameWorld* pGameWorld, vec2 Pos, vec2 InitialVel, int ClientID, int FromID)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_BONUS_DROP, Pos)
{
	m_Pos = Pos;
	m_InitialVel = InitialVel;
	m_InitialAmount = 1.0f;
	m_ClientID = ClientID;
	m_FromID = FromID;
	m_Type = WEAPON_HAMMER;
	GameWorld()->InsertEntity(this);
}

void CEntityFlyingPoint::Tick()
{
	// check valid player
	CPlayer *pPlayer = GS()->GetPlayer(m_ClientID);
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
	m_Pos += Dir*clamp(Dist, 0.0f, 16.0f) * (1.0f - m_InitialAmount) + m_InitialVel * m_InitialAmount;
	m_InitialAmount *= 0.98f;
}

void CEntityFlyingPoint::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	CNetObj_Projectile *pObj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, GetID(), sizeof(CNetObj_Projectile)));
	if(pObj)
	{
		pObj->m_X = (int)m_Pos.x;
		pObj->m_Y = (int)m_Pos.y;
		pObj->m_VelX = 0;
		pObj->m_VelY = 0;
		pObj->m_StartTick = Server()->Tick();
		pObj->m_Type = m_Type;
	}
}
