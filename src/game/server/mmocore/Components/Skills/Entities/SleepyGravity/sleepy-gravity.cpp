/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "sleepy-gravity.h"

#include <game/server/gamecontext.h>

CSleepyGravity::CSleepyGravity(CGameWorld *pGameWorld, CPlayer* pPlayer, int SkillBonus, int PowerLevel, vec2 Pos)
: CEntity(pGameWorld, CGameWorld::ENTYPE_SLEEPYGRAVITY, Pos)
{
	// transmitted arguments
	m_pPlayer = pPlayer;
	m_PowerLevel = PowerLevel;
	m_Radius = min(200 + SkillBonus, 400);
	m_LifeSpan = 10 * Server()->TickSpeed();
	GameWorld()->InsertEntity(this);
	for(int i = 0; i < NUM_IDS; i++)
		m_IDs[i] = Server()->SnapNewID();
}

CSleepyGravity::~CSleepyGravity()
{
	for(int i = 0; i < NUM_IDS; i++)
		Server()->SnapFreeID(m_IDs[i]);
}

void CSleepyGravity::Reset()
{
	if(m_pPlayer && m_pPlayer->GetCharacter())
		GS()->CreateSound(m_Pos, SOUND_GRENADE_EXPLODE);

	GS()->m_World.DestroyEntity(this);
}

void CSleepyGravity::Tick()
{
	if(!m_pPlayer || !m_pPlayer->GetCharacter() || !m_LifeSpan)
	{
		Reset();
		return;
	}

	m_LifeSpan--;
	const int TimeLeft = m_LifeSpan / Server()->TickSpeed();
	if(TimeLeft < 3)
	{
		for(int i = 0; i < CSleepyGravity::NUM_IDS; i++)
		{
			float AngleStep = 2.0f * pi / CSleepyGravity::NUM_IDS;
			vec2 VertexPos = m_Pos + vec2(m_Radius * cos(AngleStep * i), m_Radius * sin(AngleStep * i));
			if(!m_LifeSpan)
				GS()->CreateExplosion(VertexPos, m_pPlayer->GetCID(), WEAPON_GRENADE, m_PowerLevel);
			else if(m_LifeSpan && TimeLeft == i)
			{
				GS()->CreateDamage(m_Pos, m_pPlayer->GetCID(), 1, false, true);
				break;
			}
		}

		if(!m_LifeSpan)
			GS()->CreateExplosion(m_Pos, m_pPlayer->GetCID(), WEAPON_GRENADE, m_PowerLevel);
	}

	for(CCharacter *p = (CCharacter*) GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); p; p = (CCharacter *)p->TypeNext())
	{
		if(!p || distance(m_Pos, p->m_Core.m_Pos) > m_Radius  || m_pPlayer->GetCID() == p->GetPlayer()->GetCID() || !p->IsAllowedPVP(m_pPlayer->GetCID()))
			continue;

		vec2 Dir = normalize(p->m_Core.m_Pos - m_Pos);
		if(distance(m_Pos, p->m_Core.m_Pos) < 24.0f)
			continue;

		p->m_Core.m_Vel -= Dir * (1.50f);
	}
}

void CSleepyGravity::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient))
		return;

	float AngleStep = 2.0f * pi / CSleepyGravity::NUM_IDS;
	for(int i=0; i<CSleepyGravity::NUM_IDS; i++)
	{
		vec2 VertexPos = m_Pos + vec2(m_Radius * cos(AngleStep*i), m_Radius * sin(AngleStep*i));
		CNetObj_Projectile *pObj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_IDs[i], sizeof(CNetObj_Projectile)));
		if(!pObj)
			return;

		pObj->m_X = (int)VertexPos.x;
		pObj->m_Y = (int)VertexPos.y;
		pObj->m_VelX = 0;
		pObj->m_VelY = 0;
		pObj->m_StartTick = Server()->Tick()-1;
		pObj->m_Type = WEAPON_HAMMER;
	}

	CNetObj_Pickup *pObj = static_cast<CNetObj_Pickup *>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, GetID(), sizeof(CNetObj_Pickup)));
	if(!pObj)
		return;

	pObj->m_X = (int)m_Pos.x;
	pObj->m_Y = (int)m_Pos.y;
	pObj->m_Type = 0;
}
