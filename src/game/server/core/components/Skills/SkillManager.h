/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_SKILL_CORE_H
#define GAME_SERVER_COMPONENT_SKILL_CORE_H
#include <game/server/core/mmo_component.h>

#include "SkillData.h"

class CSkillManager : public MmoComponent
{
	~CSkillManager() override
	{
		CSkillDescription::Data().clear();
		CSkill::Data().clear();
	};

	void OnInit() override;
	void OnInitAccount(CPlayer* pPlayer) override;
	void OnResetClient(int ClientID) override;
	bool OnHandleTile(CCharacter* pChr, int IndexCollision) override;
	bool OnHandleMenulist(CPlayer* pPlayer, int Menulist, bool ReplaceMenu) override;
	bool OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, int VoteID, int VoteID2, int Get, const char* GetText) override;

public:
	void ParseEmoticionSkill(CPlayer* pPlayer, int EmoticionID);

private:
	void ShowMailSkillList(CPlayer* pPlayer, SkillType Type) const;
	void ShowSkill(CPlayer* pPlayer, SkillIdentifier ID) const;
};

#endif
