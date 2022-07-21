/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "healer-health.h"

#include "hearth.h"
#include <game/server/gamecontext.h>

CHealthHealer::CHealthHealer(CGameWorld *pGameWorld, CPlayer* pPlayer, int SkillBonus, int PowerLevel, vec2 Pos)
: CEntity(pGameWorld, CGameWorld::ENTYPE_SKILLTURRETHEART, Pos)
{
	m_Pos = Pos;
	m_pPlayer = pPlayer;
	m_PowerLevel = PowerLevel;

	m_LifeSpan = (10 + SkillBonus)*Server()->TickSpeed();
	m_ReloadTick = 2*Server()->TickSpeed();

	GameWorld()->InsertEntity(this);

	for(int i=0; i<NUM_IDS; i++)
		m_IDs[i] = Server()->SnapNewID();
}

CHealthHealer::~CHealthHealer()
{
	for(int i = 0; i < NUM_IDS; i++)
		Server()->SnapFreeID(m_IDs[i]);
}

void CHealthHealer::Reset()
{
	// if the player is eating, we create effects
	if(m_pPlayer && m_pPlayer->GetCharacter())
		GS()->CreateSound(m_Pos, SOUND_GRENADE_EXPLODE);

	GS()->m_World.DestroyEntity(this);
	return;
}

void CHealthHealer::Tick()
{
	m_LifeSpan--;
	if(!m_pPlayer || !m_pPlayer->GetCharacter() || !m_LifeSpan)
	{
		Reset();
		return;
	}

	if(m_ReloadTick)
	{
		m_ReloadTick--;
		return;
	}

	bool ShowHealthRestore = false;
	const int HealthRestore = m_PowerLevel;
	for(CCharacter *p = (CCharacter*) GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); p; p = (CCharacter *)p->TypeNext())
	{
		if(p && distance(m_Pos, p->m_Core.m_Pos) < 620.0f && (m_pPlayer->GetCID() == p->GetPlayer()->GetCID() || !p->IsAllowedPVP(m_pPlayer->GetCID())))
		{
			ShowHealthRestore = true;
			new CHearth(&GS()->m_World, m_Pos, p->GetPlayer(), HealthRestore, p->m_Core.m_Vel);
		}
	}

	m_ReloadTick = 2 * Server()->TickSpeed();

	if(ShowHealthRestore)
	{
		char aBuf[16];
		str_format(aBuf, sizeof(aBuf), "%dHP", HealthRestore);
		GS()->CreateText(NULL, false, vec2(m_Pos.x, m_Pos.y - 96.0f), vec2(0, 0), 40, aBuf);
	}
}

void CHealthHealer::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient))
		return;

	float AngleStep = 2.0f * pi / CHealthHealer::NUM_IDS;
	float Radius = clamp(0.0f+(int)m_ReloadTick, 0.0f, 32.0f);
	for(int i=0; i<CHealthHealer::NUM_IDS; i++)
	{
		vec2 VertexPos = m_Pos + vec2(Radius * cos(AngleStep*i), Radius * sin(AngleStep*i));
		CNetObj_Pickup *pObj = static_cast<CNetObj_Pickup *>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, m_IDs[i], sizeof(CNetObj_Pickup)));
		if(!pObj)
			return;

		pObj->m_X = (int)VertexPos.x;
		pObj->m_Y = (int)VertexPos.y;
		pObj->m_Type = PICKUP_HEALTH;
	}

	CNetObj_Pickup *pObj = static_cast<CNetObj_Pickup *>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, GetID(), sizeof(CNetObj_Pickup)));
	if(!pObj)
		return;

	pObj->m_X = (int)m_Pos.x;
	pObj->m_Y = (int)m_Pos.y;
	pObj->m_Type = PICKUP_ARMOR;
}
