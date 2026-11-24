#ifndef GAME_SERVER_CORE_COMPONENTS_SKILLS_SKILL_DB_H
#define GAME_SERVER_CORE_COMPONENTS_SKILLS_SKILL_DB_H

#include <engine/server/db/db_async.h>

namespace SkillDb
{
	Db::CResultSet ListSkills();
	Db::CResultSet ListTree();

	Db::CRequest LoadAccountSkills(int AccountID);
	Db::CRequest LoadAccountTreeSelections(int AccountID);
	Db::CRequest CreateAccountSkill(int SkillID, int AccountID, int UsedByEmoticon);
	Db::CRequest SaveUsedByEmoticon(int SkillID, int AccountID, int UsedByEmoticon);

	Db::CRequest CreateTreeSelection(int AccountID, int SkillID, int LevelIndex, int OptionIndex);
	Db::CRequest DeleteTreeSelection(int AccountID, int SkillID);
}

#endif // GAME_SERVER_COMPONENT_SKILLS_SKILL_DB_H
