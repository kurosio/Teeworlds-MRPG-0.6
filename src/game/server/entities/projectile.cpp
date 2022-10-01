/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "projectile.h"

#include <game/server/gamecontext.h>
#include "character.h"

CProjectile::CProjectile(CGameWorld *pGameWorld, int Type, int Owner, vec2 Pos, vec2 Dir, int Span,
		int Damage, bool Explosive, float Force, int SoundImpact, int Weapon)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_PROJECTILE, Pos)
{
	m_Type = Type;
	m_Direction = Dir;
	m_LifeSpan = Span;
	m_Owner = Owner;
	m_Force = Force;
	m_Damage = Damage;
	m_SoundImpact = SoundImpact;
	m_Weapon = Weapon;
	m_StartTick = Server()->Tick();
	m_Explosive = Explosive;

	GameWorld()->InsertEntity(this);
}

void CProjectile::Reset()
{
	GS()->m_World.DestroyEntity(this);
}

vec2 CProjectile::GetPos(float Time)
{
	float Curvature = 0;
	float Speed = 0;

switch (m_Type)
{
case WEAPON_GRENADE:
	Curvature = GS()->Tuning()->m_GrenadeCurvature;
	Speed = GS()->Tuning()->m_GrenadeSpeed;
	break;

case WEAPON_SHOTGUN:
	Curvature = GS()->Tuning()->m_ShotgunCurvature;
	Speed = GS()->Tuning()->m_ShotgunSpeed;
	break;

case WEAPON_GUN:
	Curvature = GS()->Tuning()->m_GunCurvature;
	Speed = GS()->Tuning()->m_GunSpeed;
	break;
}

return CalcPos(m_Pos, m_Direction, Curvature, Speed, Time);
}


void CProjectile::Tick()
{
	const float Pt = (Server()->Tick() - m_StartTick - 1) / (float)Server()->TickSpeed();
	const float Ct = (Server()->Tick() - m_StartTick) / (float)Server()->TickSpeed();
	const vec2 PrevPos = GetPos(Pt);
	vec2 CurPos = GetPos(Ct);
	if (!GS()->m_apPlayers[m_Owner] || !GS()->m_apPlayers[m_Owner]->GetCharacter())
	{
		if (m_Explosive)
			GS()->CreateExplosion(CurPos, -1, m_Weapon, m_Damage);

		GS()->m_World.DestroyEntity(this);
		return;
	}
	bool Collide = GS()->Collision()->IntersectLineWithInvisible(PrevPos, CurPos, &CurPos, 0);
	CCharacter* OwnerChar = GS()->GetPlayerChar(m_Owner);
	CCharacter* TargetChr = GS()->m_World.IntersectCharacter(PrevPos, CurPos, 6.0f, CurPos, OwnerChar);

	m_LifeSpan--;

	if (m_LifeSpan < 0 || GameLayerClipped(CurPos) || Collide || (TargetChr && !TargetChr->m_Core.m_CollisionDisabled))
	{

		if (m_LifeSpan >= 0 || m_Weapon == WEAPON_GRENADE)
			GS()->CreateSound(CurPos, m_SoundImpact);

		if (m_Explosive)
			GS()->CreateExplosion(CurPos, m_Owner, m_Weapon, m_Damage);

		else if (TargetChr)
			TargetChr->TakeDamage(m_Direction * max(0.001f, m_Force), m_Damage, m_Owner, m_Weapon);

		GS()->m_World.DestroyEntity(this);
	}
}

void CProjectile::TickPaused()
{
	++m_StartTick;
}

void CProjectile::Snap(int SnappingClient)
{
	const float Ct = (Server()->Tick() - m_StartTick) / (float)Server()->TickSpeed();
	if (NetworkClipped(SnappingClient, GetPos(Ct)))
		return;

	CNetObj_Projectile* pProj = static_cast<CNetObj_Projectile*>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, GetID(), sizeof(CNetObj_Projectile)));
	if (pProj)
	{
		pProj->m_X = (int)m_Pos.x;
		pProj->m_Y = (int)m_Pos.y;
		pProj->m_VelX = (int)(m_Direction.x * 100.0f);
		pProj->m_VelY = (int)(m_Direction.y * 100.0f);
		pProj->m_StartTick = m_StartTick;
		pProj->m_Type = m_Type;
	}
}
