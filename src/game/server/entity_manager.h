/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITY_MANAGER_H
#define GAME_SERVER_ENTITY_MANAGER_H

#include "core/components/inventory/item_data.h"

// forward declarations
class CGS;
class CEntity;
class CPlayer;
class CEntityGroup;
class CEntityLaserOrbite;
using EntGroupWeakPtr = std::weak_ptr<CEntityGroup>;

class CEntityManager
{
	CGS* m_pGS {};
	CGS* GS() const { return m_pGS; }
	IServer* Server() const;

public:
	CEntityManager(CGS* pGS);

	void DesignRandomDrop(int Amount, float MaxForce, vec2 Pos, int LifeSpan, int Type, int Subtype, int64_t Mask = -1) const;
	void DropPickup(vec2 Pos, int Type, int Subtype, int Value, int NumDrop = 1, vec2 Force = vec2(0.0f, 0.0f)) const;
	void DropItem(vec2 Pos, int ClientID, const CItem& Item, vec2 Force = vec2(0.0f, 0.0f)) const;
	void RandomDropItem(vec2 Pos, int ClientID, float Chance, const CItem& Item, vec2 Force = vec2(0.0f, 0.0f)) const;
	void FlyingPoint(vec2 Pos, int ClientID, vec2 Force = vec2(0.0f, 0.0f)) const;
	void ExpFlyingPoint(vec2 Pos, int ClientID, int Exp, vec2 Force = vec2(0.0f, 0.0f)) const;

	// text
	void Text(vec2 Pos, int Lifespan, const char* pText, bool* pResult = nullptr) const;
	void Text(CEntity* pParent, int Lifespan, const char* pText, bool* pResult = nullptr) const;

	// laser orbite
	void LaserOrbite(int ClientID, int Amount, LaserOrbiteType Type, float Speed, float Radius, int LaserType = LASERTYPE_RIFLE, int64_t Mask = -1) const;
	void LaserOrbite(CEntity* pParent, int Amount, LaserOrbiteType Type, float Speed, float Radius, int LaserType = LASERTYPE_RIFLE, int64_t Mask = -1) const;
	void LaserOrbite(CEntityLaserOrbite*& pOut, int ClientID, int Amount, LaserOrbiteType Type, float Speed, float Radius, int LaserType = LASERTYPE_RIFLE, int64_t Mask = -1) const;
	void LaserOrbite(CEntityLaserOrbite*& pOut, CEntity* pParent, int Amount, LaserOrbiteType Type, float Speed, float Radius, int LaserType = LASERTYPE_RIFLE, int64_t Mask = -1) const;

	// skills
	void StartUniversalCast(int ClientID, vec2 TargetPosition, int NumClicked,
		std::function<void(int, vec2, EntGroupWeakPtr*)> ActualSkillExecutionFunc, EntGroupWeakPtr* pCastingProcessTracker) const;
	void HealingRift(int ClientID, vec2 Position, float RiftRadius, float HealRadius, int Lifetime, float SerpentSpawnInterval,
		int NumSerpentsPerSpawn, int HealAmountPerPulse, int NumOuterSegments, int NumInnerSegments, EntGroupWeakPtr* pPtr) const;
	void GravityDisruption(int ClientID, vec2 Position, float Radius, int Lifetime, int Damage, EntGroupWeakPtr* pPtr = nullptr) const;
	void HealthTurret(int ClientID, vec2 Position, int RestoreHealth, int Lifetime, int InitialReloadtick, EntGroupWeakPtr* pPtr = nullptr) const;
	void LastStand(int ClientID, vec2 Position, float Radius, int ManaCostPerSec, EntGroupWeakPtr* pPtr = nullptr) const;
	void FlameWall(int ClientID, vec2 Position, float Radius, int Lifetime, int DamagePerTick, float SlowDownFactor, EntGroupWeakPtr* pPtr = nullptr) const;
	void HealingAura(int ClientID, vec2 Position, float Radius, int Lifetime, int HealPerTick, EntGroupWeakPtr* pPtr = nullptr) const;
	void FrostNova(int ClientID, vec2 Position, float Radius, int Damage, int FreezeTime, EntGroupWeakPtr* pPtr = nullptr) const;
	void Bow(int ClientID, int Damage, int FireCount, float ExplosionRadius, int ExplosionCount, EntGroupWeakPtr* pPtr = nullptr) const;

	// effect's
	void EffectCircleDamage(int ClientID, int DelayImpulse, int DelayBetweenImpulses, int Repeat = -1) const;
};

#endif
