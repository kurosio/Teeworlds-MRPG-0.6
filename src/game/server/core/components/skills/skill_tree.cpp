/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "skill_tree.h"
#include "skill_db.h"

std::string format_skill_modifier(SkillMod Mod, int Value)
{
	switch(Mod)
	{
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

		case SkillMod::None:
		case SkillMod::MasterAdventurerExtraSlotActiveModule:
		case SkillMod::MasterAdventurerExtraSlotPassiveModule:
		case SkillMod::AttackTeleportCombo:
		case SkillMod::AttackTeleportStun:
		case SkillMod::AttackTeleportFire:
		case SkillMod::AttackTeleportRestoreHP:
		case SkillMod::CureAlly:
		case SkillMod::MasterWeaponAutoFire:
		default:
			return "";
	}
}

const CSkillTreeOption* CSkillTreeLevel::FindOption(int OptionIdx) const
{
	const auto It = Options.find(OptionIdx);
	return It != Options.end() ? &It->second : nullptr;
}

bool CSkillTreeLevel::HasOption(int OptionIdx) const
{
	return Options.contains(OptionIdx);
}

std::size_t CSkillTreeLevel::GetOptionsCount() const noexcept
{
	return Options.size();
}

void CSkillTree::LoadFromDB()
{
	auto& Data = CSkillTree::Data();
	Data.clear();

	auto Rows = SkillDb::ListTree();
	while(Rows.Next())
	{
		const int SkillID = Rows.GetInt("SkillID");
		const int LevelIndex = Rows.GetInt("LevelIndex");
		const int OptionIndex = Rows.GetInt("OptionIndex");

		if(SkillID <= 0)
			continue;

		auto& Tree = Data[SkillID];
		Tree.SkillID = SkillID;

		if(LevelIndex <= 0 || OptionIndex <= 0)
			continue;

		auto& Opt = Tree.Levels[LevelIndex].Options[OptionIndex];
		Opt.Name = Rows.GetString("Name");
		Opt.Description = Rows.GetString("Description");
		Opt.ModType = static_cast<SkillMod>(Rows.GetInt("ModType"));
		Opt.ModValue = Rows.GetInt("ModValue");
		Opt.PriceSP = Rows.GetInt("PriceSP");
	}
}

const CSkillTree* CSkillTree::Get(int ID)
{
	const auto& Data = CSkillTree::Data();
	const auto It = Data.find(ID);
	return It != Data.end() ? &It->second : nullptr;
}

const CSkillTreeLevel* CSkillTree::GetLevel(int LevelIdx) const
{
	const auto It = Levels.find(LevelIdx);
	return It != Levels.end() ? &It->second : nullptr;
}

int CSkillTree::GetMaxLevels() const noexcept
{
	if(Levels.empty())
		return 0;

	return Levels.rbegin()->first;
}

bool CSkillTree::HasOption(int LevelIdx, int OptionIdx) const
{
	const auto* pLevel = GetLevel(LevelIdx);
	return pLevel != nullptr && pLevel->HasOption(OptionIdx);
}

const CSkillTreeOption* CSkillTree::FindOption(int LevelIdx, int OptionIdx) const
{
	const auto* pLevel = GetLevel(LevelIdx);
	return pLevel != nullptr ? pLevel->FindOption(OptionIdx) : nullptr;
}