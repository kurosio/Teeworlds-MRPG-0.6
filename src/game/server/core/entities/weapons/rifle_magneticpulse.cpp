#include "rifle_magneticpulse.h"
#include <game/server/gamecontext.h>

CEntityRifleMagneticPulse::CEntityRifleMagneticPulse(CGameWorld* pGameWorld, int OwnerCID, float Radius, vec2 Pos, vec2 Direction)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_LASER, Pos, 24)
{
	m_ClientID = OwnerCID;
	m_Direction = Direction;
	m_LifeTick = Server()->TickSpeed() * 2;
	m_Radius = 0.f;
	m_RadiusAnimation.Init(0.f, Radius, Server()->TickSpeed() / 5, [Radius](float t)
	{
		return 0.f + t * (Radius - 0.0f);
	});

	GS()->CreateSound(m_Pos, SOUND_WEAPONS_MAGNET_IMPULSE_START);
	GameWorld()->InsertEntity(this);
}

void CEntityRifleMagneticPulse::Tick()
{
	const auto* pOwner = GetOwner();
	if(!pOwner || !pOwner->GetCharacter())
	{
		MarkForDestroy();
		return;
	}

	// select phase by state
	if(!m_LastPhase)
		TickFirstPhase();
	else
		TickLastPhase();
}

void CEntityRifleMagneticPulse::RunLastPhase()
{
	if(!m_LastPhase)
	{
		m_LastPhase = true;
		AddSnappingGroupIds(GROUP_CYRCLE, NUM_IDS_CYRCLE);
		AddSnappingGroupIds(GROUP_PROJECTILE_CYRCLE, NUM_IDS_PROJECTILE_CYRCLE);
	}
}

void CEntityRifleMagneticPulse::TickFirstPhase()
{
	const auto NormalizedDirection = normalize(m_Direction);
	const auto OldPos = m_Pos;
	m_Pos += NormalizedDirection * 10.f;
	m_PosTo = OldPos;

	// by collide
	if(GS()->Collision()->CheckPoint(m_Pos))
	{
		RunLastPhase();
		return;
	}

	// by players
	for(auto* pChar = (CCharacter*)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
	{
		if(m_ClientID == pChar->GetPlayer()->GetCID() || !pChar->IsAllowedPVP(m_ClientID))
			continue;

		const auto Distance = distance(m_Pos, pChar->m_Core.m_Pos);
		if(Distance <= 64.f)
		{
			RunLastPhase();
			return;
		}
	}
}

void CEntityRifleMagneticPulse::TickLastPhase()
{
	// decrease life tick
	if(--m_LifeTick <= 0)
	{
		MarkForDestroy();
		return;
	}

	// animation
	if(m_LifeTick <= m_RadiusAnimation.GetDurationTicks())
	{
		GS()->CreateSound(m_Pos, SOUND_WEAPONS_MAGNET_IMPULSE_END);
		m_RadiusAnimation.Reverse(Server()->Tick());
	}
	else if(!m_RadiusAnimation.IsStarted())
	{
		GS()->CreateSound(m_Pos, SOUND_WEAPONS_MAGNET_IMPULSE_END);
		m_RadiusAnimation.Start(Server()->Tick());
	}

	m_Radius = m_RadiusAnimation.GetCurrentValue(Server()->Tick());

	// magnetic pulse
	for(auto* pChar = (CCharacter*)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
	{
		const auto Dist = distance(m_Pos, pChar->m_Core.m_Pos);
		if(Dist > m_Radius || Dist < 24.0f || !pChar->IsAllowedPVP(m_ClientID))
			continue;

		const auto Dir = normalize(pChar->m_Core.m_Pos - m_Pos);
		pChar->m_Core.m_Vel -= Dir * 5.f;
	}
}

void CEntityRifleMagneticPulse::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	if(m_LastPhase)
	{
		// snap inside projectiles cyrcle
		for(const auto& id : GetSnappingGroupIds(GROUP_PROJECTILE_CYRCLE))
		{
			const auto RangeRandomPos = random_range_pos(m_Pos, m_Radius);
			GS()->SnapProjectile(SnappingClient, id, RangeRandomPos, {}, Server()->Tick(), WEAPON_HAMMER, m_ClientID);
		}

		// snap cyrcle laser
		const auto& groupCyrcleIds = GetSnappingGroupIds(GROUP_CYRCLE);
		const auto AngleStep = 2.0f * pi / static_cast<float>(groupCyrcleIds.size());
		for(size_t i = 0; i < groupCyrcleIds.size(); ++i)
		{
			const auto nextIndex = (i + 1) % groupCyrcleIds.size();
			const auto CurrentPos = m_Pos + vec2(m_Radius * cos(AngleStep * i), m_Radius * sin(AngleStep * i));
			const auto NextPos = m_Pos + vec2(m_Radius * cos(AngleStep * nextIndex), m_Radius * sin(AngleStep * nextIndex));
			GS()->SnapLaser(SnappingClient, groupCyrcleIds[i], CurrentPos, NextPos, Server()->Tick() - 1);
		}
	}

	// snap default laser
	GS()->SnapLaser(SnappingClient, GetID(), m_Pos, m_PosTo, Server()->Tick() - 1);
}
