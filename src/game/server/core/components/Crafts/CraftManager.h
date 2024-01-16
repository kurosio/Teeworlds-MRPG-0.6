/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_CRAFT_CORE_H
#define GAME_SERVER_COMPONENT_CRAFT_CORE_H
#include <game/server/core/mmo_component.h>

#include "CraftData.h"

class CCraftManager : public MmoComponent
{
	~CCraftManager() override
	{
		CCraftItem::Data().clear();
	};

	void OnInit() override;
	bool OnHandleTile(CCharacter* pChr, int IndexCollision) override;
	bool OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, int VoteID, int VoteID2, int Get, const char* GetText) override;
	bool OnHandleMenulist(CPlayer* pPlayer, int Menulist, bool ReplaceMenu) override;
	CCraftItem* GetCraftByID(CraftIdentifier ID) const;

	void CraftItem(CPlayer* pPlayer, CCraftItem* pCraft) const;
	void ShowCraftList(CPlayer* pPlayer, const char* TypeName, ItemType Type) const;
};

#endif