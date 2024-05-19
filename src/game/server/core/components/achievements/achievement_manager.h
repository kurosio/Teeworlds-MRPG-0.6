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
		// free achievement data
		for(auto& p : CAchievement::Data())
		{
			for(auto& pAchievement : p.second)
				delete pAchievement.second;
		}
		CAchievement::Data().clear();

		// free achievement information data
		for(auto& p : CAchievementInfo::Data())
			delete p;
		CAchievementInfo::Data().clear();
		CAchievementInfo::Data().shrink_to_fit();
	};

	void OnInit() override;
	void OnResetClient(int ClientID) override;
	bool OnHandleMenulist(CPlayer* pPlayer, int Menulist) override;
	bool OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, int VoteID, int VoteID2, int Get, const char* GetText) override;

public:
	void UpdateAchievement(CPlayer* pPlayer, int Type, int Misc, int Value, int AppendProgress) const;
};

#endif