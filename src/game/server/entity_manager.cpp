/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "entity_manager.h"
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
