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
		mystd::freeContainer(CSkill::Data(), CSkillDescription::Data());
	}

	void OnPreInit() override;
	void OnPlayerLogin(CPlayer* pPlayer) override;
	void OnClientReset(int ClientID) override;
	bool OnSendMenuVotes(CPlayer* pPlayer, int Menulist) override;
	bool OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, const std::vector<std::any> &Extras, int ReasonNumber, const char* pReason) override;

	// vote list's menus
	void ShowSkill(CPlayer* pPlayer, int SkillID);

public:
	// use skills by Emoticon
	void UseSkillsByEmoticon(CPlayer* pPlayer, int EmoticonID);
};

#endif
