#include "weapons.h"

#include <generated/server_data.h>
#include <game/server/entities/character.h>
#include <game/server/entities/laser.h>
#include <game/server/entities/projectile.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include <game/server/core/entities/tools/flying_point.h>
#include <game/server/core/entities/weapons/grenade_pizdamet.h>
#include <game/server/core/entities/weapons/rifle_magneticpulse.h>
#include <game/server/core/entities/weapons/rifle_tesla_serpent.h>
#include <game/server/core/entities/weapons/rifle_trackedplazma.h>
#include <game/server/core/entities/weapons/rifle_wallpusher.h>

constexpr int MAX_LENGTH_CHARACTERS = 16;

#define DECL_WEAPON_VARIANT(ClassName) \
	class ClassName : public IWeaponVariant { public: void Fire(CCharacter* pChar, vec2 Direction, vec2 ProjStartPos, int Dmg, int& ReloadTimer) const override; };

DECL_WEAPON_VARIANT(CHammerDefault)
DECL_WEAPON_VARIANT(CHammerLamp)
DECL_WEAPON_VARIANT(CHammerBlast)
DECL_WEAPON_VARIANT(CGunDefault)
DECL_WEAPON_VARIANT(CGunPulse)
DECL_WEAPON_VARIANT(CGunKill)
DECL_WEAPON_VARIANT(CShotgunDefault)
DECL_WEAPON_VARIANT(CShotgunBurst)
DECL_WEAPON_VARIANT(CGrenadeDefault)
DECL_WEAPON_VARIANT(CGrenadePizdamet)
DECL_WEAPON_VARIANT(CGrenadeInjury)
DECL_WEAPON_VARIANT(CRifleDefault)
DECL_WEAPON_VARIANT(CRifleWallPusher)
DECL_WEAPON_VARIANT(CRifleMagneticPulse)
DECL_WEAPON_VARIANT(CRifleTrackedPlazma)
DECL_WEAPON_VARIANT(CRifleTeslaSerpent)
DECL_WEAPON_VARIANT(CRifleDamager)

void CHammerDefault::Fire(CCharacter* pChar, vec2, vec2 ProjStartPos, int Dmg, int& ReloadTimer) const
{
	auto* pGS = pChar->GS();
	const bool IsBot = pChar->GetPlayer()->IsBot();
	const bool IsEquippedModuleRadius = pChar->GetPlayer()->GetItem(itBasicHammerPlus)->IsEquipped();
	const float RadiusMultiplier = IsEquippedModuleRadius ? 6.4f : 2.4f;

	for(auto* pEntity : pGS->m_World.FindEntities(ProjStartPos, pChar->GetRadius() * RadiusMultiplier, MAX_LENGTH_CHARACTERS, CGameWorld::ENTTYPE_CHARACTER))
	{
		auto* pTarget = dynamic_cast<CCharacter*>(pEntity);
		if(!pTarget || pChar->GetClientID() == pTarget->GetClientID())
			continue;

		if(pGS->Collision()->IntersectLineWithInvisible(ProjStartPos, pTarget->GetPos(), nullptr, nullptr))
			continue;

		if(!pTarget->IsAllowedPVP(pChar->GetClientID()))
			continue;

		const vec2 Direction = length(pTarget->GetPos() - pChar->GetPos()) > 0.0f
			? normalize(pTarget->GetPos() - pChar->GetPos()) : vec2(0.0f, -1.0f);
		const vec2 Force = vec2(0.0f, -1.0f) + normalize(Direction + vec2(0.0f, -1.1f)) * (IsBot ? 5.0f : 10.0f);

		pGS->CreateHammerHit(pTarget->GetPos());
		pTarget->TakeDamage(Force, Dmg, pChar->GetClientID(), WEAPON_HAMMER);
	}

	pGS->CreateSound(pChar->GetPos(), SOUND_HAMMER_FIRE);
	ReloadTimer = pGS->Server()->TickSpeed() / 3;
}

void CHammerLamp::Fire(CCharacter* pChar, vec2, vec2 ProjStartPos, int Dmg, int& ReloadTimer) const
{
	auto* pGS = pChar->GS();
	const bool IsEquippedModuleRadius = pChar->GetPlayer()->GetItem(itBasicHammerPlus)->IsEquipped();
	const float Radius = IsEquippedModuleRadius ? 500.0f : 380.0f;

	for(auto* pEntity : pGS->m_World.FindEntities(ProjStartPos, Radius, MAX_LENGTH_CHARACTERS, CGameWorld::ENTTYPE_CHARACTER))
	{
		auto* pTarget = dynamic_cast<CCharacter*>(pEntity);
		if(!pTarget || pChar->GetClientID() == pTarget->GetClientID())
			continue;

		if(pGS->Collision()->IntersectLineWithInvisible(ProjStartPos, pTarget->GetPos(), nullptr, nullptr))
			continue;

		if(!pTarget->IsAllowedPVP(pChar->GetClientID()))
			continue;

		const vec2 Direction = length(pTarget->GetPos() - pChar->GetPos()) > 0.0f
			? normalize(pTarget->GetPos() - pChar->GetPos()) : vec2(0.0f, -1.0f);
		const vec2 Force = vec2(0.0f, -1.0f) + normalize(Direction + vec2(0.0f, -1.1f)) * 10.0f;

		auto* pPoint = new CEntityFlyingPoint(&pGS->m_World, ProjStartPos, Force, pTarget->GetClientID(), pChar->GetClientID());
		pPoint->Register([pGS, Force, Dmg](CPlayer* pFrom, CPlayer* pPlayer)
		{
			auto* pCharacter = pPlayer->GetCharacter();
			pGS->CreateDeath(pCharacter->GetPos(), pPlayer->GetCID());
			pCharacter->TakeDamage(Force, Dmg, pFrom->GetCID(), WEAPON_HAMMER);
		});
	}

	pGS->CreateSound(pChar->GetPos(), SOUND_HAMMER_FIRE);
	ReloadTimer = pGS->Server()->TickSpeed() / 3;
}

void CHammerBlast::Fire(CCharacter* pChar, vec2 Direction, vec2, int Dmg, int& ReloadTimer) const
{
	auto* pGS = pChar->GS();
	const bool IsEquippedModuleRadius = pChar->GetPlayer()->GetItem(itBasicHammerPlus)->IsEquipped();
	const float Radius = IsEquippedModuleRadius ? 180.0f : 128.0f;

	for(auto* pTarget = (CCharacter*)pGS->m_World.FindFirst(CGameWorld::ENTTYPE_CHARACTER); pTarget; pTarget = (CCharacter*)pTarget->TypeNext())
	{
		if(pChar->GetClientID() == pTarget->GetClientID())
			continue;

		if(!pTarget->IsAllowedPVP(pChar->GetClientID()))
			continue;

		if(distance(pTarget->GetPos(), pChar->GetPos()) < Radius)
			pGS->CreateExplosion(pTarget->GetPos(), pChar->GetClientID(), WEAPON_HAMMER, Dmg);
	}

	pChar->AddVelocity(Direction * 2.5f);
	pGS->CreateExplosion(pChar->GetPos(), pChar->GetClientID(), WEAPON_HAMMER, Dmg);
	pGS->CreateSound(pChar->GetPos(), SOUND_SFX_WEAPON_DEAF_BLOW);
}

void CGunPulse::Fire(CCharacter* pChar, vec2 Direction, vec2, int Dmg, int& ReloadTimer) const
{
	new CLaser(pChar->GameWorld(), pChar->GetClientID(), Dmg, pChar->GetPos(), Direction, 400.0f, true);
	pChar->GS()->CreateSound(pChar->GetPos(), SOUND_SFX_WEAPON_PULSE);
}

void CGunKill::Fire(CCharacter* pChar, vec2 Direction, vec2 ProjStartPos, int, int& ReloadTimer) const
{
	const vec2 MouseTarget(pChar->m_LatestInput.m_TargetX, pChar->m_LatestInput.m_TargetY);
	const int Lifetime = pChar->Server()->TickSpeed() * pChar->GS()->Tuning()->m_GunLifetime;
	const bool Explosive = pChar->GetPlayer()->GetItem(itExplosiveGun)->IsEquipped();

	for(int i = -1; i <= 1; ++i)
	{
		const float Spread = 0.1f * i;
		const float Angle = angle(Direction) + Spread;
		const vec2 ShotDirection(cosf(Angle), sinf(Angle));
		new CProjectile(pChar->GameWorld(), WEAPON_GUN, pChar->GetPlayer()->GetCID(),
			ProjStartPos, ShotDirection, Lifetime,
			Explosive, 0, -1, MouseTarget, WEAPON_GUN);
	}

	pChar->GS()->CreateSound(pChar->GetPos(), SOUND_GUN_FIRE);
	ReloadTimer = pChar->Server()->TickSpeed() / 8;
}

void CGunDefault::Fire(CCharacter* pChar, vec2 Direction, vec2 ProjStartPos, int, int& ReloadTimer) const
{
	const vec2 MouseTarget(pChar->m_LatestInput.m_TargetX, pChar->m_LatestInput.m_TargetY);
	const int Lifetime = pChar->Server()->TickSpeed() * pChar->GS()->Tuning()->m_GunLifetime;
	const bool Explosive = pChar->GetPlayer()->GetItem(itExplosiveGun)->IsEquipped();
	new CProjectile(
		pChar->GameWorld(), WEAPON_GUN, pChar->GetPlayer()->GetCID(),
		ProjStartPos, Direction, Lifetime,
		Explosive, 0, -1, MouseTarget, WEAPON_GUN);
	pChar->GS()->CreateSound(pChar->GetPos(), SOUND_GUN_FIRE);
}

static void FireShot(CCharacter* pChar, vec2 Direction, vec2 ProjStartPos, int Spread)
{
	const int Lifetime = pChar->Server()->TickSpeed() * pChar->GS()->Tuning()->m_ShotgunLifetime;
	const bool Explosive = pChar->GetPlayer()->GetItem(itExplosiveShotgun)->IsEquipped();

	for(int i = 0; i < Spread; ++i)
	{
		const float SpreadAngle = (0.0058945f * (9.0f * Spread) / 2.0f) - (0.0058945f * (9.0f * i));
		const float Angle = angle(Direction) + SpreadAngle;
		const float Speed = static_cast<float>(pChar->GS()->Tuning()->m_ShotgunSpeeddiff) + random_float(0.2f);

		const vec2 ShotDirection(cosf(Angle), sinf(Angle));
		const vec2 TargetPos = Direction + ShotDirection * (Speed * 500.0f);

		new CProjectile(pChar->GameWorld(), WEAPON_SHOTGUN, pChar->GetPlayer()->GetCID(),
			ProjStartPos, ShotDirection * Speed, Lifetime,
			Explosive, 0, 15, TargetPos, WEAPON_SHOTGUN);
	}

	pChar->GS()->CreateSound(pChar->GetPos(), SOUND_SHOTGUN_FIRE);
}

void CShotgunDefault::Fire(CCharacter* pChar, vec2 Direction, vec2 ProjStartPos, int, int& ReloadTimer) const
{
	FireShot(pChar, Direction, ProjStartPos, 5);
}

void CShotgunBurst::Fire(CCharacter* pChar, vec2 Direction, vec2 ProjStartPos, int, int& ReloadTimer) const
{
	FireShot(pChar, Direction, ProjStartPos, 9);
	ReloadTimer = pChar->Server()->TickSpeed() / 5;
}

void CGrenadePizdamet::Fire(CCharacter* pChar, vec2 Direction, vec2 ProjStartPos, int, int& ReloadTimer) const
{
	new CEntityGrenadePizdamet(&pChar->GS()->m_World, pChar->GetClientID(), ProjStartPos, Direction);
	pChar->GS()->CreateSound(pChar->GetPos(), SOUND_SFX_WEAPON_PIZDAMET);
	ReloadTimer = pChar->Server()->TickSpeed() / 10;
}

void CGrenadeInjury::Fire(CCharacter* pChar, vec2 Direction, vec2 ProjStartPos, int, int& ReloadTimer) const
{
	const vec2 MouseTarget(pChar->m_LatestInput.m_TargetX, pChar->m_LatestInput.m_TargetY);
	const int Lifetime = pChar->Server()->TickSpeed() * pChar->GS()->Tuning()->m_GrenadeLifetime * 0.8f;

	for(int i = -1; i <= 1; ++i)
	{
		const float Spread = 0.12f * i;
		const float Angle = angle(Direction) + Spread;
		const vec2 GrenadeDirection = vec2(cosf(Angle), sinf(Angle)) * (1.0f - absolute(i) * 0.1f);

		new CProjectile(pChar->GameWorld(), WEAPON_GRENADE, pChar->GetPlayer()->GetCID(),
			ProjStartPos, GrenadeDirection, Lifetime,true, 0,
			SOUND_GRENADE_EXPLODE, MouseTarget, WEAPON_GRENADE);
	}

	pChar->GS()->CreateSound(pChar->GetPos(), SOUND_GRENADE_FIRE);
	ReloadTimer = pChar->Server()->TickSpeed() / 3;
}

void CGrenadeDefault::Fire(CCharacter* pChar, vec2 Direction, vec2 ProjStartPos, int, int& ReloadTimer) const
{
	const vec2 MouseTarget(pChar->m_LatestInput.m_TargetX, pChar->m_LatestInput.m_TargetY);
	const int Lifetime = pChar->Server()->TickSpeed() * pChar->GS()->Tuning()->m_GrenadeLifetime;

	new CProjectile(pChar->GameWorld(), WEAPON_GRENADE, pChar->GetPlayer()->GetCID(),
		ProjStartPos, Direction, Lifetime, true, 0,
		SOUND_GRENADE_EXPLODE, MouseTarget, WEAPON_GRENADE);
	pChar->GS()->CreateSound(pChar->GetPos(), SOUND_GRENADE_FIRE);
}

void CRifleWallPusher::Fire(CCharacter* pChar, vec2 Direction, vec2 ProjStartPos, int, int& ReloadTimer) const
{
	new CEntityRifleWallPusher(&pChar->GS()->m_World, pChar->GetClientID(), ProjStartPos, Direction, 5 * pChar->Server()->TickSpeed());
}

void CRifleMagneticPulse::Fire(CCharacter* pChar, vec2 Direction, vec2 ProjStartPos, int, int& ReloadTimer) const
{
	new CEntityRifleMagneticPulse(&pChar->GS()->m_World, pChar->GetClientID(), 128.0f, ProjStartPos, Direction);
}

void CRifleTrackedPlazma::Fire(CCharacter* pChar, vec2 Direction, vec2 ProjStartPos, int, int& ReloadTimer) const
{
	new CEntityRifleTrackedPlazma(&pChar->GS()->m_World, pChar->GetClientID(), ProjStartPos, Direction);
	pChar->GS()->CreateSound(pChar->GetPos(), SOUND_SFX_WEAPON_PLAZMA);
}

void CRifleTeslaSerpent::Fire(CCharacter* pChar, vec2 Direction, vec2 ProjStartPos, int Dmg, int& ReloadTimer) const
{
	new CEntityTeslaSerpent(&pChar->GS()->m_World,pChar->GetClientID(), ProjStartPos,
		Direction, Dmg, 400.0f, 3,0.5f);
	pChar->GS()->CreateSound(pChar->GetPos(), SOUND_LASER_FIRE);
}

void CRifleDamager::Fire(CCharacter* pChar, vec2 Direction, vec2 ProjStartPos, int Dmg, int& ReloadTimer) const
{
	for(int i = -1; i <= 1; ++i)
	{
		const float Angle = angle(Direction) + 0.1f * i;
		const vec2 ShotDirection(cosf(Angle), sinf(Angle));
		new CLaser(&pChar->GS()->m_World, pChar->GetClientID(), Dmg, ProjStartPos,
			ShotDirection, pChar->GS()->Tuning()->m_LaserReach * 0.75f, false);
	}

	pChar->GS()->CreateSound(pChar->GetPos(), SOUND_LASER_FIRE);
	ReloadTimer = pChar->Server()->TickSpeed() / 4;
}

void CRifleDefault::Fire(CCharacter* pChar, vec2 Direction, vec2 ProjStartPos, int Dmg, int& ReloadTimer) const
{
	new CLaser(&pChar->GS()->m_World, pChar->GetClientID(), Dmg, ProjStartPos,
		Direction, pChar->GS()->Tuning()->m_LaserReach, false);
	pChar->GS()->CreateSound(pChar->GetPos(), SOUND_LASER_FIRE);
}

CWeaponVariantRegistry& CWeaponVariantRegistry::Instance()
{
	static CWeaponVariantRegistry s_Instance;
	return s_Instance;
}

CWeaponVariantRegistry::CWeaponVariantRegistry()
{
	// hammer-type
	m_HammerDefault = std::make_unique<CHammerDefault>();
	m_HammerLamp = std::make_unique<CHammerLamp>();
	m_HammerBlast = std::make_unique<CHammerBlast>();

	// gun-type
	m_GunDefault = std::make_unique<CGunDefault>();
	m_GunPulse = std::make_unique<CGunPulse>();
	m_GunKill = std::make_unique<CGunKill>();

	// shotgun-type
	m_ShotgunDefault = std::make_unique<CShotgunDefault>();
	m_ShotgunBurst = std::make_unique<CShotgunBurst>();

	// grenade-type
	m_GrenadeDefault = std::make_unique<CGrenadeDefault>();
	m_GrenadePizdamet = std::make_unique<CGrenadePizdamet>();
	m_GrenadeInjury = std::make_unique<CGrenadeInjury>();

	// rifle-type
	m_RifleDefault = std::make_unique<CRifleDefault>();
	m_RifleWallPusher = std::make_unique<CRifleWallPusher>();
	m_RifleMagneticPulse = std::make_unique<CRifleMagneticPulse>();
	m_RifleTrackedPlazma = std::make_unique<CRifleTrackedPlazma>();
	m_RifleTeslaSerpent = std::make_unique<CRifleTeslaSerpent>();
	m_RifleDamager = std::make_unique<CRifleDamager>();
}

const IWeaponVariant* CWeaponVariantRegistry::FindHammer(const std::optional<int>& Id) const
{
	if(!Id.has_value())
		return nullptr;

	if(Id == itHammerLamp)
		return m_HammerLamp.get();

	if(Id == itHammerBlast)
		return m_HammerBlast.get();

	return m_HammerDefault.get();
}

const IWeaponVariant* CWeaponVariantRegistry::FindGun(const std::optional<int>& Id) const
{
	if(!Id.has_value())
		return nullptr;

	if(Id == itGunPulse)
		return m_GunPulse.get();

	if(Id == itKillGun)
		return m_GunKill.get();

	return m_GunDefault.get();
}

const IWeaponVariant* CWeaponVariantRegistry::FindShotgun(const std::optional<int>& Id) const
{
	if(!Id.has_value())
		return nullptr;

	if(Id == itBurstShotgun)
		return m_ShotgunBurst.get();

	return m_ShotgunDefault.get();
}

const IWeaponVariant* CWeaponVariantRegistry::FindGrenade(const std::optional<int>& Id) const
{
	if(!Id.has_value())
		return nullptr;

	if(Id == itPizdamet)
		return m_GrenadePizdamet.get();

	if(Id == itInjuryGrenade)
		return m_GrenadeInjury.get();

	return m_GrenadeDefault.get();
}

const IWeaponVariant* CWeaponVariantRegistry::FindRifle(const std::optional<int>& Id) const
{
	if(!Id.has_value())
		return nullptr;

	if(Id == itRifleWallPusher)
		return m_RifleWallPusher.get();

	if(Id == itRifleMagneticPulse)
		return m_RifleMagneticPulse.get();

	if(Id == itRifleTrackedPlazma)
		return m_RifleTrackedPlazma.get();

	if(Id == itRifleTeslaSerpent)
		return m_RifleTeslaSerpent.get();

	if(Id == itLaserDamager)
		return m_RifleDamager.get();

	return m_RifleDefault.get();
}