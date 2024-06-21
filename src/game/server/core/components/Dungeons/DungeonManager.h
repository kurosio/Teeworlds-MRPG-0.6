/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_DUNGEON_CORE_H
#define GAME_SERVER_COMPONENT_DUNGEON_CORE_H
#include <game/server/core/mmo_component.h>

#include "DungeonData.h"

class CDungeonManager : public MmoComponent
{
	~CDungeonManager() override
	{
		// free data
		mrpgstd::free_container(CDungeonData::ms_aDungeon);
	};

	void OnInit() override;
	bool OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, int Extra1, int Extra2, int ReasonNumber, const char* pReason) override;
	bool OnPlayerMenulist(CPlayer* pPlayer, int Menulist) override;

public:
	static bool IsDungeonWorld(int WorldID);
	static void SaveDungeonRecord(CPlayer* pPlayer, int DungeonID, CPlayerDungeonRecord *pPlayerDungeonRecord);
	void InsertVotesDungeonTop(int DungeonID, class VoteWrapper* pWrapper) const;
	bool ShowDungeonsList(CPlayer* pPlayer, bool Story) const;
	void NotifyUnlockedDungeonsByQuest(CPlayer* pPlayer, int QuestID) const;
	void ShowInsideDungeonMenu(CPlayer* pPlayer) const;
};
#endif