/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_COMPONENTS_AUCTION_AUCTION_MANAGER_H
#define GAME_SERVER_CORE_COMPONENTS_AUCTION_AUCTION_MANAGER_H

#include <game/server/core/mmo_component.h>

class CAuctionManager : public MmoComponent
{
	~CAuctionManager() override = default;

	void OnTick() override;
	bool OnCharacterTile(CCharacter* pChr) override;
	bool OnPlayerMenulist(CPlayer* pPlayer, int Menulist) override;
	bool OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, int Extra1, int Extra2, int ReasonNumber, const char* pReason) override;

	void CreateAuctionSlot(CPlayer *pPlayer, class CAuctionSlot* pAuctionData);

	bool BuyItem(CPlayer* pPlayer, int ID);
	void ShowAuction(CPlayer* pPlayer);
};

#endif

