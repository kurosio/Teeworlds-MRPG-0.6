/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "entity_manager.h"
#include "event_key_manager.h"
#include "gamecontext.h"

#include "entities/projectile.h"
#include "core/entities/event/entitiy_group.h"
#include "core/entities/items/drop_bonuses.h"
#include "core/entities/items/drop_items.h"
#include "core/entities/tools/flying_point.h"
#include "core/entities/tools/laser_orbite.h"
#include "core/entities/tools/loltext.h"

#include "core/components/skills/entities/heart_healer.h"
#include "core/entities/event/laser_entity.h"

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
	if(!GS()->IsPlayersNearby(Pos, 800))
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
	if(!pParent || !GS()->IsPlayersNearby(pParent->GetPos(), 800))
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
	auto groupPtr = CEntityGroup::NewGroup(&GS()->m_World, ClientID);
	groupPtr->SetConfig("radius", Radius);

	// initialize element & config
	auto pCore = groupPtr->CreatePickup(Position, POWERUP_ARMOR);
	pCore->SetConfig("lifetimeTick", Lifetime);
	pCore->SetConfig("damage", Damage);

	// register event tick
	pCore->RegisterEvent(CBaseEntity::EventTick, [](CBaseEntity* pBase)
	{
		// lifetime
		int& LifeTime = pBase->GetRefConfig("lifetimeTick", 0);
		if(LifeTime <= 0)
		{
			pBase->MarkForDestroy();
			return;
		}
		LifeTime--;

		// new damage
		const float Radius = pBase->GetGroup()->GetConfig("radius", 0.f);
		const int TimeLeft = LifeTime / pBase->Server()->TickSpeed();
		if(TimeLeft < 3)
		{
			const int Damage = pBase->GetConfig("damage", 0);
			for(int i = 0; i < (int)pBase->GetIDs().size(); i++)
			{
				const float AngleStep = 2.0f * pi / (float)pBase->GetIDs().size();
				vec2 VertexPos = pBase->GetPos() + vec2(Radius * cos(AngleStep * i), Radius * sin(AngleStep * i));

				if(!LifeTime)
				{
					pBase->GS()->CreateExplosion(VertexPos, pBase->GetClientID(), WEAPON_GRENADE, Damage);
				}
				else if(LifeTime && TimeLeft == i)
				{
					pBase->GS()->CreateDamage(pBase->GetPos(), pBase->GetClientID(), 1, false, AngleStep);
					break;
				}
			}

			if(!LifeTime)
			{
				pBase->GS()->CreateExplosion(pBase->GetPos(), pBase->GetClientID(), WEAPON_GRENADE, Damage);
			}
		}

		// magnetism
		for(auto* p = (CCharacter*)pBase->GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); p; p = (CCharacter*)p->TypeNext())
		{
			if(!p || distance(pBase->GetPos(), p->m_Core.m_Pos) > Radius)
				continue;

			if(!pBase->GetPlayer() || (pBase->GetClientID() != p->GetPlayer()->GetCID() && p->IsAllowedPVP(pBase->GetClientID())))
			{
				vec2 Dir = normalize(p->m_Core.m_Pos - pBase->GetPos());
				if(distance(pBase->GetPos(), p->m_Core.m_Pos) < 24.0f)
					continue;
				p->m_Core.m_Vel -= Dir * (1.50f);
			}
		}
	});

	// register event snap
	pCore->RegisterEvent(CBaseEntity::EventSnap, 12, [](CBaseEntity* pBase, int, const std::vector<int>& vIds)
	{
		// initialize variables
		const float Radius = pBase->GetGroup()->GetConfig("radius", 0.f);
		const float AngleStep = 2.0f * pi / (float)vIds.size();

		// draw points
		for(int i = 0; i < (int)vIds.size(); i++)
		{
			vec2 VertexPos = pBase->GetPos() + vec2(Radius * cos(AngleStep * i), Radius * sin(AngleStep * i));
			CNetObj_Projectile* pObj = static_cast<CNetObj_Projectile*>(pBase->Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, vIds[i], sizeof(CNetObj_Projectile)));
			if(!pObj)
				return;

			pObj->m_X = (int)VertexPos.x;
			pObj->m_Y = (int)VertexPos.y;
			pObj->m_VelX = 0;
			pObj->m_VelY = 0;
			pObj->m_StartTick = pBase->Server()->Tick() - 1;
			pObj->m_Type = WEAPON_HAMMER;
		}
	});

	if(pPtr)
		*pPtr = groupPtr;
}

void CEntityManager::HealthTurret(int ClientID, vec2 Position, int RestoreHealth, int Lifetime, int InitialReloadTick, std::weak_ptr<CEntityGroup>* pPtr) const
{
	// initialize group & config
	auto groupPtr = CEntityGroup::NewGroup(&GS()->m_World, ClientID);
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
		if(!Lifetime)
		{
			pBase->MarkForDestroy();
			return;
		}
		Lifetime--;

		// reload time
		int& ReloadTick = pBase->GetRefConfig("currentReloadTick", 0);
		if(ReloadTick > 0)
		{
			ReloadTick--;
			return;
		}
		ReloadTick = pBase->GetConfig("initialReloadTick", 2 * pBase->Server()->TickSpeed());

		// health
		bool ShowRestoreHealth = false;
		const int HealthRestored = pBase->GetGroup()->GetConfig("healthRestored", 0);
		for(auto pChar = (CCharacter*)pBase->GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
		{
			if(!pChar || distance(pBase->GetPos(), pChar->m_Core.m_Pos) > 620.f)
				continue;

			if(!pBase->GetPlayer() || (pBase->GetClientID() == pChar->GetPlayer()->GetCID() || !pChar->IsAllowedPVP(pBase->GetClientID())))
			{
				ShowRestoreHealth = true;
				new CHeartHealer(pBase->GameWorld(), pBase->GetPos(), pChar->GetPlayer(), HealthRestored, pChar->m_Core.m_Vel / 2.f);
			}
		}

		if(ShowRestoreHealth)
		{
			char aBuf[16];
			str_format(aBuf, sizeof(aBuf), "%dHP", HealthRestored);
			pBase->GS()->EntityManager()->Text(pBase->GetPos() + vec2(0, -96), 40, aBuf);
		}

	});

	// register event snap
	pPickup->RegisterEvent(CBaseEntity::EventSnap, 4, [](CBaseEntity* pBase, int, const std::vector<int>& vIds)
	{
		const int ReloadTick = pBase->GetConfig("currentReloadTick", 0);
		const float AngleStep = 2.0f * pi / (float)vIds.size();
		const float Radius = clamp(0.0f + (float)ReloadTick, 0.0f, 32.0f);
		for(int i = 0; i < (int)vIds.size(); i++)
		{
			const vec2 VertexPos = pBase->GetPos() + vec2(Radius * cos(AngleStep * (float)i), Radius * sin(AngleStep * (float)i));
			CNetObj_Pickup* pObj = static_cast<CNetObj_Pickup*>(pBase->Server()->SnapNewItem(NETOBJTYPE_PICKUP, vIds[i], sizeof(CNetObj_Pickup)));
			if(!pObj)
				return;

			pObj->m_X = (int)VertexPos.x;
			pObj->m_Y = (int)VertexPos.y;
			pObj->m_Type = POWERUP_HEALTH;
			pObj->m_Subtype = 0;
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
	auto groupPtr = CEntityGroup::NewGroup(&GS()->m_World, ClientID);
	groupPtr->SetConfig("health", Health);

	// initialize elements
	for(int i = 0; i < (int)vShieldEdges.size(); i++)
	{
		vec2 firstPos = vShieldEdges[i];
		vec2 endPos = vShieldEdges[minimum(i + 1, (int)vShieldEdges.size() - 2)];
		const auto pLaser = groupPtr->CreateLaser(Position + firstPos, Position + endPos, LASERTYPE_SHOTGUN);

		// register event tick
		pLaser->RegisterEvent(CBaseEntity::EventTick, [firstPos, endPos](CBaseEntity* pBase)
		{
			// check health
			int& Health = pBase->GetGroup()->GetRefConfig("health", 0);
			if(Health <= 0)
			{
				pBase->GS()->CreateExplosion(pBase->GetPos(), pBase->GetClientID(), WEAPON_WORLD, 3);
				pBase->GS()->CreateExplosion(pBase->GetPosTo(), pBase->GetClientID(), WEAPON_WORLD, 3);
				pBase->MarkForDestroy();
				return;
			}

			// knock back
			for(auto* p = (CCharacter*)pBase->GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); p; p = (CCharacter*)p->TypeNext())
			{
				if(p->GetPlayer()->GetCID() != pBase->GetClientID() && p->IsAllowedPVP(pBase->GetClientID()))
				{
					if(is_within_distance_on_line(48.f, pBase->GetPos(), pBase->GetPosTo(), p->GetPos()))
					{
						vec2 knockBackDir = normalize(p->GetPos() - pBase->GetCharacter()->GetPos());
						p->m_Core.m_Vel += knockBackDir * 12.f;
						pBase->GetCharacter()->TakeDamage(knockBackDir, 100, pBase->GetPlayer()->GetCID(), WEAPON_SELF);
					}
				}
			}

			// destroy projectile
			for(auto* p = (CProjectile*)pBase->GameWorld()->FindFirst(CGameWorld::ENTTYPE_PROJECTILE); p; p = (CProjectile*)p->TypeNext())
			{
				if(p->GetOwner() != pBase->GetPlayer()->GetCID())
				{
					if(is_within_distance_on_line(48.f, pBase->GetPos(), pBase->GetPosTo(), p->GetCurrentPos()))
					{
						Health -= p->GetDamage();
						pBase->GS()->CreateHammerHit(p->GetCurrentPos());
						p->MarkForDestroy();
					}
				}
			}

			// new position
			const auto* pChar = pBase->GetCharacter();
			const float angle = std::atan2(pChar->m_Core.m_Input.m_TargetY, pChar->m_Core.m_Input.m_TargetX);
			pBase->SetPos(rotate(firstPos, pChar->GetPos(), angle));
			pBase->SetPosTo(rotate(endPos, pChar->GetPos(), angle));
		});

		// register event snap
		pLaser->RegisterEvent(CBaseEntity::EventSnap, 1, [edgePos = vShieldEdges[i]](const CBaseEntity* pBase, int, const std::vector<int>& vIDs)
		{
			CNetObj_Pickup* pObj = static_cast<CNetObj_Pickup*>(pBase->Server()->SnapNewItem(NETOBJTYPE_PICKUP, vIDs[0], sizeof(CNetObj_Pickup)));
			if(!pObj)
				return;

			const auto* pChar = pBase->GetCharacter();
			const float angle = std::atan2(pChar->m_Core.m_Input.m_TargetY, pChar->m_Core.m_Input.m_TargetX);
			const vec2 curPos = rotate(edgePos, pChar->GetPos(), angle);
			pObj->m_X = static_cast<int>(curPos.x);
			pObj->m_Y = static_cast<int>(curPos.y);
			pObj->m_Type = POWERUP_ARMOR;
			pObj->m_Subtype = 0;
		});
	}

	if(pPtr)
		*pPtr = groupPtr;
}

void CEntityManager::FlameWall(int ClientID, vec2 Position, float Radius, int Lifetime, int DamagePerTick, float SlowDownFactor, std::weak_ptr<CEntityGroup>* pPtr) const
{
	// initialize group & config
	auto groupPtr = CEntityGroup::NewGroup(&GS()->m_World, ClientID);
	groupPtr->SetConfig("radius", Radius);

	// initialize element & config
	auto pWall = groupPtr->CreatePickup(Position, POWERUP_ARMOR);
	pWall->SetConfig("lifetimeTick", Lifetime);
	pWall->SetConfig("damagePerTick", DamagePerTick);
	pWall->SetConfig("slowDownFactor", SlowDownFactor);

	// register event tick
	pWall->RegisterEvent(CBaseEntity::EventTick, [](CBaseEntity* pBase)
	{
		// lifetime
		int& Lifetime = pBase->GetRefConfig("lifetimeTick", 0);
		if(Lifetime <= 0)
		{
			pBase->MarkForDestroy();
			return;
		}
		Lifetime--;

		// damage and slow down
		const float Radius = pBase->GetGroup()->GetConfig("radius", 0.f);
		const int DamagePerTick = pBase->GetConfig("damagePerTick", 0);
		const float SlowDownFactor = pBase->GetConfig("slowDownFactor", 0.f);

		// random explosion on position
		if(pBase->Server()->Tick() % pBase->Server()->TickSpeed() / 2 == 0)
			pBase->GS()->CreateRandomRadiusExplosion(2, Radius, pBase->GetPos(), pBase->GetClientID(), WEAPON_NINJA, DamagePerTick);

		// find the closest enemies
		for(auto* p = (CCharacter*)pBase->GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); p; p = (CCharacter*)p->TypeNext())
		{
			if(!p || distance(pBase->GetPos(), p->m_Core.m_Pos) > Radius)
				continue;

			if(!pBase->GetPlayer() || (pBase->GetClientID() != p->GetPlayer()->GetCID() && p->IsAllowedPVP(pBase->GetClientID())))
			{
				if(pBase->Server()->Tick() % pBase->Server()->TickSpeed() == 0)
					p->TakeDamage(vec2(0, 0), DamagePerTick, pBase->GetClientID(), WEAPON_WORLD);
				p->m_Core.m_Vel *= SlowDownFactor;
			}
		}
	});

	// register event snap
	enum { NUM_CIRCLE = 10, NUM_PROJ_INSIDE = 2, NUM_IDS = NUM_CIRCLE + NUM_PROJ_INSIDE};
	pWall->RegisterEvent(CBaseEntity::EventSnap, NUM_IDS, [](CBaseEntity* pBase, int, const std::vector<int>& vIds)
	{
		const float Radius = pBase->GetGroup()->GetConfig("radius", 0.f);
		constexpr float AngleStep = 2.0f * pi / static_cast<float>(NUM_CIRCLE);

		// create projectiles in a circle
		for(int i = 0; i < NUM_CIRCLE; ++i)
		{
			vec2 VertexPos = pBase->GetPos() + vec2(Radius * cos(AngleStep * i), Radius * sin(AngleStep * i));
			CNetObj_Projectile* pObj = static_cast<CNetObj_Projectile*>(pBase->Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, vIds[i], sizeof(CNetObj_Projectile)));
			if(!pObj)
				return;

			pObj->m_X = static_cast<int>(VertexPos.x);
			pObj->m_Y = static_cast<int>(VertexPos.y);
			pObj->m_VelX = 0;
			pObj->m_VelY = 0;
			pObj->m_StartTick = pBase->Server()->Tick() - 1;
			pObj->m_Type = WEAPON_GRENADE;
		}

		// create random projectiles inside the circle
		for(int i = 0; i < NUM_PROJ_INSIDE; ++i)
		{
			vec2 VertexPos = random_range_pos(pBase->GetPos(), Radius);
			CNetObj_Projectile* pObj = static_cast<CNetObj_Projectile*>(pBase->Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, vIds[NUM_CIRCLE + i], sizeof(CNetObj_Projectile)));
			if(!pObj)
				return;

			pObj->m_X = static_cast<int>(VertexPos.x);
			pObj->m_Y = static_cast<int>(VertexPos.y);
			pObj->m_VelX = 0;
			pObj->m_VelY = 0;
			pObj->m_StartTick = pBase->Server()->Tick() - 1;
			pObj->m_Type = WEAPON_HAMMER;
		}
	});

	if(pPtr)
		*pPtr = groupPtr;
}

void CEntityManager::FrostNova(int ClientID, vec2 Position, float Radius, int Damage, int FreezeTime, std::weak_ptr<CEntityGroup>* pPtr) const
{
	// initialize group & config
	auto groupPtr = CEntityGroup::NewGroup(&GS()->m_World, ClientID);
	groupPtr->SetConfig("radius", Radius);

	// initialize element & config
	auto pNova = groupPtr->CreatePickup(Position, POWERUP_ARMOR);
	pNova->SetConfig("damage", Damage);
	pNova->SetConfig("freezeTime", FreezeTime);

	// register event tick
	pNova->RegisterEvent(CBaseEntity::EventTick, [](CBaseEntity* pBase)
	{
		// trigger nova
		const float Radius = pBase->GetGroup()->GetConfig("radius", 0.f);
		const int Damage = pBase->GetConfig("damage", 0);
		const int FreezeTime = pBase->GetConfig("freezeTime", 0);

		for(auto* p = (CCharacter*)pBase->GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); p; p = (CCharacter*)p->TypeNext())
		{
			if(!p || distance(pBase->GetPos(), p->m_Core.m_Pos) > Radius)
				continue;

			if(pBase->GetClientID() != p->GetPlayer()->GetCID() && p->IsAllowedPVP(pBase->GetClientID()))
			{
				p->TakeDamage(vec2(0, 0), Damage, pBase->GetClientID(), WEAPON_WORLD);
				//p->Freeze(FreezeTime);

				pBase->MarkForDestroy();
			}
		}
	});

	// register event snap
	pNova->RegisterEvent(CBaseEntity::EventSnap, 4, [](CBaseEntity* pBase, int, const std::vector<int>& vIds)
	{
		const float Radius = pBase->GetGroup()->GetConfig("radius", 0.f);
		const float AngleStep = 2.0f * pi / (float)vIds.size();

		for(int i = 0; i < (int)vIds.size(); i++)
		{
			vec2 VertexPos = pBase->GetPos() + vec2(Radius * cos(AngleStep * i), Radius * sin(AngleStep * i));
			CNetObj_Pickup* pObj = static_cast<CNetObj_Pickup*>(pBase->Server()->SnapNewItem(NETOBJTYPE_PICKUP, vIds[i], sizeof(CNetObj_Pickup)));
			if(!pObj)
				return;

			pObj->m_X = (int)VertexPos.x;
			pObj->m_Y = (int)VertexPos.y;
			pObj->m_Type = POWERUP_ARMOR;
			pObj->m_Subtype = 0;
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
	auto groupPtr = CEntityGroup::NewGroup(&GS()->m_World, ClientID);
	groupPtr->SetConfig("radius", Radius);

	// initialize element & config
	auto pAura = groupPtr->CreatePickup(Position, POWERUP_ARMOR);
	pAura->SetConfig("lifetimeTick", Lifetime);
	pAura->SetConfig("healPerTick", HealPerTick);

	// generate random positions for hearts inside the radius
	std::vector<vec2> heartPositions;
	for(int i = 0; i < NUM_HEART; ++i)
	{
		vec2 HeartPos = random_range_pos(Position, Radius);
		HeartPos.y += Radius; // Start below the circle
		heartPositions.push_back(HeartPos);
	}
	pAura->SetConfig("heartPositions", heartPositions);

	// generate random initial velocities for hearts
	std::vector<vec2> heartVelocities;
	for(int i = 0; i < NUM_HEART; ++i)
	{
		vec2 HeartVelocity = vec2(random_float(-1.0f, 1.0f), random_float(-2.0f, -1.0f));
		heartVelocities.push_back(HeartVelocity);
	}
	pAura->SetConfig("heartVelocities", heartVelocities);

	// register event tick
	pAura->RegisterEvent(CBaseEntity::EventTick, [](CBaseEntity* pBase)
	{
		// lifetime
		int& Lifetime = pBase->GetRefConfig("lifetimeTick", 0);
		if(Lifetime <= 0)
		{
			pBase->MarkForDestroy();
			return;
		}
		Lifetime--;

		// initialize variables
		const float Radius = pBase->GetGroup()->GetConfig("radius", 0.f);
		const int HealPerTick = pBase->GetConfig("healPerTick", 0);

		// find the closest enemies
		if(pBase->Server()->Tick() % pBase->Server()->TickSpeed() == 0)
		{
			for(auto* p = (CCharacter*)pBase->GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); p; p = (CCharacter*)p->TypeNext())
			{
				if(!p || distance(pBase->GetPos(), p->m_Core.m_Pos) > Radius)
					continue;

				if(!pBase->GetPlayer() || pBase->GetClientID() == p->GetPlayer()->GetCID() || !p->IsAllowedPVP(pBase->GetClientID()))
				{
					p->IncreaseHealth(HealPerTick);
				}
			}
		}

		// update heart positions and velocities
		std::vector<vec2>& heartPositions = pBase->GetRefConfig("heartPositions", std::vector<vec2>{});
		std::vector<vec2>& heartVelocities = pBase->GetRefConfig("heartVelocities", std::vector<vec2>{});
		for(size_t i = 0; i < NUM_HEART; ++i)
		{
			// initialize variables
			vec2& pos = heartPositions[i];
			vec2& vel = heartVelocities[i];

			// update position with current velocity
			pos += vel;

			// apply some randomness to the velocity for chaotic movement
			vel.x += random_float(-0.1f, 0.1f);
			vel.y += random_float(-0.1f, 0.1f);

			// limit the velocities to avoid too fast movements
			vel.x = clamp(vel.x, -1.0f, 1.0f);
			vel.y = clamp(vel.y, -2.0f, -1.0f);

			// reset position if it goes out of bounds
			if(pos.y < pBase->GetPos().y - Radius)
			{
				pos = random_range_pos(pBase->GetPos(), Radius);
				pos.y = pBase->GetPos().y + Radius;
				vel = vec2(random_float(-1.0f, 1.0f), random_float(-2.0f, -1.0f)); // Reset to initial random velocity
			}
		}

	});

	// register event snap
	pAura->RegisterEvent(CBaseEntity::EventSnap, NUM_IDS, [](CBaseEntity* pBase, int, const std::vector<int>& vIds)
	{
		// initialize variables
		const float Radius = pBase->GetGroup()->GetConfig("radius", 0.f);
		constexpr float AngleStep = 2.0f * pi / (float)NUM_CIRCLES;

		// create pickups in a circle
		for(int i = 0; i < NUM_CIRCLES; i++)
		{
			vec2 VertexPos = pBase->GetPos() + vec2(Radius * cos(AngleStep * i), Radius * sin(AngleStep * i));
			CNetObj_Pickup* pObj = static_cast<CNetObj_Pickup*>(pBase->Server()->SnapNewItem(NETOBJTYPE_PICKUP, vIds[i], sizeof(CNetObj_Pickup)));
			if(!pObj)
				return;

			pObj->m_X = (int)VertexPos.x;
			pObj->m_Y = (int)VertexPos.y;
			pObj->m_Type = POWERUP_HEALTH;
			pObj->m_Subtype = 0;
		}

		// create lasers connecting each pickup to form a circle
		for(int i = 0; i < NUM_CIRCLES; i++)
		{
			int nextIndex = (i + 1) % (int)NUM_CIRCLES;
			vec2 CurrentPos = pBase->GetPos() + vec2(Radius * cos(AngleStep * i), Radius * sin(AngleStep * i));
			vec2 NextPos = pBase->GetPos() + vec2(Radius * cos(AngleStep * nextIndex), Radius * sin(AngleStep * nextIndex));

			CNetObj_Laser* pLaser = static_cast<CNetObj_Laser*>(pBase->Server()->SnapNewItem(NETOBJTYPE_LASER, vIds[NUM_CIRCLES + i], sizeof(CNetObj_Laser)));
			if(!pLaser)
				return;

			pLaser->m_X = (int)CurrentPos.x;
			pLaser->m_Y = (int)CurrentPos.y;
			pLaser->m_FromX = (int)NextPos.x;
			pLaser->m_FromY = (int)NextPos.y;
			pLaser->m_StartTick = pBase->Server()->Tick() - 1;
		}

		// create hearts inside the circle moving upwards
		const std::vector<vec2>& heartPositions = pBase->GetConfig("heartPositions", std::vector<vec2>{});
		for(int i = 0; i < NUM_HEART; i++)
		{
			vec2 InnerPos = heartPositions[i];
			CNetObj_Pickup* pObj = static_cast<CNetObj_Pickup*>(pBase->Server()->SnapNewItem(NETOBJTYPE_PICKUP, vIds[(NUM_IDS - NUM_HEART) + i], sizeof(CNetObj_Pickup)));
			if(!pObj)
				return;

			pObj->m_X = static_cast<int>(InnerPos.x);
			pObj->m_Y = static_cast<int>(InnerPos.y);
			pObj->m_Type = POWERUP_HEALTH;
			pObj->m_Subtype = 0;
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
	auto groupPtr = CEntityGroup::NewGroup(&GS()->m_World, ClientID);
	groupPtr->SetConfig("damage", Damage);
	groupPtr->SetConfig("fireCount", FireCount);
	groupPtr->SetConfig("explosionRadius", ExplosionRadius);
	groupPtr->SetConfig("explosionCount", ExplosionCount);

	// initialize element & config
	auto pBow = groupPtr->CreatePickup(pPlayer->GetCharacter()->GetPos());

	// register event tick
	pBow->RegisterEvent(CBaseEntity::EventTick, [](CBaseEntity* pBase)
	{
		// freeze input for bow
		int& FireCount = pBase->GetGroup()->GetRefConfig("fireCount", 0);
		CEventKeyManager::BlockInputGroup(pBase->GetClientID(), BLOCK_INPUT_FIRE);
		CEventKeyManager::BlockInputGroup(pBase->GetClientID(), BLOCK_INPUT_FREEZE_GUN);

		// check is key clicked
		if(CEventKeyManager::IsKeyClicked(pBase->GetClientID(), KEY_EVENT_FIRE))
		{
			// initialize variables
			vec2 Direction = normalize(vec2(pBase->GetCharacter()->m_Core.m_Input.m_TargetX, pBase->GetCharacter()->m_Core.m_Input.m_TargetY));

			// create fire
			const auto pArrow = pBase->GetGroup()->CreatePickup(pBase->GetCharacter()->GetPos());
			pArrow->SetConfig("direction", Direction);
			FireCount--;

			// register event tick
			pArrow->RegisterEvent(CBaseEntity::EventTick, [](CBaseEntity* pBase)
			{
				// initialize variables
				const float ExplosionRadius = pBase->GetGroup()->GetConfig("explosionRadius", 0.f);
				const int ExplosionCount = pBase->GetGroup()->GetConfig("explosionCount", 0);
				const int Damage = pBase->GetGroup()->GetConfig("damage", 0);
				vec2 Direction = pBase->GetConfig("direction", vec2());

				// find the closest enemy & hit enemies
				for(auto* p = (CCharacter*)pBase->GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); p; p = (CCharacter*)p->TypeNext())
				{
					if(!p || pBase->GetClientID() == p->GetPlayer()->GetCID() || !p->IsAllowedPVP(pBase->GetClientID()))
						continue;

					// explose
					const float Distance = distance(pBase->GetPos(), p->m_Core.m_Pos);
					if(Distance < 64.f)
					{
						p->TakeDamage(Direction, Damage, pBase->GetClientID(), WEAPON_GRENADE);
						pBase->GS()->CreateRandomRadiusExplosion(ExplosionCount, ExplosionRadius, pBase->GetPos(), pBase->GetClientID(), WEAPON_NINJA, Damage);
						pBase->MarkForDestroy();
						return;
					}

					// adjust direction slightly towards the closest enemy
					if(Distance < 300.0f)
					{
						vec2 ToEnemy = normalize(p->m_Core.m_Pos - pBase->GetPos());
						Direction = normalize(Direction + ToEnemy * 0.05f);
						pBase->SetConfig("direction", Direction);
					}
				}

				// check solid
				if(pBase->GS()->Collision()->CheckPoint(pBase->GetPos()))
				{
					pBase->GS()->CreateRandomRadiusExplosion(ExplosionCount, ExplosionRadius, pBase->GetPos(), pBase->GetClientID(), WEAPON_NINJA, Damage);
					pBase->MarkForDestroy();
					return;
				}

				// Update position
				pBase->SetPos(pBase->GetPos() + Direction * 18.0f);
			});

			// Register event snap for the fire projectile (drawing the arrow)
			pArrow->RegisterEvent(CBaseEntity::EventSnap, 3, [](CBaseEntity* pBase, int, const std::vector<int>& vIds)
			{
				const vec2 Direction = pBase->GetConfig("direction", vec2());
				vec2 Pos = pBase->GetPos();

				for(int i = 0; i < (int)vIds.size(); ++i)
				{
					vec2 SegmentPos = Pos - Direction * 24.f * i;
					CNetObj_Laser* pLaser = static_cast<CNetObj_Laser*>(pBase->Server()->SnapNewItem(NETOBJTYPE_LASER, vIds[i], sizeof(CNetObj_Laser)));
					if(pLaser)
					{
						pLaser->m_X = (int)SegmentPos.x;
						pLaser->m_Y = (int)SegmentPos.y;
						pLaser->m_FromX = (int)Pos.x;
						pLaser->m_FromY = (int)Pos.y;
						pLaser->m_StartTick = pBase->Server()->Tick() - 4;
					}

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
		const float angle = std::atan2(pBase->GetCharacter()->m_Core.m_Input.m_TargetY, pBase->GetCharacter()->m_Core.m_Input.m_TargetX);
		pBase->SetPos(rotate(vec2(0.f, -56.f), pBase->GetCharacter()->GetPos(), angle));
	});

	std::vector<vec2> vArrowEdges = {
		{-60.0f, 0.0f}, {-40.0f, -20.0f}, {-20.0f, -40.0f},
		{20.0f, -40.0f}, {40.0f, -20.0f}, {60.0f, 0.0f}
	};

	// Register event snap
	pBow->RegisterEvent(CBaseEntity::EventSnap, (int)vArrowEdges.size() + 1, [vEdges = vArrowEdges](CBaseEntity* pBase, int, const std::vector<int>& vIds)
	{
		const auto* pChar = pBase->GetCharacter();
		const float angle = std::atan2(pChar->m_Core.m_Input.m_TargetY, pChar->m_Core.m_Input.m_TargetX);

		vec2 firstPos = vEdges[vEdges.size() - 1];
		vec2 endPos = vEdges[0];
		vec2 Pos = rotate(firstPos, pChar->GetPos(), angle);
		vec2 PosTo = rotate(endPos, pChar->GetPos(), angle);

		CNetObj_Laser* pLaser = static_cast<CNetObj_Laser*>(pBase->Server()->SnapNewItem(NETOBJTYPE_LASER, vIds[0], sizeof(CNetObj_Laser)));
		if(pLaser)
		{
			pLaser->m_X = (int)Pos.x;
			pLaser->m_Y = (int)Pos.y;
			pLaser->m_FromX = (int)PosTo.x;
			pLaser->m_FromY = (int)PosTo.y;
			pLaser->m_StartTick = pBase->Server()->Tick() - 3;
		}

		for(int i = 0; i < (int)vEdges.size(); i++)
		{
			vec2 pos = vEdges[i];
			vec2 curPos = rotate(pos, pChar->GetPos(), angle);

			CNetObj_Pickup* pPickup = static_cast<CNetObj_Pickup*>(pBase->Server()->SnapNewItem(NETOBJTYPE_PICKUP, vIds[1 + i], sizeof(CNetObj_Pickup)));
			if(pPickup)
			{
				pPickup->m_X = (int)curPos.x;
				pPickup->m_Y = (int)curPos.y;
				pPickup->m_Type = POWERUP_ARMOR;
				pPickup->m_Subtype = 0;
			}
		}
	});

	if(pPtr)
		*pPtr = groupPtr;
}