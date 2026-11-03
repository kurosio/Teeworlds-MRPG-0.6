#include "gun_gravity.h"

#include <generated/server_data.h>
#include <game/server/entities/character.h>
#include <game/server/entities/pickup.h>
#include <game/server/core/entities/items/drop_pickup.h>
#include <game/server/core/components/Bots/BotData.h>
#include <game/server/core/entities/items/drop_items.h>
#include <game/server/gamecontext.h>
#include <game/server/playerbot.h>

namespace
{
	constexpr int CaptureSeconds = 8;
	constexpr float CaptureRadius = 32.f;
	constexpr float CaptureRange = 600.f;
	constexpr float BreakRange = 700.f;
	constexpr float MaxVelocity = 32.f;
	constexpr float VelocityRetention = 0.40f;
	constexpr float SpringStrength = 0.32f;
	constexpr float MinImpactSpeed = 1.f;
	constexpr float ImpactDamageOffset = 8.f;
	constexpr float ImpactSpeedPerDamage = 4.f;
	constexpr int ImpactCooldownDivisor = 2;
	constexpr int WarningSeconds = 3;
	constexpr int FieldVisualsGroup = 0;
	constexpr int RingSegments = CaptureSeconds;
	constexpr int TetherPulses = 3;
	constexpr float FieldRadius = 38.f;
	constexpr float FeedbackPulseSeconds = 0.25f;
	constexpr float StrongImpactSpeed = 16.f;

	vec2 LimitLength(vec2 Value, float Maximum)
	{
		const float Length = length(Value);
		return Length > Maximum ? Value * (Maximum / Length) : Value;
	}

	vec2 AimOffset(CCharacter* pCharacter)
	{
		return vec2(pCharacter->m_LatestInput.m_TargetX, pCharacter->m_LatestInput.m_TargetY);
	}

	vec2 PositionOf(CEntity* pEntity)
	{
		auto* pChar = dynamic_cast<CCharacter*>(pEntity);
		return pChar ? pChar->m_Core.m_Pos : (pEntity ? pEntity->GetPos() : vec2(0, 0));
	}

	int64_t VisibilityMask(CEntity* pEntity)
	{
		if (!pEntity)
			return -1;

		auto* pChar = dynamic_cast<CCharacter*>(pEntity);
		return pChar && pChar->GetPlayer() ? pChar->GetPlayer()->GetMaskVisibleForClients() : -1;
	}

	bool IsAvailable(CEntity* pEntity, int OwnerID)
	{
		if(pEntity->IsMarkedForDestroy())
			return false;

		if(auto* pChar = dynamic_cast<CCharacter*>(pEntity))
			return pChar->IsAlive();
	
		if(auto* pDrop = dynamic_cast<CEntityDropItem*>(pEntity))
			return pDrop->CanTake(OwnerID);
	
		if(auto* pPickup = dynamic_cast<CPickup*>(pEntity))
			return pPickup->CanMakeDrop();
	
		return dynamic_cast<CEntityDropPickup*>(pEntity) != nullptr;
	}

	bool IsQuestNpc(CEntity* pEntity)
	{
		const auto* pChar = dynamic_cast<CCharacter*>(pEntity);
		const auto* pBot = pChar ? dynamic_cast<CPlayerBot*>(pChar->GetPlayer()) : nullptr;
		
		if(!pBot)
			return false;
		
		// is quest bot
		if(pBot->GetBotType() == TYPE_BOT_QUEST)
			return true;
		
		// giving quest bot
		if (pBot->GetBotType() == TYPE_BOT_NPC)
		{
			const auto it = NpcBotInfo::ms_aNpcBot.find(pBot->GetBotMobID());
			return it != NpcBotInfo::ms_aNpcBot.end() && it->second.m_GiveQuestID > 0;
			return true;
		}
	
		return false;
	}

	bool IsVisible(CEntity* pEntity, int ClientID)
	{
		// check if the entity is a drop item
		if(auto* pDrop = dynamic_cast<CEntityDropItem*>(pEntity))
			return pDrop->IsVisibleForClient(ClientID);

		// check if the entity is a drop pickup
		if(auto* pDrop = dynamic_cast<CEntityDropPickup*>(pEntity))
			return pDrop->IsVisibleForClient(ClientID);
		
		// check if the entity is a pickup
		if(dynamic_cast<CPickup*>(pEntity))
			return true;
		
		// check if the entity is a character
		if (auto* pChar = dynamic_cast<CCharacter*>(pEntity))
		{
			auto* pPlayer = pChar->GetPlayer();
			if (auto* pPlayerBot = dynamic_cast<CPlayerBot*>(pPlayer))
				return pPlayerBot->IsVisibleForClient(ClientID);

			return pPlayer->IsActiveForClient(ClientID) != ESnappingPriority::None;
		}

		return ClientID == -1;
	}
}

CEntityGravityGun::CEntityGravityGun(CCharacter* pOwner, CEntity* pTarget)
	: CEntity(pOwner->GameWorld(), CGameWorld::ENTTYPE_GRAVITY_GUN, pOwner->GetPos(), 0, pOwner->GetClientID()),
	m_pOwner(pOwner), m_pTarget(pTarget), m_EndTick(Server()->Tick() + CaptureSeconds * Server()->TickSpeed()),
	m_NextImpactTick(Server()->Tick()), m_PulseStartTick(Server()->Tick())
{
	AddSnappingGroupIds(FieldVisualsGroup, RingSegments + TetherPulses);
	GameWorld()->InsertEntity(this);
	GS()->MarkUpdatedBroadcast(m_ClientID);
	const int64_t Mask = VisibilityMask(pOwner) & VisibilityMask(pTarget);
	GS()->CreateHammerHit(PositionOf(pTarget), Mask);
}

CEntityGravityGun* CEntityGravityGun::FindOwnerCapture(const CEntity* pOwner)
{
	for(auto* pEntity = pOwner->GameWorld()->FindFirst(CGameWorld::ENTTYPE_GRAVITY_GUN); pEntity; pEntity = pEntity->TypeNext())
	{
		auto* pCapture = static_cast<CEntityGravityGun*>(pEntity);
		if(!pCapture->IsMarkedForDestroy() && pCapture->m_pOwner == pOwner)
			return pCapture;
	}
	return nullptr;
}

CEntityGravityGun* CEntityGravityGun::FindTargetCapture(const CEntity* pTarget)
{
	for(auto* pEntity = pTarget->GameWorld()->FindFirst(CGameWorld::ENTTYPE_GRAVITY_GUN); pEntity; pEntity = pEntity->TypeNext())
	{
		auto* pCapture = static_cast<CEntityGravityGun*>(pEntity);
		if(!pCapture->IsMarkedForDestroy() && pCapture->m_pTarget == pTarget)
			return pCapture;
	}
	return nullptr;
}

bool CEntityGravityGun::IsEquipped(const CCharacter* pCharacter)
{
	return pCharacter->m_Core.m_ActiveWeapon == WEAPON_GUN &&
		pCharacter->GetPlayer()->GetEquippedSlotItemID(ItemType::EquipGun) == itGravityGun;
}

void CEntityGravityGun::Fire(CCharacter* pOwner)
{
	if(!IsEquipped(pOwner) || !pOwner->IsAlive())
		return;

	auto* pTarget = FindOwnerCapture(pOwner) ? nullptr : FindTarget(pOwner);
	if(pTarget && IsQuestNpc(pTarget))
	{
		pOwner->GS()->Chat(pOwner->GetClientID(), "This target cannot be grabbed: quest NPCs and quest givers are protected.");
		pTarget = nullptr;
	}
	if(auto* pPickup = dynamic_cast<CPickup*>(pTarget))
		pTarget = pPickup->MakeToDrop();
	if(pTarget)
		new CEntityGravityGun(pOwner, pTarget);
	pOwner->GS()->CreatePlayerSound(pOwner->GetClientID(), pTarget ? SOUND_HOOK_ATTACH_PLAYER : SOUND_HOOK_NOATTACH);
}

CEntity* CEntityGravityGun::FindTarget(CCharacter* pOwner)
{
	const vec2 Aim = AimOffset(pOwner);
	if(length_squared(Aim) > CaptureRange * CaptureRange)
		return nullptr;

	const vec2 Origin = pOwner->m_Core.m_Pos;
	const vec2 Cursor = Origin + Aim;
	CEntity* pClosest = nullptr;
	float ClosestDistanceSquared = CaptureRadius * CaptureRadius;
	for(int Type : {CGameWorld::ENTTYPE_CHARACTER, CGameWorld::ENTTYPE_PICKUP_ITEM, CGameWorld::ENTTYPE_PICKUP})
	{
		for(auto* pTarget = pOwner->GameWorld()->FindFirst(Type); pTarget; pTarget = pTarget->TypeNext())
		{
			const vec2 Position = PositionOf(pTarget);
			const float CursorDistanceSquared = distance_squared(Cursor, Position);
			if(pTarget == pOwner || CursorDistanceSquared >= ClosestDistanceSquared || distance_squared(Origin, Position) > CaptureRange * CaptureRange)
				continue;
			if(!IsAvailable(pTarget, pOwner->GetClientID()) || !IsVisible(pTarget, pOwner->GetClientID()) || FindTargetCapture(pTarget))
				continue;
			if(pOwner->GS()->Collision()->IntersectLineWithInvisible(Origin, Position, nullptr, nullptr))
				continue;
			ClosestDistanceSquared = CursorDistanceSquared;
			pClosest = pTarget;
		}
	}
	return pClosest;
}

bool CEntityGravityGun::IsCaptureValid() const
{
	return !IsMarkedForDestroy() && m_pOwner && m_pTarget && m_pOwner->IsAlive() &&
		IsAvailable(m_pTarget, m_ClientID) && IsEquipped(m_pOwner) &&
		(m_pOwner->m_LatestInput.m_Fire & 1) && Server()->Tick() < m_EndTick &&
		distance_squared(m_pOwner->m_Core.m_Pos, PositionOf(m_pTarget)) <= BreakRange * BreakRange;
}

void CEntityGravityGun::Release()
{
	if(IsMarkedForDestroy())
		return;

	m_pOwner = nullptr;
	m_pTarget = nullptr;
	MarkForDestroy();
	GS()->MarkUpdatedBroadcast(m_ClientID);
}

void CEntityGravityGun::Stop(CEntity* pEntity, StopScope Scope)
{
	if(auto* pCapture = FindOwnerCapture(pEntity))
		pCapture->Release();
	if(Scope == StopScope::AnyParticipant)
	{
		if(auto* pCapture = FindTargetCapture(pEntity))
			pCapture->Release();
	}
}

int CEntityGravityGun::SecondsLeft(const CCharacter* pOwner)
{
	const auto* pCapture = FindOwnerCapture(pOwner);
	if(!pCapture || pCapture->m_pOwner != pOwner)
		return 0;

	return pCapture->RemainingSeconds();
}

int CEntityGravityGun::RemainingSeconds() const
{
	const int RemainingTicks = maximum(0, m_EndTick - Server()->Tick());
	const int TickSpeed = Server()->TickSpeed();
	return (RemainingTicks + TickSpeed - 1) / TickSpeed;
}

void CEntityGravityGun::Tick()
{
	if(!IsCaptureValid())
	{
		Release();
		return;
	}

	const int Seconds = RemainingSeconds();
	if(Seconds <= WarningSeconds)
		GS()->CreatePlayerSound(m_ClientID, SOUND_HOOK_LOOP);
}

bool CEntityGravityGun::BeforeMove(CEntity* pEntity)
{
	auto* pCapture = FindTargetCapture(pEntity);
	if(!pCapture)
		return false;

	if(!pCapture->IsCaptureValid())
	{
		pCapture->Release();
		return false;
	}
	
	if(pCapture->m_pTarget != pEntity)
		return false;

	const vec2 Destination = pCapture->m_pOwner->m_Core.m_Pos + LimitLength(AimOffset(pCapture->m_pOwner), CaptureRange);
	const vec2 Displacement = Destination - PositionOf(pEntity);
	const vec2 Velocity = LimitLength(pEntity->GetVelocity() * VelocityRetention + Displacement * SpringStrength, MaxVelocity);
	pEntity->SetVelocity(Velocity);
	pCapture->m_VelocityBeforeMove = pEntity->GetVelocity();
	pCapture->m_PhysicsTick = pEntity->Server()->Tick();
	return true;
}

void CEntityGravityGun::AfterMove(CCharacter* pCharacter)
{
	auto* pCapture = FindTargetCapture(pCharacter);
	if(!pCapture || pCapture->m_pTarget != pCharacter || pCapture->m_PhysicsTick != pCharacter->Server()->Tick())
		return;

	pCapture->m_PhysicsTick = -1;
	pCapture->DamageOnImpact();
}

void CEntityGravityGun::DamageOnImpact()
{
	auto* pTarget = dynamic_cast<CCharacter*>(m_pTarget);
	if(!pTarget || Server()->Tick() < m_NextImpactTick)
		return;

	const auto& Core = pTarget->m_Core;
	const vec2 Velocity = pTarget->GetVelocity();
	const vec2 Size(CCharacter::ms_PhysSize, CCharacter::ms_PhysSize);
	float Impact = 0.f;
	if(GS()->Collision()->TestBox(Core.m_Pos + vec2(m_VelocityBeforeMove.x, 0), Size))
		Impact = maximum(Impact, absolute(m_VelocityBeforeMove.x) - absolute(Velocity.x));
	
	if(GS()->Collision()->TestBox(Core.m_Pos + vec2(0, m_VelocityBeforeMove.y), Size))
		Impact = maximum(Impact, absolute(m_VelocityBeforeMove.y) - absolute(Velocity.y));
	
	if(Impact < MinImpactSpeed)
		return;
	
	m_NextImpactTick = Server()->Tick() + maximum(1, Server()->TickSpeed() / ImpactCooldownDivisor);
	if(!pTarget->IsAllowedPVP(m_ClientID))
		return;

	const vec2 Position = Core.m_Pos;
	const int HealthBefore = pTarget->Health();
	const int64_t Mask = VisibilityMask(m_pOwner) & VisibilityMask(pTarget);
	const int Damage = maximum(1, round_to_int((Impact - ImpactDamageOffset) / ImpactSpeedPerDamage));
	pTarget->TakeDamage(vec2(0, 0), Damage, m_ClientID, WEAPON_GUN);
	if(m_pTarget && pTarget->Health() >= HealthBefore)
		return;

	GS()->m_Events.CreatePositionEvent<CNetEvent_Explosion>(Position, Mask);
	GS()->CreateSound(Position, Impact >= StrongImpactSpeed ? SOUND_GRENADE_EXPLODE : SOUND_HAMMER_HIT, Mask);
	m_PulseStartTick = Server()->Tick();
	m_PulseStrength = clamp(Impact / StrongImpactSpeed, 0.35f, 1.f);
}

void CEntityGravityGun::Snap(int SnappingClient)
{
	if(!IsCaptureValid() || !IsVisible(m_pOwner, SnappingClient) || !IsVisible(m_pTarget, SnappingClient))
		return;
	
	m_Pos = m_pOwner->m_Core.m_Pos;
	m_PosTo = PositionOf(m_pTarget);
	if(NetworkClipped(SnappingClient, m_Pos) && NetworkClipped(SnappingClient, m_PosTo))
		return;
	
	GS()->SnapLaser(SnappingClient, GetID(), m_PosTo, m_Pos, Server()->Tick() - 1);
	SnapCaptureField(SnappingClient);
}

void CEntityGravityGun::SnapCaptureField(int SnappingClient)
{
	const auto* pIds = FindSnappingGroupIds(FieldVisualsGroup);
	if(!pIds)
		return;

	const int Tick = Server()->Tick();
	const float TickSpeed = (float)Server()->TickSpeed();
	const float Age = (Tick - (m_EndTick - CaptureSeconds * Server()->TickSpeed())) / TickSpeed;
	const float Motion = clamp(length(m_pTarget->GetVelocity()) / MaxVelocity, 0.f, 1.f);
	const float PulseAge = (Tick - m_PulseStartTick) / TickSpeed;
	const float Feedback = maximum(0.f, 1.f - PulseAge / FeedbackPulseSeconds) * m_PulseStrength;
	const int Seconds = RemainingSeconds();
	const float Breathing = sinf(Age * (Seconds <= WarningSeconds ? 22.f : 10.f));
	const float Radius = FieldRadius + 3.f * Breathing + 6.f * Motion + 14.f * Feedback;

	const float Step = 2.f * pi / RingSegments;
	for(int i = 0; i < minimum(Seconds, RingSegments); ++i)
	{
		const float StartAngle = Age * 1.5f + Step * i;
		const float EndAngle = StartAngle + Step * 0.7f;
		const vec2 Start = m_PosTo + vec2(cosf(StartAngle), sinf(StartAngle)) * Radius;
		const vec2 End = m_PosTo + vec2(cosf(EndAngle), sinf(EndAngle)) * Radius;
		GS()->SnapLaser(SnappingClient, (*pIds)[i], End, Start, Tick - 1);
	}

	const vec2 Delta = m_PosTo - m_Pos;
	if(length_squared(Delta) < 1.f)
		return;

	const vec2 HalfWidth = normalize(vec2(-Delta.y, Delta.x)) * (2.f + 2.f * Motion + 3.f * Feedback);
	const float Phase = Age * 1.2f + maximum(0.f, Age - (CaptureSeconds - WarningSeconds)) * 1.2f;
	for(int i = 0; i < TetherPulses; ++i)
	{
		const float Progress = fmodf(Phase + (float)i / TetherPulses, 1.f);
		const vec2 Center = m_Pos + Delta * Progress;
		GS()->SnapLaser(SnappingClient, (*pIds)[RingSegments + i], Center + HalfWidth, Center - HalfWidth, Tick - 1);
	}
}
