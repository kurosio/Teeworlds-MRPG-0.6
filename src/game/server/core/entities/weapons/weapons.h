#ifndef GAME_SERVER_CORE_ENTITIES_WEAPONS_WEAPONS_H
#define GAME_SERVER_CORE_ENTITIES_WEAPONS_WEAPONS_H

#include <memory>
#include <optional>
#include <unordered_map>

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
	using VariantMap = std::unordered_map<int, VariantPtr>;

	CWeaponVariantRegistry();
	static const IWeaponVariant* FindVariantById(const VariantMap& Variants, const VariantPtr& pDefault, const std::optional<int>& EquippedId);

	VariantPtr m_HammerDefault;
	VariantPtr m_GunDefault;
	VariantPtr m_ShotgunDefault;
	VariantPtr m_GrenadeDefault;
	VariantPtr m_RifleDefault;

	VariantMap m_HammerVariants;
	VariantMap m_GunVariants;
	VariantMap m_ShotgunVariants;
	VariantMap m_GrenadeVariants;
	VariantMap m_RifleVariants;
};

#endif
