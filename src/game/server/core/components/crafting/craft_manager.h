/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_CRAFT_CORE_H
#define GAME_SERVER_COMPONENT_CRAFT_CORE_H
#include <game/server/core/mmo_component.h>

#include "craft_data.h"

class CCraftManager : public MmoComponent
{
	std::vector<std::pair<std::string, std::deque<CCraftItem*>>> m_vOrderedCraftList {};
	void InitCraftGroup(const std::string& GroupName, const std::vector<int>& Items);

	~CCraftManager() override
	{
		m_vOrderedCraftList.clear();
		mystd::freeContainer(CCraftItem::Data());
	};

	void OnPreInit() override;
	void OnInitWorld(const std::string& Where) override;
	void OnCharacterTile(CCharacter* pChr) override;
	bool OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, int Extra1, int Extra2, int ReasonNumber, const char* pReason) override;
	bool OnSendMenuVotes(CPlayer* pPlayer, int Menulist) override;

	void ShowCraftGroup(CPlayer* pPlayer, const std::string& GroupName, const std::deque<CCraftItem*>& vItems) const;
	void ShowCraftItem(CPlayer* pPlayer, CCraftItem* pCraft) const;
	void CraftItem(CPlayer* pPlayer, CCraftItem* pCraft) const;

public:
	CCraftItem* GetCraftByID(CraftIdentifier ID) const;
};

#endif