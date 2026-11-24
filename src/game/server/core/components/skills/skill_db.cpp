#include "skill_db.h"

namespace SkillDb
{
	namespace
	{
		constexpr Db::EKind KIND_SKILLS_LIST = Db::KindOfTable("tw_skills_list");
		constexpr Db::EKind KIND_SKILLS_TREE = Db::KindOfTable("tw_skills_tree");
		constexpr Db::EKind KIND_ACCOUNTS_SKILLS = Db::KindOfTable("tw_accounts_skills");
		constexpr Db::EKind KIND_ACCOUNTS_SKILL_TREE = Db::KindOfTable("tw_accounts_skill_tree");
	}

	Db::CResultSet ListSkills()
	{
		return Db::Query(KIND_SKILLS_LIST, "SELECT * FROM tw_skills_list").Result();
	}

	Db::CResultSet ListTree()
	{
		return Db::Query(KIND_SKILLS_TREE, "SELECT * FROM tw_skills_tree").Result();
	}

	Db::CRequest LoadAccountSkills(int AccountID)
	{
		return Db::Query(KIND_ACCOUNTS_SKILLS, "SELECT * FROM tw_accounts_skills WHERE UserID = ?", Db::Bind(AccountID));
	}

	Db::CRequest LoadAccountTreeSelections(int AccountID)
	{
		return Db::Query(KIND_ACCOUNTS_SKILL_TREE,
			"SELECT SkillID, LevelIndex, OptionIndex FROM tw_accounts_skill_tree WHERE UserID = ?", Db::Bind(AccountID));
	}

	Db::CRequest CreateAccountSkill(int SkillID, int AccountID, int UsedByEmoticon)
	{
		return Db::Query(KIND_ACCOUNTS_SKILLS, "INSERT INTO tw_accounts_skills (SkillID, UserID, UsedByEmoticon) VALUES (?, ?, ?)",
			Db::Bind(SkillID, AccountID, UsedByEmoticon));
	}

	Db::CRequest SaveUsedByEmoticon(int SkillID, int AccountID, int UsedByEmoticon)
	{
		return Db::Query(KIND_ACCOUNTS_SKILLS, "UPDATE tw_accounts_skills SET UsedByEmoticon = ? WHERE SkillID = ? AND UserID = ?",
			Db::Bind(UsedByEmoticon, SkillID, AccountID));
	}

	Db::CRequest CreateTreeSelection(int AccountID, int SkillID, int LevelIndex, int OptionIndex)
	{
		return Db::Query(KIND_ACCOUNTS_SKILL_TREE,
			"INSERT INTO tw_accounts_skill_tree (UserID, SkillID, LevelIndex, OptionIndex) VALUES (?, ?, ?, ?)",
			Db::Bind(AccountID, SkillID, LevelIndex, OptionIndex));
	}

	Db::CRequest DeleteTreeSelection(int AccountID, int SkillID)
	{
		return Db::Query(KIND_ACCOUNTS_SKILL_TREE, "DELETE FROM tw_accounts_skill_tree WHERE UserID = ? AND SkillID = ?",
			Db::Bind(AccountID, SkillID));
	}
}
