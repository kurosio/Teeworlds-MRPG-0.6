/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_SHOP_CORE_H
#define GAME_SERVER_COMPONENT_SHOP_CORE_H
#include <game/server/mmocore/MmoComponent.h>

#include "ShopData.h"

class CShopCore : public MmoComponent
{
	~CShopCore() override
	{
		CShop::ms_aShopList.clear();
	};

	void OnInit() override;
	void OnTick() override;
	bool OnHandleTile(CCharacter* pChr, int IndexCollision) override;
	bool OnHandleMenulist(CPlayer* pPlayer, int Menulist, bool ReplaceMenu) override;
	bool OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, int VoteID, int VoteID2, int Get, const char* GetText) override;

public:
	void CreateAuctionSlot(CPlayer *pPlayer, CAuctionItem &pAuctionItem);
	void CheckAuctionTime();

private:
	bool BuyShopItem(CPlayer* pPlayer, int ID);
	void ShowAuction(CPlayer* pPlayer);
	void ShowMailShop(CPlayer* pPlayer, int StorageID);
};

#endif

