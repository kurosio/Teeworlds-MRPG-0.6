#ifndef GAME_SERVER_CORE_COMPONENTS_DUNGEONS_DUNGEON_MANAGER_H
#define GAME_SERVER_CORE_COMPONENTS_DUNGEONS_DUNGEON_MANAGER_H
#include <game/server/core/mmo_component.h>

#include "dungeon_data.h"

class CDungeonManager : public MmoComponent
{
	~CDungeonManager() override
	{
		mystd::freeContainer(CDungeonData::Data());
	}

	void OnPreInit() override;
	bool OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, int Extra1, int Extra2, int ReasonNumber, const char* pReason) override;
	bool OnSendMenuVotes(CPlayer* pPlayer, int Menulist) override;

public:
	void InsertVotesDungeonTop(int DungeonID, class VoteWrapper* pWrapper) const;
	bool ShowDungeonsList(CPlayer* pPlayer, bool Story) const;
	void ShowInsideDungeonMenu(CPlayer* pPlayer) const;
	CDungeonData* GetDungeonByID(int DungeonID) const;
};
#endif