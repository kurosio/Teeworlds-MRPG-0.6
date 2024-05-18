/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_COMPONENTS_ACHIEVEMENT_MANAGER_H
#define GAME_SERVER_CORE_COMPONENTS_ACHIEVEMENT_MANAGER_H
#include <game/server/core/mmo_component.h>

#include "achievement_data.h"
#include "game/server/core/components/aethernet/aether_data.h"

class CAchievementManager : public MmoComponent
{
	~CAchievementManager() override
	{
		for(const auto& pAchievement : CAchievementInfo::Data())
			delete pAchievement;
		CAetherData::Data().clear();
		CAetherData::Data().shrink_to_fit();
	};

	void OnInit() override;
	void OnInitAccount(CPlayer* pPlayer) override;
	bool OnHandleMenulist(CPlayer* pPlayer, int Menulist) override;
	bool OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, int VoteID, int VoteID2, int Get, const char* GetText) override;
};

#endif