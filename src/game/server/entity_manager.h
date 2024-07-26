/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITY_MANAGER_H
#define GAME_SERVER_ENTITY_MANAGER_H

#include "core/components/Inventory/ItemData.h"

// forward declarations
class CGS;
class CEntity;
class CPlayer;
class CEntityGroup;
class CEntityLaserOrbite;

class CEntityManager
{
	CGS* m_pGS {};
	CGS* GS() const { return m_pGS; }
	IServer* Server() const;

public:
	CEntityManager(CGS* pGS);

	void DropBonus(vec2 Pos, int Type, int Subtype, int Value, int NumDrop = 1, vec2 Force = vec2(0.0f, 0.0f)) const;
	void DropItem(vec2 Pos, int ClientID, CItem Item, vec2 Force = vec2(0.0f, 0.0f)) const;
	void RandomDropItem(vec2 Pos, int ClientID, float Chance, CItem Item, vec2 Force = vec2(0.0f, 0.0f)) const;

	void FlyingPoint(vec2 Pos, int ClientID, vec2 Force = vec2(0.0f, 0.0f)) const;
	void ExpFlyingPoint(vec2 Pos, int ClientID, int Exp, vec2 Force = vec2(0.0f, 0.0f)) const;

	void Text(vec2 Pos, int Lifespan, const char* pText, bool* pResult = nullptr) const;
	void Text(CEntity* pParent, int Lifespan, const char* pText, bool* pResult = nullptr) const;

	void LaserOrbite(int ClientID, int Amount, LaserOrbiteType Type, float Speed, float Radius, int LaserType = LASERTYPE_RIFLE, int64_t Mask = -1) const;
	void LaserOrbite(CEntity* pParent, int Amount, LaserOrbiteType Type, float Speed, float Radius, int LaserType = LASERTYPE_RIFLE, int64_t Mask = -1) const;
	void LaserOrbite(CEntityLaserOrbite*& pOut, int ClientID, int Amount, LaserOrbiteType Type, float Speed, float Radius, int LaserType = LASERTYPE_RIFLE, int64_t Mask = -1) const;
	void LaserOrbite(CEntityLaserOrbite*& pOut, CEntity* pParent, int Amount, LaserOrbiteType Type, float Speed, float Radius, int LaserType = LASERTYPE_RIFLE, int64_t Mask = -1) const;

	void GravityDisruption(int ClientID, vec2 Position, float Radius, int Lifetime, int Damage, std::weak_ptr<CEntityGroup>* pPtr = nullptr) const;
	void GravityDisruption(vec2 Position, float Radius, int Lifetime, int Damage, std::weak_ptr<CEntityGroup>* pPtr = nullptr) const
	{
		GravityDisruption(-1, Position, Radius, Lifetime, Damage);
	}
	
	void HealthTurret(int ClientID, vec2 Position, int RestoreHealth, int Lifetime, int InitialReloadtick, std::weak_ptr<CEntityGroup>* pPtr = nullptr) const;
	void HealthTurret(vec2 Position, int RestoreHealth, int Lifetime, int InitialReloadtick, std::weak_ptr<CEntityGroup>* pPtr = nullptr) const
	{
		HealthTurret(-1, Position, RestoreHealth, Lifetime, InitialReloadtick, pPtr);
	}
		
	void EnergyShield(int ClientID, vec2 Position, int Health, std::weak_ptr<CEntityGroup>* pPtr = nullptr) const;
};

#endif
