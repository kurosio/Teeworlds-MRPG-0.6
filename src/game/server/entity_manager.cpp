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
			pBase->GS()->CreateCyrcleExplosion(12, Radius, BasePos, pBase->GetClientID(), WEAPON_GRENADE, Damage);
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
				pChar->m_Core.m_Vel -= Dir * 1.50f;
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

void CEntityManager::LastStand(int ClientID, vec2 Position, float Radius, int ManaCostPerSec, EntGroupWeakPtr* pPtr) const
{
	// initialize
	const auto groupPtr = CEntityGroup::NewGroup(&GS()->m_World, CGameWorld::ENTTYPE_SKILL, ClientID);
	const auto pBase = groupPtr->CreateBase(Position);
	groupPtr->SetConfig("radius", Radius);
	groupPtr->SetConfig("manaCostPerSec", ManaCostPerSec);

	// register event tick
	pBase->RegisterEvent(CBaseEntity::EventTick, [](CBaseEntity* pBase)
	{
		auto* pChar = pBase->GetCharacter();
		const auto ManaCostPerSec = pBase->GetGroup()->GetConfig("manaCostPerSec", 0);

		// action
		if(pBase->Server()->Tick() % pBase->Server()->TickSpeed() == 0)
		{
			if(!pChar->TryUseMana(ManaCostPerSec))
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
			pBase->GS()->CreateRandomRadiusExplosion(2, Radius, pBase->GetPos(), pBase->GetClientID(), WEAPON_NINJA, DamagePerTick);

		// damage and slowdown enemies
		for(auto* pChar = (CCharacter*)pBase->GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
		{
			if(distance(pBase->GetPos(), pChar->m_Core.m_Pos) > Radius)
				continue;

			if(!pBase->GetPlayer() || (pBase->GetClientID() != pChar->GetPlayer()->GetCID() && pChar->IsAllowedPVP(pBase->GetClientID())))
			{
				if(pBase->Server()->Tick() % TickSpeed == 0)
					pChar->TakeDamage(vec2(0, 0), DamagePerTick, pBase->GetClientID(), WEAPON_WORLD);

				pChar->m_Core.m_Vel *= SlowDownFactor;
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
				pChar->TakeDamage(vec2(0, 0), Damage, pBase->GetClientID(), WEAPON_WORLD);
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
	if(!pPlayer)
		return;

	// initialize group & config
	const auto groupPtr = CEntityGroup::NewGroup(&GS()->m_World, CGameWorld::ENTTYPE_SKILL, ClientID);
	groupPtr->SetConfig("damage", Damage);
	groupPtr->SetConfig("fireCount", FireCount);
	groupPtr->SetConfig("explosionRadius", ExplosionRadius);
	groupPtr->SetConfig("explosionCount", ExplosionCount);

	// initialize element & config
	const auto pBow = groupPtr->CreatePickup(pPlayer->GetCharacter()->GetPos());

	// register event tick
	pBow->RegisterEvent(CBaseEntity::EventTick, [](CBaseEntity* pBase)
	{
		auto& FireCount = pBase->GetGroup()->GetRefConfig("fireCount", 0);

		// freeze input for bow
		pBase->Server()->Input()->BlockInputGroup(pBase->GetClientID(), BLOCK_INPUT_FIRE);
		pBase->Server()->Input()->BlockInputGroup(pBase->GetClientID(), BLOCK_INPUT_FREEZE_GUN);

		// check is key clicked
		if(pBase->Server()->Input()->IsKeyClicked(pBase->GetClientID(), KEY_EVENT_FIRE))
		{
			// create fire
			const auto Direction = normalize(vec2(pBase->GetCharacter()->m_Core.m_Input.m_TargetX, pBase->GetCharacter()->m_Core.m_Input.m_TargetY));
			const auto pArrow = pBase->GetGroup()->CreatePickup(pBase->GetCharacter()->GetPos());
			pArrow->SetConfig("direction", Direction);
			FireCount--;

			// register event tick
			pArrow->RegisterEvent(CBaseEntity::EventTick, [](CBaseEntity* pBase)
			{
				const auto ExplosionRadius = pBase->GetGroup()->GetConfig("explosionRadius", 0.f);
				const auto ExplosionCount = pBase->GetGroup()->GetConfig("explosionCount", 0);
				const auto Damage = pBase->GetGroup()->GetConfig("damage", 0);
				auto Direction = pBase->GetConfig("direction", vec2());

				for(auto* pChar = (CCharacter*)pBase->GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
				{
					if(pBase->GetClientID() == pChar->GetPlayer()->GetCID() || !pChar->IsAllowedPVP(pBase->GetClientID()))
						continue;

					// check distance
					const auto Distance = distance(pBase->GetPos(), pChar->m_Core.m_Pos);
					if(Distance < 64.f)
					{
						pChar->TakeDamage(Direction, Damage, pBase->GetClientID(), WEAPON_GRENADE);
						pBase->GS()->CreateRandomRadiusExplosion(ExplosionCount, ExplosionRadius, pBase->GetPos(), pBase->GetClientID(), WEAPON_NINJA, Damage);
						pBase->MarkForDestroy();
						return;
					}

					if(Distance < 300.0f)
					{
						vec2 ToEnemy = normalize(pChar->m_Core.m_Pos - pBase->GetPos());
						Direction = normalize(Direction + ToEnemy * 0.05f);
						pBase->SetConfig("direction", Direction);
					}
				}

				// check collide
				if(pBase->GS()->Collision()->CheckPoint(pBase->GetPos()))
				{
					pBase->GS()->CreateRandomRadiusExplosion(ExplosionCount, ExplosionRadius, pBase->GetPos(), pBase->GetClientID(), WEAPON_NINJA, Damage);
					pBase->MarkForDestroy();
					return;
				}

				// update position
				pBase->SetPos(pBase->GetPos() + Direction * 18.0f);
			});

			// Register event snap for the fire projectile (drawing the arrow)
			pArrow->RegisterEvent(CBaseEntity::EventSnap, 3, [](CBaseEntity* pBase, int SnappingClient, const std::vector<int>& vIds)
			{
				const auto Direction = pBase->GetConfig("direction", vec2());
				auto Pos = pBase->GetPos();

				for(int i = 0; i < (int)vIds.size(); ++i)
				{
					vec2 SegmentPos = Pos - Direction * 24.f * i;
					pBase->GS()->SnapLaser(SnappingClient, vIds[i], SegmentPos, Pos, pBase->Server()->Tick() - 4);
					Pos = SegmentPos;
				}
			});

			// fire count
			if(!FireCount)
			{
				pBase->MarkForDestroy();
				return;
			}
		}

		// update position
		const auto Angle = std::atan2(pBase->GetCharacter()->m_Core.m_Input.m_TargetY, pBase->GetCharacter()->m_Core.m_Input.m_TargetX);
		pBase->SetPos(rotate(vec2(0.f, -56.f), pBase->GetCharacter()->GetPos(), Angle));
	});

	// Register event snap
	std::vector<vec2> vArrowEdges =
	{
		{-60.0f, 0.0f}, {-40.0f, -20.0f}, {-20.0f, -40.0f},
		{20.0f, -40.0f}, {40.0f, -20.0f}, {60.0f, 0.0f}
	};

	pBow->RegisterEvent(CBaseEntity::EventSnap, (int)vArrowEdges.size() + 1, [vEdges = vArrowEdges](CBaseEntity* pBase, int SnappingClient, const std::vector<int>& vIds)
	{
		const auto* pChar = pBase->GetCharacter();
		const auto Angle = std::atan2(pChar->m_Core.m_Input.m_TargetY, pChar->m_Core.m_Input.m_TargetX);
		const auto firstPos = vEdges.back();
		const auto endPos = vEdges.front();
		const auto Pos = rotate(firstPos, pChar->GetPos(), Angle);
		const auto PosTo = rotate(endPos, pChar->GetPos(), Angle);
		pBase->GS()->SnapLaser(SnappingClient, vIds[0], Pos, PosTo, pBase->Server()->Tick() - 3, LASERTYPE_SHOTGUN, 0, pBase->GetClientID());

		for(size_t i = 0; i < vEdges.size(); ++i)
		{
			vec2 curPos = rotate(vEdges[i], pChar->GetPos(), Angle);
			pBase->GS()->SnapPickup(SnappingClient, vIds[1 + i], curPos, POWERUP_ARMOR);
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