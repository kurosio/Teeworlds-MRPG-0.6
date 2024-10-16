/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "entity_manager.h"
#include "gamecontext.h"

#include "entities/projectile.h"
#include "core/entities/group/entitiy_group.h"
#include "core/entities/items/drop_bonuses.h"
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

void CEntityManager::DropBonus(vec2 Pos, int Type, int Subtype, int Value, int NumDrop, vec2 Force) const
{
	for(int i = 0; i < NumDrop; i++)
	{
		vec2 Vel = Force;
		Vel.x += random_float(15.0f);
		Vel.y += random_float(15.0f);
		new CEntityDropBonuses(&GS()->m_World, Pos, Vel, Type, Subtype, Value);
	}
}

void CEntityManager::DropItem(vec2 Pos, int ClientID, CItem Item, vec2 Force) const
{
	if(Item.IsValid())
	{
		const float Angle = angle(normalize(Force));
		new CDropItem(&GS()->m_World, Pos, Force, Angle, Item, ClientID);
	}
}

void CEntityManager::RandomDropItem(vec2 Pos, int ClientID, float Chance, CItem Item, vec2 Force) const
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

void CEntityManager::GravityDisruption(int ClientID, vec2 Position, float Radius, int Lifetime, int Damage, std::weak_ptr<CEntityGroup>* pPtr) const
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

			if(!pBase->GS()->SnapProjectile(SnappingClient, vIds[i], VertexPos, {}, pBase->Server()->Tick() - 1, WEAPON_HAMMER))
				return;
		}
	});

	if(pPtr)
		*pPtr = groupPtr;
}

void CEntityManager::HealthTurret(int ClientID, vec2 Position, int RestoreHealth, int Lifetime, int InitialReloadTick, std::weak_ptr<CEntityGroup>* pPtr) const
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
			if(!pBase->GS()->SnapPickup(SnappingClient, vIds[i], VertexPos, POWERUP_HEALTH))
				return;
		}
	});

	if(pPtr)
		*pPtr = groupPtr;
}

void CEntityManager::EnergyShield(int ClientID, vec2 Position, int Health, std::weak_ptr<CEntityGroup>* pPtr) const
{
	std::vector<vec2> vShieldEdges = {
		{-144.0f, 0.0f}, {-96.0f, -48.0f}, {-48.0f, -96.0f},
		{48.0f, -96.0f}, {96.0f, -48.0f}, {144.0f, 0.0f}
	};

	// initialize group & config
	auto groupPtr = CEntityGroup::NewGroup(&GS()->m_World, CGameWorld::ENTTYPE_SKILL, ClientID);
	groupPtr->SetConfig("health", Health);

	// initialize elements
	for(int i = 0; i < (int)vShieldEdges.size(); i++)
	{
		vec2 startPos = vShieldEdges[i];
		vec2 endPos = vShieldEdges[(i + 1) % vShieldEdges.size()];

		const auto pLaser = groupPtr->CreateLaser(Position + startPos, Position + endPos, LASERTYPE_SHOTGUN);
		pLaser->GetOptions().LaserSubtype = LASERDRAGGERTYPE_NORMAL;
		pLaser->GetOptions().StartTickShift = 3;

		// register event tick
		pLaser->RegisterEvent(CBaseEntity::EventTick, [startPos, endPos](CBaseEntity* pBase)
		{
			int& Health = pBase->GetGroup()->GetRefConfig("health", 0);

			// health
			if(Health <= 0)
			{
				pBase->GS()->CreateExplosion(pBase->GetPos(), pBase->GetClientID(), WEAPON_WORLD, 3);
				pBase->GS()->CreateExplosion(pBase->GetPosTo(), pBase->GetClientID(), WEAPON_WORLD, 3);
				pBase->MarkForDestroy();
				return;
			}

			// knock back characters
			for(auto* pChar = (CCharacter*)pBase->GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
			{
				if(pChar->GetPlayer()->GetCID() != pBase->GetClientID() && pChar->IsAllowedPVP(pBase->GetClientID()) &&
					is_within_distance_on_line(48.0f, pBase->GetPos(), pBase->GetPosTo(), pChar->GetPos()))
				{
					vec2 knockBackDir = normalize(pChar->GetPos() - pBase->GetPos());
					pChar->m_Core.m_Vel += knockBackDir * 12.0f;
				}
			}

			// destroy projectiles
			for(auto* pProj = (CProjectile*)pBase->GameWorld()->FindFirst(CGameWorld::ENTTYPE_PROJECTILE); pProj; pProj = (CProjectile*)pProj->TypeNext())
			{
				if(pProj->GetOwner() != pBase->GetPlayer()->GetCID() &&
					is_within_distance_on_line(48.0f, pBase->GetPos(), pBase->GetPosTo(), pProj->GetCurrentPos()))
				{
					Health -= pProj->GetDamage();
					pBase->GS()->CreateHammerHit(pProj->GetCurrentPos());
					pProj->MarkForDestroy();
				}
			}

			// update position
			const auto* pChar = pBase->GetCharacter();
			const float angle = std::atan2(pChar->m_Core.m_Input.m_TargetY, pChar->m_Core.m_Input.m_TargetX);
			pBase->SetPos(rotate(startPos, pChar->GetPos(), angle));
			pBase->SetPosTo(rotate(endPos, pChar->GetPos(), angle));
		});

		// register event snap
		pLaser->RegisterEvent(CBaseEntity::EventSnap, 1, [edgePos = vShieldEdges[i]](const CBaseEntity* pBase, int SnappingClient, const std::vector<int>& vIDs)
		{
			const auto* pChar = pBase->GetCharacter();
			const float angle = std::atan2(pChar->m_Core.m_Input.m_TargetY, pChar->m_Core.m_Input.m_TargetX);
			const vec2 curPos = rotate(edgePos, pChar->GetPos(), angle);
			if(!pBase->GS()->SnapPickup(SnappingClient, vIDs[0], curPos, POWERUP_ARMOR))
				return;
		});
	}

	if(pPtr)
		*pPtr = groupPtr;
}

void CEntityManager::FlameWall(int ClientID, vec2 Position, float Radius, int Lifetime, int DamagePerTick, float SlowDownFactor, std::weak_ptr<CEntityGroup>* pPtr) const
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
			if(!pBase->GS()->SnapProjectile(SnappingClient, vIds[i], VertexPos, {}, pBase->Server()->Tick() - 1, WEAPON_GRENADE))
				return;
		}

		// create random projectiles inside the circle
		for(int i = 0; i < NUM_PROJ_INSIDE; ++i)
		{
			vec2 VertexPos = random_range_pos(pBase->GetPos(), Radius);
			if(!pBase->GS()->SnapProjectile(SnappingClient, vIds[NUM_CIRCLE + i], VertexPos, {}, pBase->Server()->Tick() - 1, WEAPON_HAMMER))
				return;
		}
	});

	if(pPtr)
		*pPtr = groupPtr;
}

void CEntityManager::FrostNova(int ClientID, vec2 Position, float Radius, int Damage, int FreezeTime, std::weak_ptr<CEntityGroup>* pPtr) const
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
			if(!pBase->GS()->SnapPickup(SnappingClient, vIds[i], VertexPos, POWERUP_ARMOR))
				return;
		}
	});

	if(pPtr)
		*pPtr = groupPtr;
}

void CEntityManager::HealingAura(int ClientID, vec2 Position, float Radius, int Lifetime, int HealPerTick, std::weak_ptr<CEntityGroup>* pPtr) const
{
	enum
	{
		NUM_CIRCLES = 6,
		NUM_HEART = 8,
		NUM_IDS = (NUM_CIRCLES * 2) + NUM_HEART
	};

	// initialize group & config
	auto groupPtr = CEntityGroup::NewGroup(&GS()->m_World, CGameWorld::ENTTYPE_SKILL, ClientID);
	groupPtr->SetConfig("radius", Radius);

	// initialize element & config
	auto pPickup = groupPtr->CreatePickup(Position, POWERUP_ARMOR);
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
		int& Lifetime = pBase->GetRefConfig("lifetimeTick", 0);
		if(Lifetime <= 0)
		{
			pBase->MarkForDestroy();
			return;
		}
		Lifetime--;

		// variables
		const float Radius = pBase->GetGroup()->GetConfig("radius", 0.f);
		const int HealPerTick = pBase->GetConfig("healPerTick", 0);
		const int TickSpeed = pBase->Server()->TickSpeed();

		// healing players
		if(pBase->Server()->Tick() % TickSpeed == 0)
		{
			for(auto* pChar = (CCharacter*)pBase->GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
			{
				if(distance(pBase->GetPos(), pChar->m_Core.m_Pos) > Radius)
					continue;

				if(pBase->GetClientID() == pChar->GetPlayer()->GetCID() || pChar->IsAllowedPVP(pBase->GetClientID()))
				{
					pChar->IncreaseHealth(HealPerTick);
				}
			}
		}

		// update heart positions and velocities
		std::vector<vec2>& heartPositions = pBase->GetRefConfig("heartPositions", std::vector<vec2>{});
		std::vector<vec2>& heartVelocities = pBase->GetRefConfig("heartVelocities", std::vector<vec2>{});

		for(int i = 0; i < NUM_HEART; ++i)
		{
			vec2& pos = heartPositions[i];
			vec2& vel = heartVelocities[i];

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
		const float Radius = pBase->GetGroup()->GetConfig("radius", 0.f);
		constexpr float AngleStep = 2.0f * pi / static_cast<float>(NUM_CIRCLES);

		// snap cyrcle
		for(int i = 0; i < NUM_CIRCLES; ++i)
		{
			vec2 VertexPos = pBase->GetPos() + vec2(Radius * cos(AngleStep * i), Radius * sin(AngleStep * i));
			if(!pBase->GS()->SnapPickup(SnappingClient, vIds[i], VertexPos, POWERUP_HEALTH))
				return;
		}

		// snap cyrcle connect
		for(int i = 0; i < NUM_CIRCLES; ++i)
		{
			int nextIndex = (i + 1) % NUM_CIRCLES;
			vec2 CurrentPos = pBase->GetPos() + vec2(Radius * cos(AngleStep * i), Radius * sin(AngleStep * i));
			vec2 NextPos = pBase->GetPos() + vec2(Radius * cos(AngleStep * nextIndex), Radius * sin(AngleStep * nextIndex));

			if(!pBase->GS()->SnapLaser(SnappingClient, vIds[NUM_CIRCLES + i], CurrentPos, NextPos, pBase->Server()->Tick() - 1))
				return;
		}

		// snap hearts inside cyrcle
		const std::vector<vec2>& heartPositions = pBase->GetConfig("heartPositions", std::vector<vec2>{});
		for(int i = 0; i < NUM_HEART; ++i)
		{
			vec2 InnerPos = heartPositions[i];
			if(!pBase->GS()->SnapPickup(SnappingClient, vIds[NUM_CIRCLES * 2 + i], InnerPos, POWERUP_HEALTH))
				return;
		}
	});

	if(pPtr)
		*pPtr = groupPtr;
}

void CEntityManager::Bow(int ClientID, int Damage, int FireCount, float ExplosionRadius, int ExplosionCount, std::weak_ptr<CEntityGroup>* pPtr) const
{
	CPlayer* pPlayer = GS()->GetPlayer(ClientID, false, true);
	if(!pPlayer)
		return;

	// initialize group & config
	auto groupPtr = CEntityGroup::NewGroup(&GS()->m_World, CGameWorld::ENTTYPE_SKILL, ClientID);
	groupPtr->SetConfig("damage", Damage);
	groupPtr->SetConfig("fireCount", FireCount);
	groupPtr->SetConfig("explosionRadius", ExplosionRadius);
	groupPtr->SetConfig("explosionCount", ExplosionCount);

	// initialize element & config
	auto pBow = groupPtr->CreatePickup(pPlayer->GetCharacter()->GetPos());

	// register event tick
	pBow->RegisterEvent(CBaseEntity::EventTick, [](CBaseEntity* pBase)
	{
		int& FireCount = pBase->GetGroup()->GetRefConfig("fireCount", 0);

		// freeze input for bow
		pBase->Server()->Input()->BlockInputGroup(pBase->GetClientID(), BLOCK_INPUT_FIRE);
		pBase->Server()->Input()->BlockInputGroup(pBase->GetClientID(), BLOCK_INPUT_FREEZE_GUN);

		// check is key clicked
		if(pBase->Server()->Input()->IsKeyClicked(pBase->GetClientID(), KEY_EVENT_FIRE))
		{
			// create fire
			vec2 Direction = normalize(vec2(pBase->GetCharacter()->m_Core.m_Input.m_TargetX, pBase->GetCharacter()->m_Core.m_Input.m_TargetY));
			const auto pArrow = pBase->GetGroup()->CreatePickup(pBase->GetCharacter()->GetPos());
			pArrow->SetConfig("direction", Direction);
			FireCount--;

			// register event tick
			pArrow->RegisterEvent(CBaseEntity::EventTick, [](CBaseEntity* pBase)
			{
				const float ExplosionRadius = pBase->GetGroup()->GetConfig("explosionRadius", 0.f);
				const int ExplosionCount = pBase->GetGroup()->GetConfig("explosionCount", 0);
				const int Damage = pBase->GetGroup()->GetConfig("damage", 0);
				vec2 Direction = pBase->GetConfig("direction", vec2());

				for(auto* pChar = (CCharacter*)pBase->GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
				{
					if(pBase->GetClientID() == pChar->GetPlayer()->GetCID() || !pChar->IsAllowedPVP(pBase->GetClientID()))
						continue;

					float Distance = distance(pBase->GetPos(), pChar->m_Core.m_Pos);
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
				vec2 Direction = pBase->GetConfig("direction", vec2());
				vec2 Pos = pBase->GetPos();

				for(int i = 0; i < (int)vIds.size(); ++i)
				{
					vec2 SegmentPos = Pos - Direction * 24.f * i;
					if(!pBase->GS()->SnapLaser(SnappingClient, vIds[i], SegmentPos, Pos, pBase->Server()->Tick() - 4))
						return;
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
		float angle = std::atan2(pBase->GetCharacter()->m_Core.m_Input.m_TargetY, pBase->GetCharacter()->m_Core.m_Input.m_TargetX);
		pBase->SetPos(rotate(vec2(0.f, -56.f), pBase->GetCharacter()->GetPos(), angle));
	});

	std::vector<vec2> vArrowEdges = {
		{-60.0f, 0.0f}, {-40.0f, -20.0f}, {-20.0f, -40.0f},
		{20.0f, -40.0f}, {40.0f, -20.0f}, {60.0f, 0.0f}
	};

	// Register event snap
	pBow->RegisterEvent(CBaseEntity::EventSnap, (int)vArrowEdges.size() + 1, [vEdges = vArrowEdges](CBaseEntity* pBase, int SnappingClient, const std::vector<int>& vIds)
	{
		const auto* pChar = pBase->GetCharacter();
		float angle = std::atan2(pChar->m_Core.m_Input.m_TargetY, pChar->m_Core.m_Input.m_TargetX);

		vec2 firstPos = vEdges.back();
		vec2 endPos = vEdges.front();
		vec2 Pos = rotate(firstPos, pChar->GetPos(), angle);
		vec2 PosTo = rotate(endPos, pChar->GetPos(), angle);

		if(!pBase->GS()->SnapLaser(SnappingClient, vIds[0], Pos, PosTo, pBase->Server()->Tick() - 3, LASERTYPE_SHOTGUN, 0, pBase->GetClientID()))
			return;

		for(size_t i = 0; i < vEdges.size(); ++i)
		{
			vec2 curPos = rotate(vEdges[i], pChar->GetPos(), angle);

			if(!pBase->GS()->SnapPickup(SnappingClient, vIds[1 + i], curPos, POWERUP_ARMOR))
				return;
		}
	});

	if(pPtr)
		*pPtr = groupPtr;
}

void CEntityManager::EffectCircleDamage(int ClientID, int DelayImpulse, int DelayBetweenImpulses, int Repeat) const
{
	CPlayer* pPlayer = GS()->GetPlayer(ClientID, false, true);
	if(!pPlayer)
		return;

	// initialize group
	auto groupPtr = CEntityGroup::NewGroup(&GS()->m_World, CGameWorld::ENTTYPE_VISUAL, ClientID);
	groupPtr->SetConfig("delayImpulse", DelayImpulse);
	groupPtr->SetConfig("delayBetweenImpulses", DelayBetweenImpulses);

	// initialize effect
	auto pEffect = groupPtr->CreateBase(pPlayer->GetCharacter()->GetPos());
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