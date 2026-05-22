#ifndef GAME_SERVER_CORE_ENTITIES_WEAPONS_WEAPONS_H
#define GAME_SERVER_CORE_ENTITIES_WEAPONS_WEAPONS_H

class CCharacter;
class IWeaponVariant
{
public:
	virtual ~IWeaponVariant() = default;
	virtual void Fire(CCharacter* pChar, vec2 Direction, vec2 ProjStartPos, int TotalWeaponDamage, int& ReloadTimer) const = 0;
};

class CWeaponVariantRegistry
{
public:
	using VariantPtr = std::unique_ptr<IWeaponVariant>;

	static CWeaponVariantRegistry& Instance();
	const IWeaponVariant* FindHammer(const std::optional<int>& EquippedId) const;
	const IWeaponVariant* FindGun(const std::optional<int>& EquippedId) const;
	const IWeaponVariant* FindShotgun(const std::optional<int>& EquippedId) const;
	const IWeaponVariant* FindGrenade(const std::optional<int>& EquippedId) const;
	const IWeaponVariant* FindRifle(const std::optional<int>& EquippedId) const;

private:
	CWeaponVariantRegistry();

	VariantPtr m_HammerDefault;
	VariantPtr m_HammerLamp;
	VariantPtr m_HammerBlast;
	VariantPtr m_GunDefault;
	VariantPtr m_GunPulse;
	VariantPtr m_GunKill;
	VariantPtr m_ShotgunDefault;
	VariantPtr m_ShotgunBurst;
	VariantPtr m_GrenadeDefault;
	VariantPtr m_GrenadePizdamet;
	VariantPtr m_GrenadeInjury;
	VariantPtr m_RifleDefault;
	VariantPtr m_RifleWallPusher;
	VariantPtr m_RifleMagneticPulse;
	VariantPtr m_RifleTrackedPlazma;
	VariantPtr m_RifleTeslaSerpent;
	VariantPtr m_RifleDamager;
};

#endif
