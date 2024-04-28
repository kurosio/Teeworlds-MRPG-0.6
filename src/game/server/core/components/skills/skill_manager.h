/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_SKILL_CORE_H
#define GAME_SERVER_COMPONENT_SKILL_CORE_H
#include <game/server/core/mmo_component.h>

#include "skill_data.h"

class CSkillManager : public MmoComponent
{
	~CSkillManager() override
	{
		// clear skill's
		for(auto& p : CSkill::Data())
		{
			for(auto pData : p.second)
				delete pData;
		}

		// clear containers
		CSkill::Data().clear();
		CSkillDescription::Data().clear();
	};

	void OnInit() override;
	void OnInitAccount(CPlayer* pPlayer) override;
	void OnResetClient(int ClientID) override;
	bool OnHandleTile(CCharacter* pChr, int IndexCollision) override;
	bool OnHandleMenulist(CPlayer* pPlayer, int Menulist) override;
	bool OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, int VoteID, int VoteID2, int Get, const char* GetText) override;

	// vote list's menus
	void ShowSkillList(CPlayer* pPlayer, const char* pTitle, SkillType Type) const;
	void ShowSkill(CPlayer* pPlayer, SkillIdentifier ID) const;

public:
	// use skills by emoticion
	void UseSkillsByEmoticion(CPlayer* pPlayer, int EmoticionID);
};

#endif
