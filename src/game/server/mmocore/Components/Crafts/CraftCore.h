/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_CRAFT_CORE_H
#define GAME_SERVER_COMPONENT_CRAFT_CORE_H
#include <game/server/mmocore/MmoComponent.h>

#include "CraftData.h"

class CCraftCore : public MmoComponent
{
	~CCraftCore() override
	{
		CCraftData::ms_aCraft.clear();
	};

	void OnInit() override;
	bool OnHandleTile(CCharacter* pChr, int IndexCollision) override;
	bool OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, int VoteID, int VoteID2, int Get, const char* GetText) override;
	bool OnHandleMenulist(CPlayer* pPlayer, int Menulist, bool ReplaceMenu) override;

	int GetFinalPrice(CPlayer* pPlayer, int CraftID) const;
	void CraftItem(CPlayer* pPlayer, int CraftID) const;
	void ShowCraftList(CPlayer* pPlayer, const char* TypeName, int SelectType) const;
};

#endif