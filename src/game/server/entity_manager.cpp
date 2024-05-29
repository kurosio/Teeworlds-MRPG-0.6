/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "entity_manager.h"
#include "gamecontext.h"

#include "core/entities/items/drop_bonuses.h"
#include "core/entities/items/drop_items.h"
#include "core/entities/tools/flying_point.h"
#include "core/entities/tools/laser_orbite.h"
#include "core/entities/tools/loltext.h"

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