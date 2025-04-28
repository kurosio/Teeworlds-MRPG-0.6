#ifndef GAME_SERVER_CORE_COMPONENTS_DUTIES_DUTIES_MANAGER_H
#define GAME_SERVER_CORE_COMPONENTS_DUTIES_DUTIES_MANAGER_H
#include <game/server/core/mmo_component.h>

#include "dungeon_data.h"

class CDutiesManager : public MmoComponent
{
	~CDutiesManager() override
	{
		mystd::freeContainer(CDungeonData::Data());
	}

	void OnPreInit() override;
	bool OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, int Extra1, int Extra2, int ReasonNumber, const char* pReason) override;
	bool OnSendMenuVotes(CPlayer* pPlayer, int Menulist) override;

public:
	void ShowDutiesList(CPlayer* pPlayer, WorldType Type) const;

	void ShowDungeonInfo(CPlayer* pPlayer, CDungeonData* pDungeon) const;
	void ShowInsideMenu(CPlayer* pPlayer) const;

	void ShowPvpInfo(CPlayer* pPlayer, int WorldID) const;

	CDungeonData* GetDungeonByID(int DungeonID) const;
	CDungeonData* GetDungeonByWorldID(int WorldID) const;
	int GetWorldsCountByType(WorldType Type) const;
};
#endif