#ifndef GAME_SERVER_CORE_COMPONENTS_SKILLS_SKILL_TREE_H
#define GAME_SERVER_CORE_COMPONENTS_SKILLS_SKILL_TREE_H

enum class SkillMod : int
{
	None = 0,

	// Base modification
	ManaCostPct = 1,
	Radius = 2,
	CastClick = 3,
	Lifetime = 4,
	BonusIncreasePct = 5,
	BonusIncreaseValue = 6,
	BonusDecreasePct = 7,
	BonusDecreaseValue = 8,

	// Master craftsman
	MasterCraftExtraItemPct = 10,

	// Attack teleport
	AttackTeleportCombo = 11,      // Allow combo attack
	AttackTeleportStun = 12,       // Attack teleport has stun effect
	AttackTeleportFire = 13,       // Attack teleport has fire effect
	AttackTeleportRestoreHP = 14,  // Attack teleport can restore health

	// Cure
	CureAlly = 15,                 // Cure can restore health for ally

	// Master weapon
	MasterWeaponAutoFire = 16,     // Auto fire from weapons
};

[[nodiscard]] std::string format_skill_modifier(SkillMod Mod, int Value);

struct CSkillTreeOption
{
	std::string Name;
	std::string Description;
	SkillMod ModType { SkillMod::None };
	int ModValue {};
	int PriceSP {};
};

struct CSkillTreeLevel
{
	std::map<int, CSkillTreeOption> Options;

	[[nodiscard]] const CSkillTreeOption* FindOption(int OptionIdx) const;
	[[nodiscard]] bool HasOption(int OptionIdx) const;
	[[nodiscard]] std::size_t GetOptionsCount() const noexcept;
};

struct CSkillTree : public MultiworldIdentifiableData<std::unordered_map<int, CSkillTree>>
{
	int SkillID {};
	std::map<int, CSkillTreeLevel> Levels;

	static void LoadFromDB();

	[[nodiscard]] static const CSkillTree* Get(int ID);
	[[nodiscard]] const CSkillTreeLevel* GetLevel(int LevelIdx) const;
	[[nodiscard]] int GetMaxLevels() const noexcept;
	[[nodiscard]] bool HasOption(int LevelIdx, int OptionIdx) const;
	[[nodiscard]] const CSkillTreeOption* FindOption(int LevelIdx, int OptionIdx) const;
};

#endif // GAME_SERVER_CORE_COMPONENTS_SKILLS_SKILL_TREE_H