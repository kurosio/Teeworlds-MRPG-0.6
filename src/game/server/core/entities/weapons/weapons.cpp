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
	class ClassName : public IWeaponVariant { public: void Fire(CCharacter* pChar, vec2 Direction, vec2 ProjStartPos, int Dmg) const override; };

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

void CHammerDefault::Fire(CCharacter* pChar, vec2, vec2 ProjStartPos, int Dmg) const
{
	auto* pGS = pChar->GS();
	const bool IsBot = pChar->GetPlayer()->IsBot();
	const bool IsEquippedModuleRadius = pChar->GetPlayer()->GetItem(itBasicHammerPlus)->IsEquipped();
	const float RadiusMultiplier = IsEquippedModuleRadius ? 6.4f : 2.4f;

	bool Hit = false;
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
		Hit = true;
	}

	pGS->CreateSound(pChar->GetPos(), SOUND_HAMMER_FIRE);
	pChar->SetAttackSpeedReloadTimer(WEAPON_HAMMER, Hit ? 2.5f : 1.0f);
}

void CHammerLamp::Fire(CCharacter* pChar, vec2, vec2 ProjStartPos, int Dmg) const
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
	pChar->SetAttackSpeedReloadTimer(WEAPON_HAMMER, 1.4f);
}

void CHammerBlast::Fire(CCharacter* pChar, vec2 Direction, vec2, int Dmg) const
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
	pChar->SetAttackSpeedReloadTimer(WEAPON_HAMMER, 1.0f);
}

void CGunPulse::Fire(CCharacter* pChar, vec2 Direction, vec2, int Dmg) const
{
	new CLaser(pChar->GameWorld(), pChar->GetClientID(), Dmg, pChar->GetPos(), Direction, 400.0f, true);
	pChar->GS()->CreateSound(pChar->GetPos(), SOUND_SFX_WEAPON_PULSE);
	pChar->SetAttackSpeedReloadTimer(WEAPON_GUN, 1.2f);
}

void CGunKill::Fire(CCharacter* pChar, vec2 Direction, vec2 ProjStartPos, int) const
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
	pChar->SetAttackSpeedReloadTimer(WEAPON_GUN, 0.9f);
}

void CGunDefault::Fire(CCharacter* pChar, vec2 Direction, vec2 ProjStartPos, int) const
{
	const vec2 MouseTarget(pChar->m_LatestInput.m_TargetX, pChar->m_LatestInput.m_TargetY);
	const int Lifetime = pChar->Server()->TickSpeed() * pChar->GS()->Tuning()->m_GunLifetime;
	const bool Explosive = pChar->GetPlayer()->GetItem(itExplosiveGun)->IsEquipped();
	new CProjectile(
		pChar->GameWorld(), WEAPON_GUN, pChar->GetPlayer()->GetCID(),
		ProjStartPos, Direction, Lifetime,
		Explosive, 0, -1, MouseTarget, WEAPON_GUN);
	pChar->GS()->CreateSound(pChar->GetPos(), SOUND_GUN_FIRE);
	pChar->SetAttackSpeedReloadTimer(WEAPON_GUN, 1.0f);
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

void CShotgunDefault::Fire(CCharacter* pChar, vec2 Direction, vec2 ProjStartPos, int) const
{
	FireShot(pChar, Direction, ProjStartPos, 5);
	pChar->SetAttackSpeedReloadTimer(WEAPON_SHOTGUN, 1.0f);
}

void CShotgunBurst::Fire(CCharacter* pChar, vec2 Direction, vec2 ProjStartPos, int) const
{
	FireShot(pChar, Direction, ProjStartPos, 9);
	pChar->SetAttackSpeedReloadTimer(WEAPON_SHOTGUN, 0.5f);
}

void CGrenadePizdamet::Fire(CCharacter* pChar, vec2 Direction, vec2 ProjStartPos, int) const
{
	new CEntityGrenadePizdamet(&pChar->GS()->m_World, pChar->GetClientID(), ProjStartPos, Direction);
	pChar->GS()->CreateSound(pChar->GetPos(), SOUND_SFX_WEAPON_PIZDAMET);
	pChar->SetAttackSpeedReloadTimer(WEAPON_GRENADE, 0.25f);
}

void CGrenadeInjury::Fire(CCharacter* pChar, vec2 Direction, vec2 ProjStartPos, int) const
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
	pChar->SetAttackSpeedReloadTimer(WEAPON_GRENADE, 0.9f);
}

void CGrenadeDefault::Fire(CCharacter* pChar, vec2 Direction, vec2 ProjStartPos, int) const
{
	const vec2 MouseTarget(pChar->m_LatestInput.m_TargetX, pChar->m_LatestInput.m_TargetY);
	const int Lifetime = pChar->Server()->TickSpeed() * pChar->GS()->Tuning()->m_GrenadeLifetime;

	new CProjectile(pChar->GameWorld(), WEAPON_GRENADE, pChar->GetPlayer()->GetCID(),
		ProjStartPos, Direction, Lifetime, true, 0,
		SOUND_GRENADE_EXPLODE, MouseTarget, WEAPON_GRENADE);
	pChar->GS()->CreateSound(pChar->GetPos(), SOUND_GRENADE_FIRE);
	pChar->SetAttackSpeedReloadTimer(WEAPON_GRENADE, 1.0f);
}

void CRifleWallPusher::Fire(CCharacter* pChar, vec2 Direction, vec2 ProjStartPos, int) const
{
	new CEntityRifleWallPusher(&pChar->GS()->m_World, pChar->GetClientID(), ProjStartPos, Direction, 5 * pChar->Server()->TickSpeed());
	pChar->SetAttackSpeedReloadTimer(WEAPON_LASER, 0.8f);
}

void CRifleMagneticPulse::Fire(CCharacter* pChar, vec2 Direction, vec2 ProjStartPos, int) const
{
	new CEntityRifleMagneticPulse(&pChar->GS()->m_World, pChar->GetClientID(), 128.0f, ProjStartPos, Direction);
	pChar->SetAttackSpeedReloadTimer(WEAPON_LASER, 2.0f);
}

void CRifleTrackedPlazma::Fire(CCharacter* pChar, vec2 Direction, vec2 ProjStartPos, int) const
{
	new CEntityRifleTrackedPlazma(&pChar->GS()->m_World, pChar->GetClientID(), ProjStartPos, Direction);
	pChar->GS()->CreateSound(pChar->GetPos(), SOUND_SFX_WEAPON_PLAZMA);
	pChar->SetAttackSpeedReloadTimer(WEAPON_LASER, 1.5f);
}

void CRifleTeslaSerpent::Fire(CCharacter* pChar, vec2 Direction, vec2 ProjStartPos, int Dmg) const
{
	new CEntityTeslaSerpent(&pChar->GS()->m_World,pChar->GetClientID(), ProjStartPos,
		Direction, Dmg, 400.0f, 3,0.5f);
	pChar->GS()->CreateSound(pChar->GetPos(), SOUND_LASER_FIRE);
	pChar->SetAttackSpeedReloadTimer(WEAPON_LASER, 0.25f);
}

void CRifleDamager::Fire(CCharacter* pChar, vec2 Direction, vec2 ProjStartPos, int Dmg) const
{
	for(int i = -1; i <= 1; ++i)
	{
		const float Angle = angle(Direction) + 0.1f * i;
		const vec2 ShotDirection(cosf(Angle), sinf(Angle));
		new CLaser(&pChar->GS()->m_World, pChar->GetClientID(), Dmg, ProjStartPos,
			ShotDirection, pChar->GS()->Tuning()->m_LaserReach * 0.75f, false);
	}

	pChar->GS()->CreateSound(pChar->GetPos(), SOUND_LASER_FIRE);
	pChar->SetAttackSpeedReloadTimer(WEAPON_LASER, 0.8f);
}

void CRifleDefault::Fire(CCharacter* pChar, vec2 Direction, vec2 ProjStartPos, int Dmg) const
{
	new CLaser(&pChar->GS()->m_World, pChar->GetClientID(), Dmg, ProjStartPos,
		Direction, pChar->GS()->Tuning()->m_LaserReach, false);
	pChar->GS()->CreateSound(pChar->GetPos(), SOUND_LASER_FIRE);
	pChar->SetAttackSpeedReloadTimer(WEAPON_LASER, 1.0f);
}

CWeaponVariantRegistry& CWeaponVariantRegistry::Instance()
{
	static CWeaponVariantRegistry s_Instance;
	return s_Instance;
}

CWeaponVariantRegistry::CWeaponVariantRegistry()
{
	// default weapons
	m_HammerDefault = std::make_unique<CHammerDefault>();
	m_GunDefault = std::make_unique<CGunDefault>();
	m_ShotgunDefault = std::make_unique<CShotgunDefault>();
	m_GrenadeDefault = std::make_unique<CGrenadeDefault>();
	m_RifleDefault = std::make_unique<CRifleDefault>();

	// hammer
	m_HammerVariants.emplace(itHammerLamp, std::make_unique<CHammerLamp>());
	m_HammerVariants.emplace(itHammerBlast, std::make_unique<CHammerBlast>());

	// gun
	m_GunVariants.emplace(itGunPulse, std::make_unique<CGunPulse>());
	m_GunVariants.emplace(itKillGun, std::make_unique<CGunKill>());

	// shotgun
	m_ShotgunVariants.emplace(itBurstShotgun, std::make_unique<CShotgunBurst>());

	// grenade
	m_GrenadeVariants.emplace(itPizdamet, std::make_unique<CGrenadePizdamet>());
	m_GrenadeVariants.emplace(itInjuryGrenade, std::make_unique<CGrenadeInjury>());

	// rifle
	m_RifleVariants.emplace(itRifleWallPusher, std::make_unique<CRifleWallPusher>());
	m_RifleVariants.emplace(itRifleMagneticPulse, std::make_unique<CRifleMagneticPulse>());
	m_RifleVariants.emplace(itRifleTrackedPlazma, std::make_unique<CRifleTrackedPlazma>());
	m_RifleVariants.emplace(itRifleTeslaSerpent, std::make_unique<CRifleTeslaSerpent>());
	m_RifleVariants.emplace(itLaserDamager, std::make_unique<CRifleDamager>());
}

const IWeaponVariant* CWeaponVariantRegistry::FindVariantById(const VariantMap& Variants, const VariantPtr& pDefault, const std::optional<int>& EquippedId)
{
	if(!EquippedId.has_value())
		return nullptr;

	const auto It = Variants.find(*EquippedId);
	if(It != Variants.end())
		return It->second.get();

	return pDefault.get();
}

const IWeaponVariant* CWeaponVariantRegistry::FindHammer(const std::optional<int>& Id) const
{
	return FindVariantById(m_HammerVariants, m_HammerDefault, Id);
}

const IWeaponVariant* CWeaponVariantRegistry::FindGun(const std::optional<int>& Id) const
{
	return FindVariantById(m_GunVariants, m_GunDefault, Id);
}

const IWeaponVariant* CWeaponVariantRegistry::FindShotgun(const std::optional<int>& Id) const
{
	return FindVariantById(m_ShotgunVariants, m_ShotgunDefault, Id);
}

const IWeaponVariant* CWeaponVariantRegistry::FindGrenade(const std::optional<int>& Id) const
{
	return FindVariantById(m_GrenadeVariants, m_GrenadeDefault, Id);
}

const IWeaponVariant* CWeaponVariantRegistry::FindRifle(const std::optional<int>& Id) const
{
	return FindVariantById(m_RifleVariants, m_RifleDefault, Id);
}
