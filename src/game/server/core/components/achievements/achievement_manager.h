/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_COMPONENTS_ACHIEVEMENT_MANAGER_H
#define GAME_SERVER_CORE_COMPONENTS_ACHIEVEMENT_MANAGER_H
#include <game/server/core/mmo_component.h>

#include "achievement_data.h"

class CAchievementManager : public MmoComponent
{
	~CAchievementManager() override
	{
		// free data
		std::ranges::for_each(CAchievement::Data(), [](auto& pair)
		{
			mrpgstd::cleaning_free_container_data(pair.second);
		});
		mrpgstd::cleaning_free_container_data(CAchievement::Data(), CAchievementInfo::Data());
	};

	void OnInit() override;
	void OnClientReset(int ClientID) override;
	bool OnPlayerMenulist(CPlayer* pPlayer, int Menulist) override;
	bool OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, int Extra1, int Extra2, int ReasonNumber, const char* pReason) override;

	void ShowMenu(CPlayer* pPlayer) const;
	void ShowGroupMenu(CPlayer* pPlayer, int Group) const;

public:
	int GetCountByGroup(int Group) const;
	int GetCompletedCountByGroup(int ClientID, int Group) const;
	int GetCount() const;
	int GetCompletedCount(int ClientID) const;
	void UpdateAchievement(CPlayer* pPlayer, int Type, int Misc, int Value, int AppendProgress) const;
};

#endif