#include "rifle_wallpusher.h"

#include <game/server/gamecontext.h>

CEntityRifleWallPusher::CEntityRifleWallPusher(CGameWorld* pGameWorld, int OwnerCID, vec2 Pos, vec2 Direction, int LifeTick)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_LASER, Pos, 24)
{
	m_ClientID = OwnerCID;
	m_Direction = Direction;
	m_LifeTick = LifeTick;
	m_ID2 = Server()->SnapNewID();

	GS()->CreateSound(Pos, SOUND_WEAPONS_WALL_PUSHER_START);
	GameWorld()->InsertEntity(this);
}

CEntityRifleWallPusher::~CEntityRifleWallPusher()
{
	Server()->SnapFreeID(m_ID2);
}

void CEntityRifleWallPusher::Tick()
{
	const auto* pOwner = GetOwner();
	if(!pOwner || !pOwner->GetCharacter())
	{
		MarkForDestroy();
		return;
	}

	// sound
	if(Server()->Tick() % Server()->TickSpeed() * GetSoundInterval(SOUND_WEAPONS_WALL_PUSHER_BULLET) == 0)
		GS()->CreateSound(m_Pos, SOUND_WEAPONS_WALL_PUSHER_BULLET);

	// check lifetime
	m_LifeTick--;
	if(!m_LifeTick)
	{
		MarkForDestroy();
		return;
	}

	// check collide
	if(GS()->Collision()->CheckPoint(m_Pos) && GS()->Collision()->CheckPoint(m_PosTo))
	{
		GS()->CreateDeath(m_Pos, m_ClientID);
		GS()->CreateDeath(m_PosTo, m_ClientID);
		MarkForDestroy();
		return;
	}

	// initialize variables
	const auto NormalizedDirection = normalize(m_Direction);
	const auto PrevPos = m_Pos;
	const auto DistanceBetwenPoint = distance(m_Pos, m_PosTo);

	// increase length & move
	if(DistanceBetwenPoint < 240.f)
	{
		const auto PerpendicularDirection = vec2(-NormalizedDirection.y, NormalizedDirection.x);
		m_Pos += PerpendicularDirection * 3.0f;
		m_PosTo -= PerpendicularDirection * 3.0f;
	}

	// update position
	m_Pos += NormalizedDirection * 10.f;
	m_PosTo += NormalizedDirection * 10.f;

	// check hitting characters
	CheckHitCharacter(PrevPos);
}

void CEntityRifleWallPusher::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	GS()->SnapLaser(SnappingClient, GetID(), m_Pos, m_PosTo, Server()->Tick() - 3);
	GS()->SnapLaser(SnappingClient, m_ID2, m_PosTo, m_Pos, Server()->Tick() - 3);
}

void CEntityRifleWallPusher::CheckHitCharacter(const vec2 PrevPos) const
{
	// hit character
	for(auto* pChar = (CCharacter*)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
	{
		if(m_ClientID == pChar->GetPlayer()->GetCID() || !pChar->IsAllowedPVP(m_ClientID))
			continue;

		// check intersect line
		vec2 IntersectPos;
		if(!closest_point_on_line(m_Pos, m_PosTo, pChar->m_Core.m_Pos, IntersectPos))
			continue;

		// check distance
		const float Distance = distance(IntersectPos, pChar->m_Core.m_Pos);
		if(Distance > g_Config.m_SvDoorRadiusHit * 3)
			continue;

		// wall pushing
		const auto WallMovement = m_Pos - PrevPos;
		const auto PredictedPos = pChar->m_Core.m_Pos + WallMovement;
		const auto WallCollide = GS()->Collision()->TestBox(PredictedPos, vec2(CCharacter::ms_PhysSize, CCharacter::ms_PhysSize));
		if(WallCollide)
		{
			pChar->TakeDamage({}, 1, m_ClientID, WEAPON_LASER);
		}
		else
		{
			pChar->m_Core.m_Pos = PredictedPos;
			pChar->m_Core.m_Vel = {};
		}
	}
}
