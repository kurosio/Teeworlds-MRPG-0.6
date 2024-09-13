/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_COMPONENTS_ACHIEVEMENT_MANAGER_H
#define GAME_SERVER_CORE_COMPONENTS_ACHIEVEMENT_MANAGER_H
#include <game/server/core/mmo_component.h>

#include "achievement_data.h"
#include "game/server/core/tools/vote_wrapper.h"

class CAchievementManager : public MmoComponent
{
	~CAchievementManager() override
	{
		mystd::freeContainer(CAchievement::Data(), CAchievementInfo::Data());
	};

	void OnInit() override;
	void OnClientReset(int ClientID) override;
	bool OnSendMenuVotes(CPlayer* pPlayer, int Menulist) override;

	void ShowMenu(CPlayer* pPlayer) const;
	void ShowGroupMenu(CPlayer* pPlayer, int Group) const;
	void AddAchievementDetails(VoteWrapper& VAchievement, const CAchievementInfo* pInfo, int Progress, int Required) const;

public:
	void UpdateAchievement(CPlayer* pPlayer, AchievementType Type, int Criteria, int Progress, int AppendProgress) const;
	int GetCountByGroup(int Group) const;
	int GetCompletedCountByGroup(int ClientID, int Group) const;
	int GetCount() const;
	int GetCompletedCount(int ClientID) const;
};

#endif