/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_COMPONENTS_ACHIEVEMENTS_ACHIEVEMENT_MANAGER_H
#define GAME_SERVER_CORE_COMPONENTS_ACHIEVEMENTS_ACHIEVEMENT_MANAGER_H
#include <game/server/core/mmo_component.h>

#include "achievement_data.h"
#include "game/server/core/tools/vote_wrapper.h"

class CAchievementManager : public MmoComponent
{
	struct SAchievementChain
	{
		AchievementType m_Type {};
		std::vector<int> m_vIDs {};
	};

	// Shared across worlds: only the initializer world runs OnPreInit().
	static inline std::vector<SAchievementChain> m_vChains {};

	~CAchievementManager() override
	{
		mystd::freeContainer(CAchievement::Data(), CAchievementInfo::Data());
	};

	void OnPreInit() override;
	void OnPostInit() override;
	void OnClientReset(int ClientID) override;
	bool OnSendMenuVotes(CPlayer* pPlayer, int Menulist) override;

	void BuildChains();
	void ShowMenu(CPlayer* pPlayer) const;
	void ShowTypeList(CPlayer* pPlayer, AchievementType Type) const;
	void AddAchievementDetails(VoteWrapper& Vote, const CAchievementInfo* pInfo, int Progress, int Level, int TotalLevels) const;
};

#endif
