#ifndef GAME_SERVER_CORE_COMPONENTS_SKILLS_SKILL_TREE_H
#define GAME_SERVER_CORE_COMPONENTS_SKILLS_SKILL_TREE_H

enum class SkillMod : int
{
	None = 0,

	// base modification
	ManaCostPct = 1,
	Radius = 2,
	CastClick = 3,
	Lifetime = 4,
	BonusIncreasePct = 5,
	BonusIncreaseValue = 6,
	BonusDecreasePct = 7,
	BonusDecreaseValue = 8,

	// master craftsman
	MasterCraftExtraItemPct = 10,

	// attack teleport
	AttackTeleportCombo = 11,
	AttackTeleportStun = 12,
	AttackTeleportFire = 13,
	AttackTeleportRestoreHP = 14,

	// cure
	CureAlly = 15,

	// master weapon
	MasterWeaponAutoFire = 16,
};

inline std::string format_skill_modifier(SkillMod Mod, int Value)
{
	switch(Mod)
	{
		default:
			return "\0";

		case SkillMod::BonusIncreaseValue:
		case SkillMod::Radius:
			return fmt_default(" +{}", Value);

		case SkillMod::BonusDecreaseValue:
		case SkillMod::CastClick:
			return fmt_default(" -{}", Value);

		case SkillMod::BonusDecreasePct:
		case SkillMod::ManaCostPct:
			return fmt_default(" -{}%", Value);

		case SkillMod::BonusIncreasePct:
		case SkillMod::MasterCraftExtraItemPct:
			return fmt_default(" +{}%", Value);

		case SkillMod::Lifetime:
			return fmt_default(" +{}sec", Value);
	}
}

struct CSkillTreeOption
{
	std::string Name {};
	std::string Description {};
	SkillMod ModType = SkillMod::None;
	int ModValue {};
	int PriceSP {};
};

struct CSkillTreeLevel
{
	std::map<int, CSkillTreeOption> Options;

	const CSkillTreeOption* FindOption(int OptionIdx) const
	{
		if(const auto it = Options.find(OptionIdx); it != Options.end())
			return &it->second;
		return nullptr;
	}

	bool HasOption(int OptionIdx) const { return Options.contains(OptionIdx); }
	size_t GetOptionsCount() const { return Options.size(); }
};

struct CSkillTree : public MultiworldIdentifiableData< std::unordered_map < int, CSkillTree > >
{
	int SkillID {};
	std::map<int, CSkillTreeLevel> Levels {};

	static void LoadFromDB()
	{
		m_pData.clear();

		ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_skills_tree");
		while(pRes->next())
		{
			const int SkillID = pRes->getInt("SkillID");
			const int LevelIndex = pRes->getInt("LevelIndex");
			const int OptionIndex = pRes->getInt("OptionIndex");

			auto& Tree = m_pData[SkillID];
			Tree.SkillID = SkillID;
			if(LevelIndex <= 0 || OptionIndex <= 0)
				continue;

			auto& Opt = Tree.Levels[LevelIndex].Options[OptionIndex];
			Opt.Name = pRes->getString("Name");
			Opt.Description = pRes->getString("Description");
			Opt.ModType = static_cast<SkillMod>(pRes->getInt("ModType"));
			Opt.ModValue = pRes->getInt("ModValue");
			Opt.PriceSP = pRes->getInt("PriceSP");
		}
	}

	static CSkillTree* Get(int ID)
	{
		if(const auto it = CSkillTree::Data().find(ID); it != CSkillTree::Data().end())
			return &it->second;
		return nullptr;
	}

	const CSkillTreeLevel* GetLevel(int LevelIdx) const
	{
		if(const auto it = Levels.find(LevelIdx); it != Levels.end())
			return &it->second;
		return nullptr;
	}

	int GetMaxLevels() const { return Levels.size(); }

	bool HasOption(int LevelIdx, int OptionIdx) const
	{
		if(const auto* pLevel = GetLevel(LevelIdx))
			return pLevel->HasOption(OptionIdx);
		return false;
	}

	const CSkillTreeOption* FindOption(int LevelIdx, int OptionIdx) const
	{
		if(const auto* pLevel = GetLevel(LevelIdx))
			return pLevel->FindOption(OptionIdx);
		return nullptr;
	}
};

#endif