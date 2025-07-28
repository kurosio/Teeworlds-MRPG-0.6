/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "entity_manager.h"
#include "gamecontext.h"

#include "entities/projectile.h"
#include "core/entities/group/entitiy_group.h"
#include "core/entities/items/design_drop.h"
#include "core/entities/items/drop_pickup.h"
#include "core/entities/items/drop_items.h"
#include "core/entities/tools/flying_point.h"
#include "core/entities/tools/laser_orbite.h"
#include "core/entities/tools/loltext.h"

#include "core/components/skills/entities/heart_healer.h"
#include "core/entities/weapons/rifle_tesla_serpent.h"

IServer* CEntityManager::Server() const
{
	return Instance::Server();
}

CEntityManager::CEntityManager(CGS* pGS)
{
	m_pGS = pGS;
}

void CEntityManager::DesignRandomDrop(int Amount, float Force, vec2 Pos, int LifeSpan, int Type, int Subtype, int64_t Mask) const
{
	for(int i = 0; i < Amount; i++)
	{
		const auto RandVel = vec2(random_float(-Force, Force), Force + random_float(-Force, Force));
		new CEntityDesignDrop(&GS()->m_World, LifeSpan, Pos, RandVel, Type, Subtype, Mask);
	}
}

void CEntityManager::DropPickup(vec2 Pos, int Type, int Subtype, int Value, int NumDrop, vec2 Force) const
{
	for(int i = 0; i < NumDrop; i++)
	{
		vec2 Vel = Force;
		Vel.x += random_float(15.0f);
		Vel.y += random_float(15.0f);
		new CEntityDropPickup(&GS()->m_World, Pos, Vel, Type, Subtype, Value);
	}
}

void CEntityManager::DropItem(vec2 Pos, int ClientID, const CItem& Item, vec2 Force) const
{
	if(Item.IsValid())
	{
		const float Angle = angle(normalize(Force));
		new CEntityDropItem(&GS()->m_World, Pos, Force, Angle, Item, ClientID);
	}
}

void CEntityManager::RandomDropItem(vec2 Pos, int ClientID, float Chance, const CItem& Item, vec2 Force) const
{
	if(random_float(100.0f) < Chance)
		DropItem(Pos, ClientID, Item, Force);
}

void CEntityManager::FlyingPoint(vec2 Pos, int ClientID, vec2 Force) const
{
	new CEntityFlyingPoint(&GS()->m_World, Pos, Force, ClientID, -1);
}

void CEntityManager::ExpFlyingPoint(vec2 Pos, int ClientID, int Exp, vec2 Force) const
{
	auto* pPoint = new CEntityFlyingPoint(&GS()->m_World, Pos, Force, ClientID, -1);
	pPoint->Register([Exp](CPlayer*, CPlayer* pPlayer)
	{
		pPlayer->Account()->AddExperience(Exp);
	});
}

void CEntityManager::Text(vec2 Pos, int Lifespan, const char* pText, bool* pResult) const
{
	if(!GS()->ArePlayersNearby(Pos, 800))
	{
		if(pResult)
			*pResult = false;
		return;
	}
	if(pResult)
		*pResult = true;
	CLoltext::Create(&GS()->m_World, nullptr, Pos, Lifespan, pText);
}

void CEntityManager::Text(CEntity* pParent, int Lifespan, const char* pText, bool* pResult) const
{
	if(!pParent || !GS()->ArePlayersNearby(pParent->GetPos(), 800))
	{
		if(pResult)
			*pResult = false;
		return;
	}
	if(pResult)
		*pResult = true;
	CLoltext::Create(&GS()->m_World, pParent, {}, Lifespan, pText);
}

void CEntityManager::LaserOrbite(int ClientID, int Amount, LaserOrbiteType Type, float Speed, float Radius, int LaserType, int64_t Mask) const
{
	if(GS()->GetPlayer(ClientID, false, true))
		new CEntityLaserOrbite(&GS()->m_World, ClientID, nullptr, Amount, Type, Speed, Radius, LaserType, Mask);
}

void CEntityManager::LaserOrbite(CEntity* pParent, int Amount, LaserOrbiteType Type, float Speed, float Radius, int LaserType, int64_t Mask) const
{
	if(pParent)
		new CEntityLaserOrbite(&GS()->m_World, -1, pParent, Amount, Type, Speed, Radius, LaserType, Mask);
}

void CEntityManager::LaserOrbite(CEntityLaserOrbite*& pOut, int ClientID, int Amount, LaserOrbiteType Type, float Speed, float Radius, int LaserType, int64_t Mask) const
{
	if(GS()->GetPlayer(ClientID, false, true))
		pOut = new CEntityLaserOrbite(&GS()->m_World, ClientID, nullptr, Amount, Type, Speed, Radius, LaserType, Mask);
}

void CEntityManager::LaserOrbite(CEntityLaserOrbite*& pOut, CEntity* pParent, int Amount, LaserOrbiteType Type, float Speed, float Radius, int LaserType, int64_t Mask) const
{
	if(pParent)
		pOut = new CEntityLaserOrbite(&GS()->m_World, -1, pParent, Amount, Type, Speed, Radius, LaserType, Mask);
}

void CEntityManager::GravityDisruption(int ClientID, vec2 Position, float Radius, int Lifetime, int Damage, EntGroupWeakPtr* pPtr) const
{
	// initialize group & config
	auto groupPtr = CEntityGroup::NewGroup(&GS()->m_World, CGameWorld::ENTTYPE_SKILL, ClientID);
	groupPtr->SetConfig("radius", Radius);

	// initialize element & config
	auto pPickup = groupPtr->CreatePickup(Position, POWERUP_ARMOR);
	pPickup->SetConfig("lifetimeTick", Lifetime);
	pPickup->SetConfig("damage", Damage);

	// register event tick
	pPickup->RegisterEvent(CBaseEntity::EventTick, [](CBaseEntity* pBase)
	{
		int& LifeTime = pBase->GetRefConfig("lifetimeTick", 0);
		const int Damage = pBase->GetConfig("damage", 0);
		const float Radius = pBase->GetGroup()->GetConfig("radius", 0.f);
		const vec2 BasePos = pBase->GetPos();

		// life time
		if(LifeTime <= 0)
		{
			pBase->GS()->CreateCyrcleExplosion(12, Radius, BasePos, pBase->GetClientID(), WEAPON_GAME, Damage);
			pBase->MarkForDestroy();
			return;
		}
		LifeTime--;

		// magnetism
		for(auto* pChar = (CCharacter*)pBase->GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
		{
			const float Dist = distance(BasePos, pChar->m_Core.m_Pos);
			if(Dist > Radius || Dist < 24.0f)
				continue;

			if(!pBase->GetPlayer() || (pBase->GetClientID() != pChar->GetPlayer()->GetCID() && pChar->IsAllowedPVP(pBase->GetClientID())))
			{
				vec2 Dir = normalize(pChar->m_Core.m_Pos - BasePos);
				pChar->AddVelocity(-Dir * 1.5f);
			}
		}
	});

	// register event snap
	pPickup->RegisterEvent(CBaseEntity::EventSnap, 12, [](CBaseEntity* pBase, int SnappingClient, const std::vector<int>& vIds)
	{
		const float Radius = pBase->GetGroup()->GetConfig("radius", 0.f);
		const float AngleStep = 2.0f * pi / static_cast<float>(vIds.size());
		const vec2 BasePos = pBase->GetPos();

		for(size_t i = 0; i < vIds.size(); ++i)
		{
			float Angle = AngleStep * static_cast<float>(i);
			vec2 VertexPos = BasePos + vec2(Radius * cos(Angle), Radius * sin(Angle));
			pBase->GS()->SnapProjectile(SnappingClient, vIds[i], VertexPos, {}, pBase->Server()->Tick() - 1, WEAPON_HAMMER);
		}
	});

	if(pPtr)
		*pPtr = groupPtr;
}

void CEntityManager::HealthTurret(int ClientID, vec2 Position, int RestoreHealth, int Lifetime, int InitialReloadTick, EntGroupWeakPtr* pPtr) const
{
	// initialize group & config
	auto groupPtr = CEntityGroup::NewGroup(&GS()->m_World, CGameWorld::ENTTYPE_SKILL, ClientID);
	groupPtr->SetConfig("healthRestored", RestoreHealth);

	// initialize element & config
	auto* pPickup = groupPtr->CreatePickup(Position, POWERUP_ARMOR);
	pPickup->SetConfig("initialReloadTick", InitialReloadTick);
	pPickup->SetConfig("lifetimeTick", Lifetime);
	pPickup->SetConfig("currentReloadTick", InitialReloadTick);

	// register event tick
	pPickup->RegisterEvent(CBaseEntity::EventTick, [](CBaseEntity* pBase)
	{
		// lifetime
		int& Lifetime = pBase->GetRefConfig("lifetimeTick", 0);
		if(Lifetime <= 0)
		{
			pBase->MarkForDestroy();
			return;
		}
		Lifetime--;

		// reload
		int& ReloadTick = pBase->GetRefConfig("currentReloadTick", 0);
		if(ReloadTick > 0)
		{
			ReloadTick--;
			return;
		}
		ReloadTick = pBase->GetConfig("initialReloadTick", 2 * pBase->Server()->TickSpeed());

		// variables
		bool ShowRestoreHealth = false;
		const int HealthRestored = pBase->GetGroup()->GetConfig("healthRestored", 0);

		// restore health
		for(auto* pChar = (CCharacter*)pBase->GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
		{
			const float Distance = distance(pBase->GetPos(), pChar->m_Core.m_Pos);
			if(Distance < 620.f &&
				(!pBase->GetPlayer() || (pBase->GetClientID() == pChar->GetPlayer()->GetCID() || !pChar->IsAllowedPVP(pBase->GetClientID()))))
			{
				ShowRestoreHealth = true;
				new CHeartHealer(pBase->GameWorld(), pBase->GetPos(), pChar->GetPlayer(), HealthRestored, pChar->m_Core.m_Vel / 2.f);
			}
		}

		if(ShowRestoreHealth)
		{
			pBase->GS()->EntityManager()->Text(pBase->GetPos() + vec2(0, -96), 40, fmt_default("{}HP", HealthRestored).c_str());
		}
	});

	// register event snap
	pPickup->RegisterEvent(CBaseEntity::EventSnap, 4, [](CBaseEntity* pBase, int SnappingClient, const std::vector<int>& vIds)
	{
		const int ReloadTick = pBase->GetConfig("currentReloadTick", 0);
		const float Radius = clamp(static_cast<float>(ReloadTick), 0.0f, 32.0f);
		const float AngleStep = 2.0f * pi / static_cast<float>(vIds.size());

		for(size_t i = 0; i < vIds.size(); ++i)
		{
			const vec2 VertexPos = pBase->GetPos() + vec2(Radius * cos(AngleStep * static_cast<float>(i)), Radius * sin(AngleStep * static_cast<float>(i)));
			pBase->GS()->SnapPickup(SnappingClient, vIds[i], VertexPos, POWERUP_HEALTH);
		}
	});

	if(pPtr)
		*pPtr = groupPtr;
}

void CEntityManager::LastStand(int ClientID, vec2 Position, float Radius, int PercentManaCostPerSec, EntGroupWeakPtr* pPtr) const
{
	// initialize
	const auto groupPtr = CEntityGroup::NewGroup(&GS()->m_World, CGameWorld::ENTTYPE_SKILL, ClientID);
	const auto pBase = groupPtr->CreateBase(Position);
	groupPtr->SetConfig("radius", Radius);

	// register event tick
	pBase->RegisterEvent(CBaseEntity::EventTick, [PercentManaCostPerSec](CBaseEntity* pBase)
	{
		auto* pChar = pBase->GetCharacter();

		// action
		if(pBase->Server()->Tick() % pBase->Server()->TickSpeed() == 0)
		{
			const int ManaPerSeconds = maximum(1, translate_to_percent_rest(pChar->GetPlayer()->GetMaxMana(), PercentManaCostPerSec));
			if(!pChar->TryUseMana(ManaPerSeconds))
			{
				if(pChar->GetPlayer()->m_Effects.Remove("LastStand"))
					pBase->GS()->Chat(pBase->GetClientID(), "'Last Stand' effect has been removed.");

				pBase->GS()->Broadcast(pBase->GetClientID(), BroadcastPriority::MainInformation, 100, "Not enough mana to maintain the shield.");
				pBase->MarkForDestroy();
				return;
			}

			const auto LastStandTime = 3;
			pChar->GetPlayer()->m_Effects.Add("LastStand", LastStandTime * pBase->Server()->TickSpeed());
		}

		pBase->SetPos(pChar->GetPos());
	});

	// register event snap
	pBase->RegisterEvent(CBaseEntity::EventSnap, 12, [](CBaseEntity* pBase, int SnappingClient, const std::vector<int>& vIds)
	{
		const auto Radius = pBase->GetGroup()->GetConfig("radius", 0.f);
		const auto AngleStep = 2.0f * pi / static_cast<float>(vIds.size());
		const auto BasePos = pBase->GetPos();

		for(size_t i = 0; i < vIds.size(); ++i)
		{
			const auto Angle = AngleStep * static_cast<float>(i);
			const auto VertexPos = BasePos + vec2(Radius * cos(Angle), Radius * sin(Angle));
			pBase->GS()->SnapProjectile(SnappingClient, vIds[i], VertexPos, {}, pBase->Server()->Tick() - 1, WEAPON_HAMMER);
		}
	});

	if(pPtr)
	{
		*pPtr = groupPtr;
	}
}

void CEntityManager::FlameWall(int ClientID, vec2 Position, float Radius, int Lifetime, int DamagePerTick, float SlowDownFactor, EntGroupWeakPtr* pPtr) const
{
	// initialize group & config
	auto groupPtr = CEntityGroup::NewGroup(&GS()->m_World, CGameWorld::ENTTYPE_SKILL, ClientID);
	groupPtr->SetConfig("radius", Radius);

	// initialize element & config
	auto pPickup = groupPtr->CreatePickup(Position, POWERUP_ARMOR);
	pPickup->SetConfig("lifetimeTick", Lifetime);
	pPickup->SetConfig("damagePerTick", DamagePerTick);
	pPickup->SetConfig("slowDownFactor", SlowDownFactor);

	// register event tick
	pPickup->RegisterEvent(CBaseEntity::EventTick, [](CBaseEntity* pBase)
	{
		// lifetime
		int& Lifetime = pBase->GetRefConfig("lifetimeTick", 0);
		if(Lifetime <= 0)
		{
			pBase->MarkForDestroy();
			return;
		}
		Lifetime--;

		const float Radius = pBase->GetGroup()->GetConfig("radius", 0.f);
		const int DamagePerTick = pBase->GetConfig("damagePerTick", 0);
		const float SlowDownFactor = pBase->GetConfig("slowDownFactor", 0.f);
		const int TickSpeed = pBase->Server()->TickSpeed();

		// random explosion on radius
		if(pBase->Server()->Tick() % (TickSpeed / 2) == 0)
			pBase->GS()->CreateRandomRadiusExplosion(2, Radius, pBase->GetPos(), pBase->GetClientID(), WEAPON_GAME, DamagePerTick);

		// damage and slowdown enemies
		for(auto* pChar = (CCharacter*)pBase->GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
		{
			if(distance(pBase->GetPos(), pChar->m_Core.m_Pos) > Radius)
				continue;

			if(!pBase->GetPlayer() || (pBase->GetClientID() != pChar->GetPlayer()->GetCID() && pChar->IsAllowedPVP(pBase->GetClientID())))
			{
				if(pBase->Server()->Tick() % TickSpeed == 0)
					pChar->TakeDamage(vec2(0, 0), DamagePerTick, pBase->GetClientID(), WEAPON_GAME);

				vec2 NewVelocity = pChar->m_Core.m_Vel * SlowDownFactor;
				pChar->SetVelocity(NewVelocity);
			}
		}
	});

	// register event snap
	enum { NUM_CIRCLE = 10, NUM_PROJ_INSIDE = 2, NUM_IDS = NUM_CIRCLE + NUM_PROJ_INSIDE};
	pPickup->RegisterEvent(CBaseEntity::EventSnap, NUM_IDS, [](CBaseEntity* pBase, int SnappingClient, const std::vector<int>& vIds)
	{
		const float Radius = pBase->GetGroup()->GetConfig("radius", 0.f);
		constexpr float AngleStep = 2.0f * pi / static_cast<float>(NUM_CIRCLE);

		// create projectiles in a circle
		for(int i = 0; i < NUM_CIRCLE; ++i)
		{
			vec2 VertexPos = pBase->GetPos() + vec2(Radius * cos(AngleStep * i), Radius * sin(AngleStep * i));
			pBase->GS()->SnapProjectile(SnappingClient, vIds[i], VertexPos, {}, pBase->Server()->Tick() - 1, WEAPON_GRENADE);
		}

		// create random projectiles inside the circle
		for(int i = 0; i < NUM_PROJ_INSIDE; ++i)
		{
			vec2 VertexPos = random_range_pos(pBase->GetPos(), Radius);
			pBase->GS()->SnapProjectile(SnappingClient, vIds[NUM_CIRCLE + i], VertexPos, {}, pBase->Server()->Tick() - 1, WEAPON_HAMMER);
		}
	});

	if(pPtr)
		*pPtr = groupPtr;
}

void CEntityManager::FrostNova(int ClientID, vec2 Position, float Radius, int Damage, int FreezeTime, EntGroupWeakPtr* pPtr) const
{
	// initialize group & config
	auto groupPtr = CEntityGroup::NewGroup(&GS()->m_World, CGameWorld::ENTTYPE_SKILL, ClientID);
	groupPtr->SetConfig("radius", Radius);

	// initialize element & config
	auto pNova = groupPtr->CreatePickup(Position, POWERUP_ARMOR);
	pNova->SetConfig("damage", Damage);
	pNova->SetConfig("freezeTime", FreezeTime);

	// register event tick
	pNova->RegisterEvent(CBaseEntity::EventTick, [](CBaseEntity* pBase)
	{
		const float Radius = pBase->GetGroup()->GetConfig("radius", 0.f);
		const int Damage = pBase->GetConfig("damage", 0);
		//const int FreezeTime = pBase->GetConfig("freezeTime", 0);

		// damage and freeze enimies
		for(auto* pChar = (CCharacter*)pBase->GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
		{
			if(distance(pBase->GetPos(), pChar->m_Core.m_Pos) > Radius)
				continue;

			if(pBase->GetClientID() != pChar->GetPlayer()->GetCID() && pChar->IsAllowedPVP(pBase->GetClientID()))
			{
				pChar->TakeDamage(vec2(0, 0), Damage, pBase->GetClientID(), WEAPON_GAME);
				//pChar->Freeze(FreezeTime);
				pBase->MarkForDestroy();
			}
		}
	});

	// register event snap
	pNova->RegisterEvent(CBaseEntity::EventSnap, 4, [](CBaseEntity* pBase, int SnappingClient, const std::vector<int>& vIds)
	{
		const float Radius = pBase->GetGroup()->GetConfig("radius", 0.f);
		const float AngleStep = 2.0f * pi / static_cast<float>(vIds.size());

		for(size_t i = 0; i < vIds.size(); ++i)
		{
			vec2 VertexPos = pBase->GetPos() + vec2(Radius * cos(AngleStep * i), Radius * sin(AngleStep * i));
			pBase->GS()->SnapPickup(SnappingClient, vIds[i], VertexPos, POWERUP_ARMOR);
		}
	});

	if(pPtr)
		*pPtr = groupPtr;
}

void CEntityManager::HealingAura(int ClientID, vec2 Position, float Radius, int Lifetime, int HealPerTick, EntGroupWeakPtr* pPtr) const
{
	enum
	{
		NUM_CIRCLES = 6,
		NUM_HEART = 8,
		NUM_IDS = (NUM_CIRCLES * 2) + NUM_HEART
	};

	// initialize group & config
	const auto groupPtr = CEntityGroup::NewGroup(&GS()->m_World, CGameWorld::ENTTYPE_SKILL, ClientID);
	groupPtr->SetConfig("radius", Radius);

	// initialize element & config
	const auto pPickup = groupPtr->CreatePickup(Position, POWERUP_ARMOR);
	pPickup->SetConfig("lifetimeTick", Lifetime);
	pPickup->SetConfig("healPerTick", HealPerTick);

	// generate random positions for hearts inside the radius
	std::vector<vec2> heartPositions(NUM_HEART);
	std::vector<vec2> heartVelocities(NUM_HEART);
	for(int i = 0; i < NUM_HEART; ++i)
	{
		heartPositions[i] = random_range_pos(Position, Radius);
		heartPositions[i].y += Radius;
		heartVelocities[i] = vec2(random_float(-1.0f, 1.0f), random_float(-2.0f, -1.0f));
	}
	pPickup->SetConfig("heartPositions", heartPositions);
	pPickup->SetConfig("heartVelocities", heartVelocities);

	// register event tick
	pPickup->RegisterEvent(CBaseEntity::EventTick, [](CBaseEntity* pBase)
	{
		// life time
		auto& Lifetime = pBase->GetRefConfig("lifetimeTick", 0);
		if(Lifetime <= 0)
		{
			pBase->MarkForDestroy();
			return;
		}
		Lifetime--;

		// variables
		const auto Radius = pBase->GetGroup()->GetConfig("radius", 0.f);
		const auto HealPerTick = pBase->GetConfig("healPerTick", 0);
		const auto TickSpeed = pBase->Server()->TickSpeed();

		// healing players
		if(pBase->Server()->Tick() % TickSpeed == 0)
		{
			for(auto* pChar = (CCharacter*)pBase->GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
			{
				if(distance(pBase->GetPos(), pChar->m_Core.m_Pos) > Radius)
					continue;

				if(pBase->GetClientID() == pChar->GetPlayer()->GetCID() || !pChar->IsAllowedPVP(pBase->GetClientID()))
				{
					pChar->IncreaseHealth(HealPerTick);
				}
			}
		}

		// update heart positions and velocities
		auto& heartPositions = pBase->GetRefConfig("heartPositions", std::vector<vec2>{});
		auto& heartVelocities = pBase->GetRefConfig("heartVelocities", std::vector<vec2>{});

		for(int i = 0; i < NUM_HEART; ++i)
		{
			auto& pos = heartPositions[i];
			auto& vel = heartVelocities[i];

			pos += vel;
			vel.x = clamp(vel.x + random_float(-0.1f, 0.1f), -1.0f, 1.0f);
			vel.y = clamp(vel.y + random_float(-0.1f, 0.1f), -2.0f, -1.0f);

			if(pos.y < pBase->GetPos().y - Radius)
			{
				pos = random_range_pos(pBase->GetPos(), Radius);
				pos.y = pBase->GetPos().y + Radius;
				vel = vec2(random_float(-1.0f, 1.0f), random_float(-2.0f, -1.0f));
			}
		}
	});

	// register event snap
	pPickup->RegisterEvent(CBaseEntity::EventSnap, NUM_IDS, [](CBaseEntity* pBase, int SnappingClient, const std::vector<int>& vIds)
	{
		const auto Radius = pBase->GetGroup()->GetConfig("radius", 0.f);
		constexpr float AngleStep = 2.0f * pi / static_cast<float>(NUM_CIRCLES);

		// snap cyrcle
		for(int i = 0; i < NUM_CIRCLES; ++i)
		{
			const auto VertexPos = pBase->GetPos() + vec2(Radius * cos(AngleStep * i), Radius * sin(AngleStep * i));
			pBase->GS()->SnapPickup(SnappingClient, vIds[i], VertexPos, POWERUP_HEALTH);
		}

		// snap cyrcle connect
		for(int i = 0; i < NUM_CIRCLES; ++i)
		{
			const auto nextIndex = (i + 1) % NUM_CIRCLES;
			const auto CurrentPos = pBase->GetPos() + vec2(Radius * cos(AngleStep * i), Radius * sin(AngleStep * i));
			const auto NextPos = pBase->GetPos() + vec2(Radius * cos(AngleStep * nextIndex), Radius * sin(AngleStep * nextIndex));
			pBase->GS()->SnapLaser(SnappingClient, vIds[NUM_CIRCLES + i], CurrentPos, NextPos, pBase->Server()->Tick() - 1);
		}

		// snap hearts inside cyrcle
		const auto& heartPositions = pBase->GetConfig("heartPositions", std::vector<vec2>{});
		for(int i = 0; i < NUM_HEART; ++i)
		{
			const auto InnerPos = heartPositions[i];
			pBase->GS()->SnapPickup(SnappingClient, vIds[NUM_CIRCLES * 2 + i], InnerPos, POWERUP_HEALTH);
		}
	});

	if(pPtr)
	{
		*pPtr = groupPtr;
	}
}

void CEntityManager::Bow(int ClientID, int Damage, int FireCount, float ExplosionRadius, int ExplosionCount, EntGroupWeakPtr* pPtr) const
{
	const auto* pPlayer = GS()->GetPlayer(ClientID, false, true);
	if(!pPlayer || !pPlayer->GetCharacter())
		return;

	constexpr int MaxChargeTime = 35;
	const auto groupPtr = CEntityGroup::NewGroup(&GS()->m_World, CGameWorld::ENTTYPE_SKILL, ClientID);
	groupPtr->SetConfig("damage", Damage);
	groupPtr->SetConfig("initialFireCount", FireCount);
	groupPtr->SetConfig("explosionRadius", ExplosionRadius);
	groupPtr->SetConfig("explosionCount", ExplosionCount);

	const auto pBowController = groupPtr->CreateBase(pPlayer->GetCharacter()->GetPos());
	pBowController->SetConfig("currentFireCount", FireCount);
	pBowController->SetConfig("chargeTick", 0);
	pBowController->SetConfig("isCharging", 0);

	pBowController->RegisterEvent(CBaseEntity::EventTick, [MaxChargeTime](CBaseEntity* pBase)
	{
		int& CurrentFireCount = pBase->GetRefConfig("currentFireCount", 0);
		int& IsCharging = pBase->GetRefConfig("isCharging", 0);
		int& ChargeTick = pBase->GetRefConfig("chargeTick", 0);

		// block input
		pBase->Server()->Input()->BlockInputGroup(pBase->GetClientID(), BLOCK_INPUT_FIRE);
		pBase->Server()->Input()->BlockInputGroup(pBase->GetClientID(), BLOCK_INPUT_FREEZE_GUN);

		// update position
		auto* pOwnerChar = pBase->GetCharacter();
		pBase->SetPos(pOwnerChar->GetPos());

		// charging
		if(pOwnerChar->m_Core.m_Input.m_Fire & 1)
		{
			if(!IsCharging)
			{
				IsCharging = 1;
				pBase->GS()->CreateSound(pOwnerChar->GetPos(), SOUND_NINJA_HIT);
			}
			if(ChargeTick < MaxChargeTime)
			{
				ChargeTick++;
			}
		}
		else if(IsCharging)
		{
			IsCharging = 0;
			float ChargePower = static_cast<float>(ChargeTick) / static_cast<float>(MaxChargeTime);
			ChargePower = std::max(0.1f, ChargePower);
			ChargeTick = 0;
			CurrentFireCount--;

			// creating bow and fire
			pBase->GS()->CreateSound(pOwnerChar->GetPos(), SOUND_SHOTGUN_FIRE);
			const auto Direction = normalize(vec2(pOwnerChar->m_Core.m_Input.m_TargetX, pOwnerChar->m_Core.m_Input.m_TargetY));
			const auto ProjStartPos = pOwnerChar->GetPos() + Direction * 40.0f;
			const auto pArrow = pBase->GetGroup()->CreateBase(ProjStartPos);

			// damage and speed by charge power
			const float ArrowSpeed = 10.f * (1.0f + (ChargePower * 2.5f));
			const int ArrowDamage = static_cast<int>(pBase->GetGroup()->GetConfig("damage", 0) * (1.0f + ChargePower));
			const auto InitialVelocity = Direction * ArrowSpeed;

			pArrow->SetConfig("velocity", InitialVelocity);
			pArrow->SetConfig("chargePower", ChargePower);
			pArrow->SetConfig("speed", ArrowSpeed);
			pArrow->SetConfig("damage", ArrowDamage);
			pArrow->SetConfig("lifeSpan", pBase->Server()->TickSpeed() * 3);

			// tick
			pArrow->RegisterEvent(CBaseEntity::EventTick, [](CBaseEntity* pArrowBase)
			{
				int& LifeSpan = pArrowBase->GetRefConfig("lifeSpan", 0);
				vec2& Velocity = pArrowBase->GetRefConfig("velocity", vec2(0, 0));
				const auto PrevPos = pArrowBase->GetPos();
				auto NewPos = PrevPos + Velocity;

				Velocity.y += 0.5f;
				pArrowBase->SetPos(NewPos);
				LifeSpan--;
				if(length(Velocity) > 0.0f)
					pArrowBase->SetConfig("direction", normalize(Velocity));

				auto* pTargetChar = pArrowBase->GameWorld()->IntersectCharacter(PrevPos, NewPos, 24.0f, NewPos, pArrowBase->GetCharacter());
				bool Collide = pArrowBase->GS()->Collision()->IntersectLineWithInvisible(PrevPos, NewPos, &NewPos, nullptr);
				if(LifeSpan <= 0 || pArrowBase->GameLayerClipped(NewPos) || Collide || (pTargetChar && pTargetChar->IsAllowedPVP(pArrowBase->GetClientID())))
				{
					const auto Damage = pArrowBase->GetConfig("damage", 0);
					const auto ExplosionRadius = pArrowBase->GetGroup()->GetConfig("explosionRadius", 0.f);
					const auto ExplosionCount = pArrowBase->GetGroup()->GetConfig("explosionCount", 0);
					pArrowBase->GS()->CreateSound(NewPos, SOUND_NINJA_HIT);

					if(pTargetChar && pArrowBase->GetCharacter() && pTargetChar->IsAllowedPVP(pArrowBase->GetClientID()))
						pTargetChar->TakeDamage(normalize(Velocity) * 5.0f, Damage, pArrowBase->GetClientID(), WEAPON_GAME);

					pArrowBase->GS()->CreateRandomRadiusExplosion(ExplosionCount, ExplosionRadius, NewPos, pArrowBase->GetClientID(), WEAPON_GAME, Damage);
					pArrowBase->MarkForDestroy();
					return;
				}
			});

			pArrow->RegisterEvent(CBaseEntity::EventSnap, 1, [](CBaseEntity* pArrowBase, int SnappingClient, const std::vector<int>& vIds)
			{
				if(pArrowBase->NetworkClipped(SnappingClient))
					return;

				const auto Direction = pArrowBase->GetConfig("direction", vec2(1, 0));
				const auto Pos = pArrowBase->GetPos();
				const auto From = Pos - Direction;
				const auto To = Pos - Direction * 96.f;
				pArrowBase->GS()->SnapLaser(SnappingClient, vIds[0], From, To, pArrowBase->Server()->Tick() - 3, LASERTYPE_DRAGGER);
			});
		}

		// end fire count
		if(CurrentFireCount <= 0)
			pBase->MarkForDestroy();
	});

	pBowController->RegisterEvent(CBaseEntity::EventSnap, 7, [MaxChargeTime](CBaseEntity* pBase, int SnappingClient, const std::vector<int>& vIds)
	{
		auto* pOwnerChar = pBase->GetCharacter();
		if(!pOwnerChar || pBase->NetworkClipped(SnappingClient))
			return;

		const auto ChargeTick = pBase->GetConfig("chargeTick", 0);
		const float ChargeProgress = (MaxChargeTime > 0) ? static_cast<float>(ChargeTick) / static_cast<float>(MaxChargeTime) : 0.0f;
		const vec2 Center = pOwnerChar->GetPos();
		const auto AimAngle = angle(vec2(pOwnerChar->m_Core.m_Input.m_TargetX, pOwnerChar->m_Core.m_Input.m_TargetY));
		const auto Tick = pBase->Server()->Tick();

		constexpr float BowWidth = 150.0f;
		constexpr float BowCurveFactor = 30.0f;
		constexpr int NumSegments = 4;

		int id = 0;
		vec2 LastPoint;
		for(int i = 0; i <= NumSegments; i++)
		{
			float p = static_cast<float>(i) / NumSegments;
			float x = -BowWidth / 2.0f + p * BowWidth;
			float y = BowCurveFactor - (4 * BowCurveFactor / (BowWidth * BowWidth)) * x * x;
			const auto Point = Center + rotate(vec2(y, x), AimAngle);
			if(i > 0 && id < (int)vIds.size())
				pBase->GS()->SnapLaser(SnappingClient, vIds[id++], LastPoint, Point, Tick - 1, LASERTYPE_SHOTGUN);

			LastPoint = Point;
		}

		const auto DrawBack = -48.0f * ChargeProgress;
		const auto BowTip1 = Center + rotate(vec2(0.0f, -BowWidth / 2.0f), AimAngle);
		const auto BowTip2 = Center + rotate(vec2(0.0f, BowWidth / 2.0f), AimAngle);
		const auto NockingPoint = Center + rotate(vec2(DrawBack, 0.0f), AimAngle);
		pBase->GS()->SnapLaser(SnappingClient, vIds[id++], BowTip1, NockingPoint, Tick - 1);
		pBase->GS()->SnapLaser(SnappingClient, vIds[id++], BowTip2, NockingPoint, Tick - 1);

		if(ChargeProgress > 0.05f)
		{
			constexpr float ArrowLength = 96.0f;
			const auto ArrowHead = NockingPoint + rotate(vec2(ArrowLength, 0.0f), AimAngle);
			pBase->GS()->SnapLaser(SnappingClient, vIds[id++], ArrowHead, NockingPoint, Tick - 3, LASERTYPE_DRAGGER);
		}
	});

	if(pPtr)
	{
		*pPtr = groupPtr;
	}
}

void CEntityManager::StartUniversalCast(int ClientID, vec2 TargetPosition, int NumRequiredClicks,
	std::function<void(int, vec2, EntGroupWeakPtr*)> ActualSkillExecutionFunc, EntGroupWeakPtr* pCastingProcessTracker) const
{
	// initialize special symbols
	struct FCastingSymbol
	{
		std::vector<vec2> m_vPoints;
	};
	static const std::vector<FCastingSymbol> s_vCastingSymbols = {
		{ {{-0.0f, 0.8f}, {-0.6928f, -0.4f}, {0.6928f, -0.4f}} },
		{ {{-0.6f, -0.6f}, {0.6f, -0.6f}, {0.6f, 0.6f}, {-0.6f, 0.6f}} },
		{ {{0.0f, 0.8f}, {0.5f, 0.0f}, {0.0f, -0.8f}, {-0.5f, 0.0f}} },
		{ {{0.0f, 0.9f}, {0.25f, 0.25f}, {0.9f, 0.0f}, {0.25f, -0.25f}, {0.0f, -0.9f}, {-0.25f, -0.25f}, {-0.9f, 0.0f}, {-0.25f, 0.25f}} },
		{ {{-0.7f, -0.7f}, {0.7f, 0.7f}, {-0.7f, 0.7f}, {0.7f, -0.7f} }},
		{ {{-0.7f, 0.7f}, {0.7f, 0.7f}, {0.7f, 0.2f}, {-0.2f, 0.2f}, {-0.2f, -0.3f}, {0.7f, -0.3f}, {0.7f, -0.8f}, {-0.7f, -0.8f}} },
		{ {{0.0f, 0.8f}, {-0.6f, 0.1f}, {-0.2f, 0.1f}, {0.0f, -0.7f}, {0.2f, 0.1f}, {0.6f, 0.1f}} },
		{ {{-0.7f, 0.6f}, {0.7f, 0.6f}, {0.7f, 0.4f}, {-0.5f, 0.4f}, {-0.5f, 0.0f}, {0.5f, 0.0f}, {0.5f, -0.4f}, {-0.7f, -0.4f}} },
		{ {{-0.2f, 0.7f}, {0.7f, 0.7f}, {0.7f, -0.7f}, {-0.7f, -0.7f}, {-0.7f, 0.2f}, {0.2f, 0.2f}, {0.2f, -0.2f}, {-0.2f, -0.2f} }}
	};
	constexpr float StartCastVisualRadius = 64.0f;
	constexpr float EndCastVisualRadius = 96.0f;
	constexpr int MAX_SYMBOL_SEGMENTS = 8;

	// check player
	const auto* pPlayer = GS()->GetPlayer(ClientID);
	if(!pPlayer || !pPlayer->GetCharacter())
	{
		if(pCastingProcessTracker)
			pCastingProcessTracker->reset();
		return;
	}

	auto pCastingGroup = CEntityGroup::NewGroup(&GS()->m_World, CGameWorld::ENTTYPE_ACTION, ClientID);
	auto pCastingEntity = pCastingGroup->CreateBase(TargetPosition);
	pCastingEntity->SetConfig("currentRequiredClicks", 0);
	pCastingEntity->SetConfig("totalRequiredClicks", std::max(1, NumRequiredClicks));
	pCastingEntity->SetConfig("currentSymbolID", -1);
	pCastingEntity->SetConfig("symbolRotationSpeed", random_float(0.05f, 0.15f) * (rand() % 2 ? 1 : -1));

	if(pCastingProcessTracker)
		*pCastingProcessTracker = pCastingGroup;

	GS()->CreatePlayerSpawn(TargetPosition);
	GS()->Broadcast(ClientID, BroadcastPriority::GameWarning, SERVER_TICK_SPEED,
		"Skill Cast. Requires fire pressing '{}' times.", std::max(1, NumRequiredClicks));
	GS()->CreateSound(TargetPosition, SOUND_SKILL_START);

	// register event tick
	pCastingEntity->RegisterEvent(CBaseEntity::EventTick, [this, ActualSkillExecutionFunc, pCastingProcessTracker](CBaseEntity* pBase)
	{
		// interrupt
		bool Interrupted = false;
		auto* pOwner = pBase->GetPlayer();
		auto* pOwnerChar = pBase->GetCharacter();

		if(distance(pOwnerChar->m_Core.m_Pos, pBase->GetPos()) > 256.0f)
			Interrupted = true;

		if(Interrupted)
		{
			GS()->Chat(pOwner->GetCID(), "Skill channeling interrupted!");
			if(pCastingProcessTracker && pCastingProcessTracker->lock())
				pCastingProcessTracker->reset();
			pBase->MarkForDestroy();
			return;
		}

		// initialize variables
		const int totalClicks = pBase->GetConfig("totalRequiredClicks", 1);
		int& currentClicks = pBase->GetRefConfig("currentRequiredClicks", 0);

		// check status
		if(currentClicks < totalClicks)
		{
			GS()->Server()->Input()->BlockInputGroup(pBase->GetClientID(), BLOCK_INPUT_FIRE);
			if(GS()->Server()->Input()->IsKeyClicked(pBase->GetClientID(), KEY_EVENT_FIRE))
			{
				int NewSymbolID = rand() % s_vCastingSymbols.size();
				if(pBase->GetConfig("currentSymbolID", -1) != -1)
				{
					while(NewSymbolID == pBase->GetConfig("currentSymbolID", 0))
						NewSymbolID = rand() % s_vCastingSymbols.size();
				}

				currentClicks++;
				pBase->SetConfig("currentSymbolID", NewSymbolID);
				GS()->CreatePlayerSpawn(pBase->GetPos());
				GS()->CreateSound(pBase->GetPos(), SOUND_PICKUP_NINJA);
				GS()->Broadcast(pBase->GetClientID(), BroadcastPriority::GameWarning, SERVER_TICK_SPEED,
					"Clicks remaining: '{}'.", std::max(0, totalClicks - currentClicks));

				// create damage star effect
				const auto& SymbolDef = s_vCastingSymbols[NewSymbolID];
				if(!SymbolDef.m_vPoints.empty())
				{
					const auto centerPos = pBase->GetPos();
					for(size_t j = 0; j < SymbolDef.m_vPoints.size(); ++j)
					{
						const auto localPos = SymbolDef.m_vPoints[j];
						vec2 normalized_direction = normalize(localPos);
						const float TargetAngleRad = angle(normalized_direction);
						const float DamageAngleInput = TargetAngleRad - (3.0f * pi / 2.0f) + (pi / 9.0f);
						int TotalClicks = pBase->GetConfig("totalRequiredClicks", 1);
						int CurrentClicks = pBase->GetConfig("currentRequiredClicks", 0);
						float ClickProgress = (TotalClicks > 0) ? (float)CurrentClicks / (float)TotalClicks : 0.0f;
						ClickProgress = std::min(ClickProgress, 1.0f);
						float CurrentSymbolScale = StartCastVisualRadius + (EndCastVisualRadius - StartCastVisualRadius) * ClickProgress;

						GS()->CreateDamage(centerPos - localPos * CurrentSymbolScale, pBase->GetClientID(), 1, DamageAngleInput, -1);
					}
				}
			}
		}

		// finish
		if(currentClicks >= totalClicks)
		{
			if(ActualSkillExecutionFunc)
				ActualSkillExecutionFunc(pOwner->GetCID(), pBase->GetPos(), pCastingProcessTracker);
			pBase->MarkForDestroy();
		}
	});

	// register event snap
	const int NumCastingVisualIDs = MAX_SYMBOL_SEGMENTS;
	pCastingEntity->RegisterEvent(CBaseEntity::EventSnap, NumCastingVisualIDs, [](CBaseEntity* pBase, int SnappingClient, const std::vector<int>& vIds)
	{
		// initialize variables
		int Idx = 0;
		int TotalClicks = pBase->GetConfig("totalRequiredClicks", 1);
		int CurrentClicks = pBase->GetConfig("currentRequiredClicks", 0);
		float ClickProgress = (TotalClicks > 0) ? (float)CurrentClicks / (float)TotalClicks : 0.0f;
		ClickProgress = std::min(ClickProgress, 1.0f);
		int ServerTick = pBase->Server()->Tick();
		int CurrentSymbolID = pBase->GetConfig("currentSymbolID", -1);
		float SymbolRotationSpeed = pBase->GetConfig("symbolRotationSpeed", 0.1f);
		float SymbolBaseRotation = std::fmod(ServerTick * SymbolRotationSpeed, 2.0f * pi);
		float CurrentSymbolScale = StartCastVisualRadius + (EndCastVisualRadius - StartCastVisualRadius) * ClickProgress;

		if(CurrentSymbolID < 0 || CurrentSymbolID >= (int)s_vCastingSymbols.size() || (ClickProgress < 0.01f && CurrentClicks == 0))
			return;

		const auto cos_r = std::cos(SymbolBaseRotation);
		const auto sin_r = std::sin(SymbolBaseRotation);
		const auto& Symbol = s_vCastingSymbols[CurrentSymbolID];
		for(size_t i = 0; i < Symbol.m_vPoints.size() && Idx < (int)vIds.size(); ++i)
		{
			const auto p1_local = Symbol.m_vPoints[i];
			const auto p2_local = Symbol.m_vPoints[(i + 1) % Symbol.m_vPoints.size()];
			const auto p1_rotated = vec2(p1_local.x * cos_r - p1_local.y * sin_r, p1_local.x * sin_r + p1_local.y * cos_r);
			const auto p2_rotated = vec2(p2_local.x * cos_r - p2_local.y * sin_r, p2_local.x * sin_r + p2_local.y * cos_r);
			const auto p1_world = pBase->GetPos() + p1_rotated * CurrentSymbolScale;
			const auto p2_world = pBase->GetPos() + p2_rotated * CurrentSymbolScale;

			int LaserType = LASERTYPE_RIFLE;
			if(ClickProgress > 0.6f)
				LaserType = LASERTYPE_SHOTGUN;
			if(ClickProgress >= 0.95f)
				LaserType = LASERTYPE_DRAGGER;

			pBase->GS()->SnapLaser(SnappingClient, vIds[Idx++], p1_world, p2_world, ServerTick - 1, LaserType, 0, pBase->GetClientID());
		}
	});
}

void CEntityManager::HealingRift(int ClientID, vec2 Position, float RiftRadius, float HealRadius, int Lifetime, float SerpentSpawnInterval,
	int NumSerpentsPerSpawn, int HealAmountPerPulse, int NumOuterSegments, int NumInnerSegments, EntGroupWeakPtr* pPtr) const
{
	const auto* pPlayer = GS()->GetPlayer(ClientID, false, true);
	if(!pPlayer)
		return;

	// initialize group & config
	auto groupPtr = CEntityGroup::NewGroup(&GS()->m_World, CGameWorld::ENTTYPE_SKILL, ClientID);
	groupPtr->SetConfig("riftRadius", RiftRadius);
	groupPtr->SetConfig("healRadius", HealRadius);
	groupPtr->SetConfig("initialLifetimeTicks", Lifetime);
	groupPtr->SetConfig("serpentSpawnIntervalTicks", (int)(SerpentSpawnInterval * Server()->TickSpeed()));
	groupPtr->SetConfig("numSerpentsPerSpawn", NumSerpentsPerSpawn);
	groupPtr->SetConfig("healAmountPerPulse", HealAmountPerPulse);
	groupPtr->SetConfig("numOuterSegments", NumOuterSegments);
	groupPtr->SetConfig("numInnerSegments", NumInnerSegments);
	groupPtr->SetConfig("serpentPulseEffectDurationTicks", Server()->TickSpeed() / 2);
	groupPtr->SetConfig("fadeOutDurationTicks", Server()->TickSpeed());

	// initialize element (rift controller) & config
	auto pRiftController = groupPtr->CreatePickup(Position, POWERUP_ARMOR);
	pRiftController->SetConfig("currentLifetimeTicks", Lifetime);
	pRiftController->SetConfig("serpentCooldownTicks", 0);
	pRiftController->SetConfig("lastSerpentSpawnTick", 0);
	pRiftController->SetConfig("recentSerpentTargets", std::vector<vec2>{});

	// register event tick
	pRiftController->RegisterEvent(CBaseEntity::EventTick, [this](CBaseEntity* pBase)
	{
		// check life time
		int& currentLifetime = pBase->GetRefConfig("currentLifetimeTicks", 0);
		currentLifetime--;
		const int fadeOutDuration = pBase->GetGroup()->GetConfig("fadeOutDurationTicks", pBase->Server()->TickSpeed());
		if(currentLifetime <= 0)
		{
			pBase->GS()->CreateExplosion(pBase->GetPos(), pBase->GetClientID(), WEAPON_GRENADE, 0);
			pBase->GS()->CreateSound(pBase->GetPos(), SOUND_PLAYER_DIE);
			pBase->MarkForDestroy();
			return;
		}

		// initialize variables
		const auto centerPos = pBase->GetPos();
		const auto ServerTick = pBase->Server()->Tick();
		const float riftRadius = pBase->GetGroup()->GetConfig("riftRadius", 100.0f);
		const float healRadius = pBase->GetGroup()->GetConfig("healRadius", 150.0f);
		const int serpentSpawnIntervalTicks = pBase->GetGroup()->GetConfig("serpentSpawnIntervalTicks", 100);
		const int numSerpentsPerSpawn = pBase->GetGroup()->GetConfig("numSerpentsPerSpawn", 1);
		const int healAmountPerPulse = pBase->GetGroup()->GetConfig("healAmountPerPulse", 10);
		const int ownerCID = pBase->GetClientID();
		auto* pOwner = pBase->GetPlayer();
		auto* pOwnerChar = pBase->GetCharacter();

		// pulsing
		const int lastSerpentSpawnTick = pBase->GetConfig("lastSerpentSpawnTick", 0);
		const int serpentPulseEffectDuration = pBase->GetGroup()->GetConfig("serpentPulseEffectDurationTicks", pBase->Server()->TickSpeed() / 2);
		bool isPulsing = ServerTick < lastSerpentSpawnTick + serpentPulseEffectDuration;

		// moving
		if(!isPulsing && currentLifetime > fadeOutDuration)
		{
			auto NewPos = pBase->GetPos();
			vec2 TargetFollowPos = pOwnerChar->m_Core.m_Pos - vec2(0, 48.f);
			vec2 DirToTarget = normalize(TargetFollowPos - NewPos);
			float DistToTarget = distance(TargetFollowPos, NewPos);
			float MoveSpeed = 16.0f;
			if(DistToTarget > 10.0f)
			{
				NewPos += DirToTarget * std::min(MoveSpeed, DistToTarget * 0.1f);
				pBase->SetPos(NewPos);
			}
		}

		// serpent cooldown
		if(currentLifetime > fadeOutDuration)
		{
			int& serpentCooldown = pBase->GetRefConfig("serpentCooldownTicks", 0);
			serpentCooldown--;
			if(serpentCooldown <= 0)
			{
				serpentCooldown = serpentSpawnIntervalTicks;
				pBase->GS()->CreateSound(centerPos, SOUND_NINJA_HIT);
				pBase->SetConfig("lastSerpentSpawnTick", pBase->Server()->Tick());

				// serpent
				std::vector<vec2> currentSerpentTargets;
				std::vector<CCharacter*> potentialTargets;
				const auto totalDamage = pOwner->GetTotalAttributeValue(AttributeIdentifier::DMG);
				const auto vEntities = GS()->m_World.FindEntities(centerPos, riftRadius * 2.0f, 8, CGameWorld::ENTTYPE_CHARACTER);
				for(auto* pEnt : vEntities)
				{
					auto* pTarget = dynamic_cast<CCharacter*>(pEnt);
					if(!pTarget || !pTarget->GetPlayer() || ownerCID == pTarget->GetPlayer()->GetCID() || !pTarget->IsAllowedPVP(ownerCID))
						continue;
					potentialTargets.push_back(pTarget);
				}

				for(int i = 0; i < numSerpentsPerSpawn; ++i)
				{
					const auto serpentTargetPos = !potentialTargets.empty() ?
						potentialTargets[rand() % potentialTargets.size()]->GetPos() : random_range_pos(vec2 {}, 128.f);
					currentSerpentTargets.push_back(serpentTargetPos);
					new CEntityTeslaSerpent(pBase->GameWorld(), ownerCID, centerPos, serpentTargetPos, totalDamage, 500.f, 2, 0.5f);
				}
				pBase->SetConfig("recentSerpentTargets", currentSerpentTargets);

				// healing
				bool ShowRestoreHealth = false;
				for(auto* pChar = (CCharacter*)pBase->GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
				{
					if(!pChar->GetPlayer()) continue;
					if(distance(pChar->GetPos(), centerPos) > healRadius)
						continue;
					if(pChar->GetPlayer()->GetCID() == ownerCID || !pChar->IsAllowedPVP(pBase->GetClientID()))
					{
						ShowRestoreHealth = true;
						new CHeartHealer(pBase->GameWorld(), centerPos, pChar->GetPlayer(), healAmountPerPulse, pChar->m_Core.m_Vel / 2.f);
					}
				}
				if(ShowRestoreHealth)
				{
					pBase->GS()->EntityManager()->Text(centerPos + vec2(0, -96), 40, fmt_default("{}HP", healAmountPerPulse).c_str());
					pBase->GS()->CreateSound(centerPos, SOUND_PICKUP_HEALTH);
				}

				// create hammer effect
				pBase->GS()->CreateHammerHit(centerPos);
			}
		}
	});

	// register event snap
	const int NumIDs = NumOuterSegments + NumInnerSegments + 2;
	pRiftController->RegisterEvent(CBaseEntity::EventSnap, NumIDs, [this](CBaseEntity* pBase, int SnappingClient, const std::vector<int>& vIds)
	{
		// initialize variables
		int currentIdIndex = 0;
		const auto centerPos = pBase->GetPos();
		const auto ServerTick = pBase->Server()->Tick();
		const float initialRiftRadius = pBase->GetGroup()->GetConfig("riftRadius", 100.0f);
		const int numOuterSegments = pBase->GetGroup()->GetConfig("numOuterSegments", 12);
		const int numInnerSegments = pBase->GetGroup()->GetConfig("numInnerSegments", 8);

		const int initialLifetime = pBase->GetGroup()->GetConfig("initialLifetimeTicks", pBase->Server()->TickSpeed() * 10);
		const int currentLifetime = pBase->GetConfig("currentLifetimeTicks", 0);
		const int fadeOutDuration = pBase->GetGroup()->GetConfig("fadeOutDurationTicks", pBase->Server()->TickSpeed());

		float fadeProgress = 1.0f;
		bool isFadingOut = false;

		if(currentLifetime < fadeOutDuration)
		{
			isFadingOut = true;
			fadeProgress = (float)currentLifetime / (float)std::max(1, fadeOutDuration);
		}

		float currentVisualRadius = initialRiftRadius * fadeProgress;
		if(currentVisualRadius < 1.0f && isFadingOut)
			return;

		const int lastSerpentSpawnTick = pBase->GetConfig("lastSerpentSpawnTick", 0);
		const int serpentPulseEffectDuration = pBase->GetGroup()->GetConfig("serpentPulseEffectDurationTicks", pBase->Server()->TickSpeed() / 2);
		bool isPulsing = ServerTick < lastSerpentSpawnTick + serpentPulseEffectDuration;

		// outer ring
		float OuterAngleStep = 2.0f * pi / std::max(1, numOuterSegments);
		float OuterRotationPhase = std::fmod(ServerTick * 0.02f, 2.0f * pi);
		float TimeParamOuter = (float)ServerTick / (float)SERVER_TICK_SPEED;
		float OuterRadiusModulation = currentVisualRadius * (isPulsing ? 0.15f : 0.05f);
		float ModulatedOuterRadius = currentVisualRadius + std::sin(TimeParamOuter * pi * 2.0f * (isPulsing ? 1.0f : 0.3f)) * OuterRadiusModulation;
		ModulatedOuterRadius = std::max(0.0f, ModulatedOuterRadius);

		for(int i = 0; i < numOuterSegments && currentIdIndex < (int)vIds.size(); ++i)
		{
			const auto p1 = centerPos + vec2(std::cos(OuterAngleStep * i + OuterRotationPhase), std::sin(OuterAngleStep * i + OuterRotationPhase)) * ModulatedOuterRadius;
			const auto p2 = centerPos + vec2(std::cos(OuterAngleStep * (i + 1) + OuterRotationPhase), std::sin(OuterAngleStep * (i + 1) + OuterRotationPhase)) * ModulatedOuterRadius;
			int laserTypeOuter = isFadingOut ? LASERTYPE_RIFLE : LASERTYPE_SHOTGUN;
			int laserSubtypeOuter = (isPulsing && !isFadingOut) ? 1 : 0;
			pBase->GS()->SnapLaser(SnappingClient, vIds[currentIdIndex++], p1, p2, ServerTick - 1, laserTypeOuter, laserSubtypeOuter, pBase->GetClientID());
		}

		// inner ring
		float InnerAngleStep = 2.0f * pi / std::max(1, numInnerSegments);
		float InnerRotationPhase = std::fmod(ServerTick * -0.05f, 2.0f * pi);
		float ModulatedInnerRadius = ModulatedOuterRadius * (isPulsing ? 0.4f : 0.6f);
		ModulatedInnerRadius = std::max(0.0f, ModulatedInnerRadius * fadeProgress);
		for(int i = 0; i < numInnerSegments && currentIdIndex < (int)vIds.size(); ++i)
		{
			const auto p1 = centerPos + vec2(std::cos(InnerAngleStep * i + InnerRotationPhase), std::sin(InnerAngleStep * i + InnerRotationPhase)) * ModulatedInnerRadius;
			const auto p2 = centerPos + vec2(std::cos(InnerAngleStep * (i + 1) + InnerRotationPhase), std::sin(InnerAngleStep * (i + 1) + InnerRotationPhase)) * ModulatedInnerRadius;
			int laserSubtypeInner = (isPulsing && !isFadingOut) ? 1 : 0;
			pBase->GS()->SnapLaser(SnappingClient, vIds[currentIdIndex++], p1, p2, ServerTick - 1, LASERTYPE_RIFLE, laserSubtypeInner, pBase->GetClientID());
		}

		// clock lines
		if((!isFadingOut || fadeProgress > 0.1f) && ModulatedOuterRadius > 1.0f)
		{
			float lifetimeProgressRatio = 0.0f;
			if(initialLifetime > 0)
				lifetimeProgressRatio = std::clamp(((float)initialLifetime - (float)currentLifetime) / (float)initialLifetime, 0.0f, 1.0f);

			float minuteHandAngle = -pi / 2.0f + lifetimeProgressRatio * (2.0f * pi);
			float minuteHandLength = ModulatedOuterRadius * 0.75f;
			vec2 minuteHandEndPos = centerPos + vec2(std::cos(minuteHandAngle) * minuteHandLength, std::sin(minuteHandAngle) * minuteHandLength);
			if(currentIdIndex < (int)vIds.size() && minuteHandLength > 1.0f)
				pBase->GS()->SnapLaser(SnappingClient, vIds[currentIdIndex++], minuteHandEndPos, centerPos, ServerTick - 1, LASERTYPE_DOOR, 0, pBase->GetClientID());

			float hourHandAngle = -pi / 2.0f + lifetimeProgressRatio * (2.0f * pi / 6.0f);
			float hourHandLength = ModulatedOuterRadius * 0.45f;
			vec2 hourHandEndPos = centerPos + vec2(std::cos(hourHandAngle) * hourHandLength, std::sin(hourHandAngle) * hourHandLength);
			if(currentIdIndex < (int)vIds.size() && hourHandLength > 1.0f)
				pBase->GS()->SnapLaser(SnappingClient, vIds[currentIdIndex++], hourHandEndPos, centerPos, ServerTick - 1, LASERTYPE_DOOR, 0, pBase->GetClientID());
		}

		// pulsing effect's
		if(isPulsing && fadeProgress > 0.5f)
		{
			const int numPulseEffects = 2;
			for(int i = 0; i < numPulseEffects; ++i)
			{
				float randomAngle = random_float(0.0f, 2.0f * pi);
				pBase->GS()->CreateDamage(centerPos, pBase->GetClientID(), 1, randomAngle, CmaskOne(SnappingClient));
			}
		}
	});

	if(pPtr)
	{
		*pPtr = groupPtr;
	}
}


void CEntityManager::EffectCircleDamage(int ClientID, int DelayImpulse, int DelayBetweenImpulses, int Repeat) const
{
	const auto* pPlayer = GS()->GetPlayer(ClientID, false, true);
	if(!pPlayer)
		return;

	// initialize group
	const auto groupPtr = CEntityGroup::NewGroup(&GS()->m_World, CGameWorld::ENTTYPE_VISUAL, ClientID);
	groupPtr->SetConfig("delayImpulse", DelayImpulse);
	groupPtr->SetConfig("delayBetweenImpulses", DelayBetweenImpulses);

	// initialize effect
	const auto pEffect = groupPtr->CreateBase(pPlayer->GetCharacter()->GetPos());
	pEffect->SetConfig("currentImpulseTick", DelayImpulse);
	pEffect->SetConfig("currentAngle", 0.f);
	pEffect->SetConfig("remainingRepeats", Repeat);
	pEffect->SetConfig("currentDelayTick", 0);

	// register tick event
	pEffect->RegisterEvent(CBaseEntity::EventTick, [](CBaseEntity* pBase)
	{
		constexpr float angleIncrement = 1.f;
		constexpr float maxAngle = 256.f;

		// delay tick
		auto& delayTick = pBase->GetRefConfig("currentDelayTick", 0);
		if(delayTick > 0)
		{
			--delayTick;
			return;
		}

		// variables
		auto& currentImpulseTick = pBase->GetRefConfig("currentImpulseTick", 0);
		auto& currentAngle = pBase->GetRefConfig("currentAngle", 0.f);

		// is impulse active
		if(currentImpulseTick > 0)
		{
			currentAngle = fmod(currentAngle + angleIncrement, maxAngle);
			--currentImpulseTick;

			pBase->GS()->CreateDamage(pBase->GetCharacter()->GetPos(), pBase->GetClientID(), 1, false, currentAngle);
			return;
		}

		// repeats
		auto& remainingRepeats = pBase->GetRefConfig("remainingRepeats", 0);
		if(remainingRepeats > 0 || remainingRepeats == -1)
		{
			remainingRepeats = maximum(remainingRepeats - 1, -1);
			currentImpulseTick = pBase->GetGroup()->GetConfig("delayImpulse", 0);
			delayTick = pBase->GetGroup()->GetConfig("delayBetweenImpulses", 0);
			return;
		}

		pBase->MarkForDestroy();
	});
}