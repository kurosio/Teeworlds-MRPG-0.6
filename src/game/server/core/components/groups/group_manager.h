/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GROUP_CORE_H
#define GAME_SERVER_COMPONENT_GROUP_CORE_H
#include <game/server/core/mmo_component.h>

#include "group_data.h"

class CGroupManager : public MmoComponent
{
	~CGroupManager() override
	{
		// free data
		mystd::freeContainer(GroupData::Data());
	}

	void OnPreInit() override;
	void OnPlayerLogin(CPlayer* pPlayer) override;
	void ShowGroupMenu(CPlayer* pPlayer) const;
	bool OnSendMenuVotes(CPlayer* pPlayer, int Menulist) override;
	bool OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, int Extra1, int Extra2, int ReasonNumber, const char* pReason) override;

public:
	GroupData* CreateGroup(CPlayer* pPlayer) const;
	GroupData* GetGroupByID(GroupIdentifier ID) const;
};

#endif
