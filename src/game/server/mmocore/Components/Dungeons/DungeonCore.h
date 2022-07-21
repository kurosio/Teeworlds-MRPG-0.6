/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_DUNGEON_CORE_H
#define GAME_SERVER_COMPONENT_DUNGEON_CORE_H
#include <game/server/mmocore/MmoComponent.h>

#include "DungeonData.h"

class DungeonCore : public MmoComponent
{
	~DungeonCore() override
	{
		CDungeonData::ms_aDungeon.clear();
	};

	void OnInit() override;
	bool OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, int VoteID, int VoteID2, int Get, const char* GetText) override;
	bool OnHandleMenulist(CPlayer* pPlayer, int Menulist, bool ReplaceMenu) override;

public:
	static bool IsDungeonWorld(int WorldID);
	static void SaveDungeonRecord(CPlayer* pPlayer, int DungeonID, CPlayerDungeonRecord *pPlayerDungeonRecord);
	void ShowDungeonTop(CPlayer* pPlayer, int DungeonID, int HideID) const;
	void ShowDungeonsList(CPlayer* pPlayer, bool Story) const;
	void CheckQuestingOpened(CPlayer* pPlayer, int QuestID) const;
	void ShowTankVotingDungeon(CPlayer* pPlayer) const;
};
#endif