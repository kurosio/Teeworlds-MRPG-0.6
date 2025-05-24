/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "laser.h"

#include <game/server/gamecontext.h>
#include <generated/server_data.h>
#include "character.h"

CLaser::CLaser(CGameWorld *pGameWorld, int OwnerCID, int Damage, vec2 Pos, vec2 Direction, float StartEnergy, bool Explosive)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_LASER, Pos)
{
	m_ClientID = OwnerCID;
	m_Energy = StartEnergy;
	m_Direction = Direction;
	m_Explosive = Explosive;
	m_Damage = Damage;
	m_Bounces = 0;
	m_EvalTick = 0;
	GameWorld()->InsertEntity(this);
	DoBounce();
}

bool CLaser::HitCharacter(vec2 From, vec2 To)
{
	vec2 At;
	auto *pOwnerChar = GS()->GetPlayerChar(m_ClientID);
	auto*pHit = GS()->m_World.IntersectCharacter(m_Pos, To, 0.f, At, pOwnerChar);
	if(!pHit || pHit->m_Core.m_CollisionDisabled)
		return false;

	m_PosTo = From;
	m_Pos = At;
	m_Energy = -1;
	if(m_Explosive)
		GS()->CreateExplosion(m_Pos, m_ClientID, WEAPON_LASER, m_Damage);
	else
		pHit->TakeDamage(vec2(0.f, 0.f), m_Damage, m_ClientID, WEAPON_LASER);
	return true;
}

void CLaser::DoBounce()
{
	m_EvalTick = Server()->Tick();

	if(m_Energy < 0 || !GS()->GetPlayerChar(m_ClientID))
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	vec2 To = m_Pos + m_Direction * m_Energy;

	if(GS()->Collision()->IntersectLine(m_Pos, To, 0x0, &To))
	{
		if(!HitCharacter(m_Pos, To))
		{
			// intersected
			m_PosTo = m_Pos;
			m_Pos = To;

			vec2 TempPos = m_Pos;
			vec2 TempDir = m_Direction * 4.0f;

			GS()->Collision()->MovePoint(&TempPos, &TempDir, 1.0f, 0);
			m_Pos = TempPos;
			m_Direction = normalize(TempDir);

			m_Energy -= distance(m_PosTo, m_Pos) + GS()->Tuning()->m_LaserBounceCost;
			m_Bounces++;

			if(m_Bounces > GS()->Tuning()->m_LaserBounceNum)
				m_Energy = -1;

			if(m_Explosive)
				GS()->CreateExplosion(m_Pos, m_ClientID, WEAPON_LASER, 1);

			GS()->CreateSound(m_Pos, SOUND_LASER_BOUNCE);
		}
	}
	else
	{
		if(!HitCharacter(m_Pos, To))
		{
			m_PosTo = m_Pos;
			m_Pos = To;
			m_Energy = -1;
		}
	}
}

void CLaser::Tick()
{
	if ((Server()->Tick() - m_EvalTick) > (Server()->TickSpeed() * GS()->Tuning()->m_LaserBounceDelay) / 1000.0f)
		DoBounce();
}

void CLaser::TickPaused()
{
	++m_EvalTick;
}

void CLaser::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient) && NetworkClipped(SnappingClient, m_PosTo))
		return;

	GS()->SnapLaser(SnappingClient, GetID(), m_Pos, m_PosTo, m_EvalTick);
}
