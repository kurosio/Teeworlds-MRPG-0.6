/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "projectile.h"

#include <game/server/gamecontext.h>
#include "character.h"

CProjectile::CProjectile(CGameWorld *pGameWorld, int Type, int Owner, vec2 Pos, vec2 Dir, int Span,
	bool Explosive, float Force, int SoundImpact, vec2 InitDir, int Weapon)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_PROJECTILE, Pos)
{
	m_Type = Type;
	m_Direction = Dir;
	m_LifeSpan = Span;
	m_Owner = Owner;
	m_Force = Force;
	m_SoundImpact = SoundImpact;
	m_Weapon = Weapon;
	m_StartTick = Server()->Tick();
	m_Explosive = Explosive;
	m_InitDir = InitDir;

	GameWorld()->InsertEntity(this);
}

void CProjectile::Reset()
{
	GameWorld()->DestroyEntity(this);
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
	// initialize variables
	CPlayer* pOwner = GS()->GetPlayer(m_Owner);
	const float Pt = (Server()->Tick() - m_StartTick - 1) / (float)Server()->TickSpeed();
	const float Ct = (Server()->Tick() - m_StartTick) / (float)Server()->TickSpeed();
	const vec2 PrevPos = GetPos(Pt);
	vec2 CurPos = GetPos(Ct);
	m_CurrentPos = CurPos;

	// check owner exists
	if (!pOwner || !pOwner->GetCharacter())
	{
		if (m_Explosive)
			GS()->CreateExplosion(CurPos, -1, m_Weapon, 3);

		GameWorld()->DestroyEntity(this);
		return;
	}

	// initialize variables
	bool Collide = GS()->Collision()->IntersectLineWithInvisible(PrevPos, CurPos, &CurPos, 0);
	CCharacter* OwnerChar = GS()->GetPlayerChar(m_Owner);
	CCharacter* TargetChr = GS()->m_World.IntersectCharacter(PrevPos, CurPos, 6.0f, CurPos, OwnerChar);
	m_LifeSpan--;

	// check collide span and target
	if (m_LifeSpan < 0 || GameLayerClipped(CurPos) || Collide || (TargetChr && TargetChr->IsAllowedPVP(OwnerChar->GetPlayer()->GetCID())))
	{
		if(m_LifeSpan >= 0 || m_Weapon == WEAPON_GRENADE)
		{
			GS()->CreateSound(CurPos, m_SoundImpact);
		}

		if(m_Explosive)
		{
			GS()->CreateExplosion(CurPos, m_Owner, m_Weapon, 3);
		}
		else if(TargetChr)
		{
			TargetChr->TakeDamage(m_Direction * maximum(0.001f, m_Force), 0, m_Owner, m_Weapon);
		}

		GameWorld()->DestroyEntity(this);
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

	CNetObj_DDRaceProjectile DDRaceProjectile;
	int SnappingClientVersion = GS()->GetClientVersion(SnappingClient);\

	if(SnappingClientVersion >= VERSION_DDNET_ENTITY_NETOBJS)
	{
		CNetObj_DDNetProjectile* pDDNetProjectile = static_cast<CNetObj_DDNetProjectile*>(Server()->SnapNewItem(NETOBJTYPE_DDNETPROJECTILE, GetID(), sizeof(CNetObj_DDNetProjectile)));
		if(!pDDNetProjectile)
		{
			return;
		}
		FillExtraInfo(pDDNetProjectile);
	}
	else if(SnappingClientVersion >= VERSION_DDNET_ANTIPING_PROJECTILE && FillExtraInfoLegacy(&DDRaceProjectile))
	{
		int Type = SnappingClientVersion < VERSION_DDNET_MSG_LEGACY ? (int)NETOBJTYPE_PROJECTILE : NETOBJTYPE_DDRACEPROJECTILE;
		void* pProj = Server()->SnapNewItem(Type, GetID(), sizeof(DDRaceProjectile));
		if(!pProj)
		{
			return;
		}
		mem_copy(pProj, &DDRaceProjectile, sizeof(DDRaceProjectile));
	}
	else
	{
		CNetObj_Projectile* pProj = Server()->SnapNewItem<CNetObj_Projectile>(GetID());
		if(!pProj)
		{
			return;
		}
		FillInfo(pProj);
	}
}



void CProjectile::FillInfo(CNetObj_Projectile* pProj)
{
	pProj->m_X = (int)m_Pos.x;
	pProj->m_Y = (int)m_Pos.y;
	pProj->m_VelX = (int)(m_Direction.x * 100.0f);
	pProj->m_VelY = (int)(m_Direction.y * 100.0f);
	pProj->m_StartTick = m_StartTick;
	pProj->m_Type = m_Type;
}

void CProjectile::FillExtraInfo(CNetObj_DDNetProjectile* pProj)
{
	int Flags = 0;
	/*if(m_Bouncing & 1)
	{
		Flags |= PROJECTILEFLAG_BOUNCE_HORIZONTAL;
	}
	if(m_Bouncing & 2)
	{
		Flags |= PROJECTILEFLAG_BOUNCE_VERTICAL;
	}*/
	if(m_Explosive)
	{
		Flags |= PROJECTILEFLAG_EXPLOSIVE;
	}
	/*if(m_Freeze)
	{
		Flags |= PROJECTILEFLAG_FREEZE;
	}*/

	if(m_Owner < 0)
	{
		pProj->m_VelX = round_to_int(m_Direction.x * 1e6f);
		pProj->m_VelY = round_to_int(m_Direction.y * 1e6f);
	}
	else
	{
		pProj->m_VelX = round_to_int(m_InitDir.x);
		pProj->m_VelY = round_to_int(m_InitDir.y);
		Flags |= PROJECTILEFLAG_NORMALIZE_VEL;
	}

	pProj->m_X = round_to_int(m_Pos.x * 100.0f);
	pProj->m_Y = round_to_int(m_Pos.y * 100.0f);
	pProj->m_Type = m_Type;
	pProj->m_StartTick = m_StartTick;
	pProj->m_Owner = m_Owner;
	pProj->m_Flags = Flags;
	pProj->m_SwitchNumber = 0;
	pProj->m_TuneZone = 0;
}

bool CProjectile::FillExtraInfoLegacy(CNetObj_DDRaceProjectile* pProj)
{
	const int MaxPos = 0x7fffffff / 100;
	if(absolute((int)m_Pos.y) + 1 >= MaxPos || absolute((int)m_Pos.x) + 1 >= MaxPos)
	{
		//If the modified data would be too large to fit in an integer, send normal data instead
		return false;
	}
	//Send additional/modified info, by modifying the fields of the netobj
	float Angle = -std::atan2(m_Direction.x, m_Direction.y);

	int Data = 0;
	Data |= (absolute(m_Owner) & 255) << 0;
	if(m_Owner < 0)
		Data |= LEGACYPROJECTILEFLAG_NO_OWNER;
	//This bit tells the client to use the extra info
	Data |= LEGACYPROJECTILEFLAG_IS_DDNET;
	// LEGACYPROJECTILEFLAG_BOUNCE_HORIZONTAL, LEGACYPROJECTILEFLAG_BOUNCE_VERTICAL
	Data |= (0 & 3) << 10;
	if(m_Explosive)
		Data |= LEGACYPROJECTILEFLAG_EXPLOSIVE;
	//if(m_Freeze)
	//	Data |= LEGACYPROJECTILEFLAG_FREEZE;

	pProj->m_X = (int)(m_Pos.x * 100.0f);
	pProj->m_Y = (int)(m_Pos.y * 100.0f);
	pProj->m_Angle = (int)(Angle * 1000000.0f);
	pProj->m_Data = Data;
	pProj->m_StartTick = m_StartTick;
	pProj->m_Type = m_Type;
	return true;
}
