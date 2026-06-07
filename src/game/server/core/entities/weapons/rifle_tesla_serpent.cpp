#include "rifle_tesla_serpent.h"
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/entities/character.h>

enum
{
	MAX_CHAIN_SEGMENTS = 3,
	NUM_SUB_SEGMENTS_PER_BOLT = 3,
};

static constexpr float JITTER_MAGNITUDE = 64.0f;

CEntityTeslaSerpent::CEntityTeslaSerpent(CGameWorld* pGameWorld, int OwnerCID, vec2 Pos, vec2 Direction, float Damage, float ChainRange, int MaxTargets, float DamageFalloff)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_LASER, Pos, 0, OwnerCID), m_InitialDir(normalize(Direction)),
	m_LifeSpanTicks(Server()->TickSpeed() / 4), m_Damage(Damage), m_ChainRange(ChainRange),
	m_MaxTargetsInChain(clamp(MaxTargets, 1, MAX_CHAIN_SEGMENTS + 1)), m_DamageFalloff(DamageFalloff), m_ChainCalculationDone(false)
{
	m_vTargetsHitThisShot.reserve(m_MaxTargetsInChain);
	m_vChainSegmentEndPoints.reserve(m_MaxTargetsInChain + 1);
	m_vChainSegmentEndPoints.push_back(m_Pos);

	for(int i = 0; i < MAX_CHAIN_SEGMENTS; i++)
		AddSnappingGroupIds(i, NUM_SUB_SEGMENTS_PER_BOLT);

	GameWorld()->InsertEntity(this);

	// append damage by 25% from item
	if(auto* pOwner = GetOwner(); pOwner && pOwner->GetItem(itTeslaInductiveCoil)->IsEquipped())
	{
		m_Damage += maximum(1.0f, translate_to_percent_rest(m_Damage, 25.0f));
	}
}

void CEntityTeslaSerpent::Tick()
{
	if(!GetOwnerChar())
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	if(!m_ChainCalculationDone)
	{
		CalculateChainLightning();
		m_ChainCalculationDone = true;
	}

	if(--m_LifeSpanTicks < 0)
	{
		GameWorld()->DestroyEntity(this);
	}
}

void CEntityTeslaSerpent::CalculateChainLightning()
{
	vec2 CurrentSource = m_Pos;
	vec2 CurrentDir = m_InitialDir;
	float CurrentDamage = m_Damage;

	for(int i = 0; i < m_MaxTargetsInChain; ++i)
	{
		CCharacter* pClosestTarget = nullptr;
		vec2 TargetPos = CurrentSource + CurrentDir * m_ChainRange;
		float MinDist = m_ChainRange;

		vec2 WallPos;
		if(GS()->Collision()->IntersectLineWithInvisible(CurrentSource, TargetPos, &WallPos, nullptr))
		{
			TargetPos = WallPos;
			MinDist = distance(CurrentSource, WallPos);
		}

		for(auto* pChar = (CCharacter*)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
		{
			const int TargetCID = pChar->GetPlayer()->GetCID();
			if(TargetCID == m_ClientID || !pChar->IsAllowedPVP(m_ClientID))
				continue;

			if(std::find(m_vTargetsHitThisShot.begin(), m_vTargetsHitThisShot.end(), TargetCID) != m_vTargetsHitThisShot.end())
				continue;

			const float Dist = distance(CurrentSource, pChar->m_Core.m_Pos);
			if(Dist < MinDist && !GS()->Collision()->IntersectLineWithInvisible(CurrentSource, pChar->m_Core.m_Pos, nullptr, nullptr))
			{
				MinDist = Dist;
				TargetPos = pChar->m_Core.m_Pos;
				pClosestTarget = pChar;
			}
		}

		m_vChainSegmentEndPoints.push_back(TargetPos);

		GS()->CreateExplosion(TargetPos, m_ClientID, WEAPON_LASER, round_to_int(CurrentDamage), FORCE_FLAG_CANT_SELF);
		GS()->CreateSound(TargetPos, SOUND_GRENADE_EXPLODE);
		GS()->CreateSound(TargetPos, SOUND_HOOK_LOOP);

		if(!pClosestTarget)
			break;

		m_vTargetsHitThisShot.push_back(pClosestTarget->GetPlayer()->GetCID());
		CurrentSource = TargetPos;
		CurrentDamage *= m_DamageFalloff;
		CurrentDir = vec2(0, 0);

		if(m_vChainSegmentEndPoints.size() > MAX_CHAIN_SEGMENTS)
			break;
	}
}

void CEntityTeslaSerpent::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient) || m_vChainSegmentEndPoints.size() < 2)
		return;

	const int NumSegments = std::min<int>(m_vChainSegmentEndPoints.size() - 1, MAX_CHAIN_SEGMENTS);

	for(int i = 0; i < NumSegments; ++i)
	{
		const vec2 Start = m_vChainSegmentEndPoints[i];
		const vec2 End = m_vChainSegmentEndPoints[i + 1];
		const vec2 Delta = End - Start;
		const float SegLen = length(Delta);

		if(SegLen < 0.001f)
			continue;

		const vec2 Perpendicular = normalize(vec2(-Delta.y, Delta.x));
		const auto* pGroupIds = FindSnappingGroupIds(i);
		if(!pGroupIds)
			continue;

		vec2 SubStart = Start;
		for(int j = 0; j < NUM_SUB_SEGMENTS_PER_BOLT; ++j)
		{
			vec2 SubEnd;
			if(j == NUM_SUB_SEGMENTS_PER_BOLT - 1)
			{
				SubEnd = End;
			}
			else
			{
				const float Lerp = (float)((j + 1) / (int)NUM_SUB_SEGMENTS_PER_BOLT);
				const float Jitter = (random_float() * 2.0f - 1.0f) * JITTER_MAGNITUDE;
				SubEnd = Start + Delta * Lerp + Perpendicular * Jitter;
			}

			GS()->SnapLaser(SnappingClient, (*pGroupIds)[j], SubStart, SubEnd, Server()->Tick() - 2);
			SubStart = SubEnd;
		}
	}
}