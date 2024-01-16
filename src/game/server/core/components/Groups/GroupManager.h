/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GROUP_CORE_H
#define GAME_SERVER_COMPONENT_GROUP_CORE_H
#include <game/server/core/mmo_component.h>

#include "GroupData.h"

class CGroupManager : public MmoComponent
{
	~CGroupManager() override
	{
		GroupData::Data().clear();
	}

	void OnInit() override;
	void OnInitAccount(CPlayer* pPlayer) override;
	void ShowGroupMenu(CPlayer* pPlayer);
	bool OnHandleMenulist(CPlayer* pPlayer, int Menulist, bool ReplaceMenu) override;
	bool OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, const int VoteID, const int VoteID2, int Get, const char* GetText) override;
public:
	GroupData* CreateGroup(CPlayer* pPlayer) const;
};

#endif
