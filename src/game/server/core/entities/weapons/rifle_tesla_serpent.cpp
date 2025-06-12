#include "rifle_tesla_serpent.h"
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/entities/character.h>

enum
{
	MAX_CHAIN_SEGMENTS = 3,
	NUM_SUB_SEGMENTS_PER_BOLT = 3,
};

constexpr float JITTER_MAGNITUDE = 64.0f;

CEntityTeslaSerpent::CEntityTeslaSerpent(CGameWorld* pGameWorld, int OwnerCID, vec2 Pos, vec2 Direction, float Damage, float ChainRange, int MaxTargets, float DamageFalloff)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_LASER, Pos, 0, OwnerCID), m_Damage(Damage), m_ChainRange(ChainRange)
	, m_MaxTargetsInChain(clamp(MaxTargets, 1, MAX_CHAIN_SEGMENTS + 1)), m_DamageFalloff(DamageFalloff)
{
	m_InitialDir = normalize(Direction);
	m_LifeSpanTicks = Server()->TickSpeed() / 4;
	m_ChainCalculationDone = false;

	for(int i = 0; i < MAX_CHAIN_SEGMENTS; i++)
		AddSnappingGroupIds(i, NUM_SUB_SEGMENTS_PER_BOLT);
	GameWorld()->InsertEntity(this);
	m_vChainSegmentEndPoints.push_back(m_Pos);

	// append damage by 25% from item
	auto* pOwner = GetOwner();
	if(pOwner && pOwner->GetItem(itTeslaInductiveCoil)->IsEquipped())
		m_Damage += maximum(1.0f, translate_to_percent_rest(m_Damage, 25.f));
}

void CEntityTeslaSerpent::Tick()
{
	auto* pOwnerChar = GetOwnerChar();
	if(!pOwnerChar)
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	if(!m_ChainCalculationDone)
	{
		CalculateChainLightning();
		m_ChainCalculationDone = true;
	}

	m_LifeSpanTicks--;
	if(m_LifeSpanTicks < 0)
	{
		GameWorld()->DestroyEntity(this);
	}
}

void CEntityTeslaSerpent::CalculateChainLightning()
{
	vec2 CurrentChainSourcePos = m_Pos;
	vec2 CurrentDirection = m_InitialDir;
	float CurrentSegmentDamage = m_Damage;

	for(int TargetsHitCount = 0; TargetsHitCount < m_MaxTargetsInChain; ++TargetsHitCount)
	{
		CCharacter* pClosestHitCharacter = nullptr;
		vec2 HitPosition = CurrentChainSourcePos + CurrentDirection * m_ChainRange;
		float MinHitDistance = m_ChainRange;

		vec2 WallCollisionPoint;
		if(GS()->Collision()->IntersectLine(CurrentChainSourcePos, HitPosition, &WallCollisionPoint, nullptr))
		{
			HitPosition = WallCollisionPoint;
			MinHitDistance = distance(CurrentChainSourcePos, HitPosition);
		}

		for(auto* pTargetChar = (CCharacter*)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pTargetChar; pTargetChar = (CCharacter*)pTargetChar->TypeNext())
		{
			if(pTargetChar->GetPlayer()->GetCID() == m_ClientID || !pTargetChar->IsAllowedPVP(m_ClientID))
				continue;

			bool bAlreadyHit = false;
			for(size_t i = 0; i < m_vTargetsHitThisShot.size(); ++i)
			{
				if(m_vTargetsHitThisShot[i] == pTargetChar->GetPlayer()->GetCID())
				{
					bAlreadyHit = true;
					break;
				}
			}

			if(bAlreadyHit)
				continue;

			vec2 TargetCharPos = pTargetChar->m_Core.m_Pos;
			float DistToTarget = distance(CurrentChainSourcePos, TargetCharPos);

			if(DistToTarget < MinHitDistance)
			{
				if(!GS()->Collision()->IntersectLine(CurrentChainSourcePos, TargetCharPos, nullptr, nullptr))
				{
					MinHitDistance = DistToTarget;
					HitPosition = TargetCharPos;
					pClosestHitCharacter = pTargetChar;
				}
			}
		}

		m_vChainSegmentEndPoints.push_back(HitPosition);

		if(pClosestHitCharacter)
		{
			GS()->CreateExplosion(HitPosition, m_ClientID, WEAPON_LASER, (int)CurrentSegmentDamage, FORCE_FLAG_CANT_SELF);
			m_vTargetsHitThisShot.push_back(pClosestHitCharacter->GetPlayer()->GetCID());
			GS()->CreateSound(HitPosition, SOUND_GRENADE_EXPLODE);
			GS()->CreateSound(HitPosition, SOUND_HOOK_LOOP);

			CurrentChainSourcePos = HitPosition;
			CurrentSegmentDamage *= m_DamageFalloff;
			CurrentDirection = vec2(0, 0);
		}
		else
		{
			GS()->CreateExplosion(HitPosition, m_ClientID, WEAPON_LASER, (int)CurrentSegmentDamage, FORCE_FLAG_CANT_SELF);
			GS()->CreateSound(HitPosition, SOUND_GRENADE_EXPLODE);
			GS()->CreateSound(HitPosition, SOUND_HOOK_LOOP);
			break;
		}

		if(m_vChainSegmentEndPoints.size() >= MAX_CHAIN_SEGMENTS + 1)
			break;
	}
}

void CEntityTeslaSerpent::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	if(m_vChainSegmentEndPoints.size() < 2)
		return;

	int SegmentsToDraw = std::min((int)m_vChainSegmentEndPoints.size() - 1, (int)MAX_CHAIN_SEGMENTS);

	for(int i = 0; i < SegmentsToDraw; ++i)
	{
		vec2 MainSegmentStart = m_vChainSegmentEndPoints[i];
		vec2 MainSegmentEnd = m_vChainSegmentEndPoints[i + 1];
		vec2 CurrentSubSegmentStart = MainSegmentStart;
		vec2 SegmentVector = MainSegmentEnd - MainSegmentStart;
		vec2 PerpendicularVector = normalize(vec2(-SegmentVector.y, SegmentVector.x));

		if(length(SegmentVector) < 0.001f)
			continue;

		if(const auto* pvGroupIds = FindSnappingGroupIds(i))
		{
			for(int j = 0; j < NUM_SUB_SEGMENTS_PER_BOLT; ++j)
			{
				vec2 SubSegmentEnd;
				if(j == NUM_SUB_SEGMENTS_PER_BOLT - 1)
				{
					SubSegmentEnd = MainSegmentEnd;
				}
				else
				{
					float LerpFactor = (j + 1.0f) / (float)NUM_SUB_SEGMENTS_PER_BOLT;
					vec2 PointOnMainLine = MainSegmentStart + SegmentVector * LerpFactor;
					float RandomOffset = (random_float() * 2.0f - 1.0f) * JITTER_MAGNITUDE;
					SubSegmentEnd = PointOnMainLine + PerpendicularVector * RandomOffset;
				}

				GS()->SnapLaser(SnappingClient, (*pvGroupIds)[j], CurrentSubSegmentStart, SubSegmentEnd,
					Server()->Tick() - 2);

				CurrentSubSegmentStart = SubSegmentEnd;
			}
		}
	}
}
