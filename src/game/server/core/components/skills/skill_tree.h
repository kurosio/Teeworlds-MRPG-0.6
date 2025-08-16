#ifndef GAME_SERVER_CORE_COMPONENTS_SKILLS_SKILL_TREE_H
#define GAME_SERVER_CORE_COMPONENTS_SKILLS_SKILL_TREE_H

enum class SkillMod : int
{
	None = 0,
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
	int OptionIndex = 0;
	std::string Name;
	std::string Description;
	SkillMod ModType = SkillMod::None;
	int ModValue = 0;
	int PriceSP = 0;
};

struct CSkillTreeLevel
{
	int LevelIndex = 0;
	std::vector<CSkillTreeOption> Options;

	const CSkillTreeOption* FindOption(int OptionIdx) const
	{
		for(const auto& o : Options)
			if(o.OptionIndex == OptionIdx)
				return &o;
		return nullptr;
	}
	bool HasOption(int OptionIdx) const { return FindOption(OptionIdx) != nullptr; }
	size_t GetOptionsCount() const { return Options.size(); }
};

struct CSkillTree : public MultiworldIdentifiableData< std::unordered_map < int, CSkillTree > >
{
	int SkillID = 0;
	std::vector<CSkillTreeLevel> Levels;

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

			if(LevelIndex <= 0)
				continue;

			if((int)Tree.Levels.size() < LevelIndex)
				Tree.Levels.resize(LevelIndex);

			auto& Level = Tree.Levels[LevelIndex - 1];
			Level.LevelIndex = LevelIndex;

			if(OptionIndex <= 0)
				continue;

			CSkillTreeOption Opt;
			Opt.OptionIndex = OptionIndex;
			Opt.Name = pRes->getString("Name");
			Opt.Description = pRes->getString("Description");
			Opt.ModType = static_cast<SkillMod>(pRes->getInt("ModType"));
			Opt.ModValue = pRes->getInt("ModValue");
			Opt.PriceSP = pRes->getInt("PriceSP");

			if(!Level.HasOption(OptionIndex))
				Level.Options.push_back(std::move(Opt));
		}

		for(auto& [_, Tree] : m_pData)
		{
			for(auto& L : Tree.Levels)
			{
				std::sort(L.Options.begin(), L.Options.end(),
					[](const CSkillTreeOption& a, const CSkillTreeOption& b){ return a.OptionIndex < b.OptionIndex; });
			}
		}
	}

	static CSkillTree* Get(int ID)
	{
		const auto it = CSkillTree::Data().find(ID);
		if(it != CSkillTree::Data().end())
			return &it->second;
		return nullptr;
	}

	const CSkillTreeLevel* GetLevel(int LevelIdx) const
	{
		if(LevelIdx <= 0 || LevelIdx > (int)Levels.size())
			return nullptr;
		return &Levels[LevelIdx - 1];
	}

	int GetMaxLevels() const { return (int)Levels.size(); }

	bool HasOption(int LevelIdx, int OptionIdx) const
	{
		const auto* pLevel = GetLevel(LevelIdx);
		return pLevel && pLevel->HasOption(OptionIdx);
	}

	const CSkillTreeOption* FindOption(int LevelIdx, int OptionIdx) const
	{
		const auto* pLevel = GetLevel(LevelIdx);
		return pLevel ? pLevel->FindOption(OptionIdx) : nullptr;
	}
};

#endif