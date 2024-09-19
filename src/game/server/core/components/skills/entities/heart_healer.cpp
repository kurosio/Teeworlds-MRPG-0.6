/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "heart_healer.h"

#include <game/server/entity_manager.h>
#include <game/server/gamecontext.h>

CHeartHealer::CHeartHealer(CGameWorld *pGameWorld, vec2 Pos, CPlayer *pPlayer, int Health, vec2 InitialVel, bool ShowInformation)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_TOOLS, Pos)
{
	// set the values by arguments
	m_Pos = Pos;
	m_InitialVel = InitialVel/2;
	m_pPlayer = pPlayer;
	m_InitialAmount = 1.0f;
	m_Health = Health;
	m_ShowInformation = ShowInformation;

	GameWorld()->InsertEntity(this);
}

void CHeartHealer::Tick()
{
	// check if there is a player or not to use his functions
	if(!m_pPlayer || !m_pPlayer->GetCharacter())
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	// if the distance is greater than 24.0
	CCharacter *pChar = m_pPlayer->GetCharacter();
	float Dist = distance(m_Pos, pChar->m_Core.m_Pos);
	if(Dist > 24.0f)
	{
		vec2 Dir = normalize(pChar->m_Core.m_Pos - m_Pos);
		m_Pos += Dir*clamp(Dist, 0.0f, 16.0f) * (1.0f - m_InitialAmount) + m_InitialVel * m_InitialAmount;

		m_InitialAmount *= 0.98f;
		return;
	}

	// create the effect
	GS()->CreateSound(m_Pos, 15);
	pChar->IncreaseHealth(m_Health);
	if(m_ShowInformation)
	{
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "%dHP", m_Health);
		GS()->EntityManager()->Text(m_Pos + vec2(0, -96), 20, aBuf);
	}

	GameWorld()->DestroyEntity(this);
	return;
}

void CHeartHealer::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient))
		return;

	CNetObj_Pickup *pObj = static_cast<CNetObj_Pickup *>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, GetID(), sizeof(CNetObj_Pickup)));
	if(!pObj)
		return;

	pObj->m_X = (int)m_Pos.x;
	pObj->m_Y = (int)m_Pos.y;
	pObj->m_Type = 0;
	pObj->m_Subtype = 0;
}