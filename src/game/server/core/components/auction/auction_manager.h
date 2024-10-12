/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_COMPONENTS_AUCTION_AUCTION_MANAGER_H
#define GAME_SERVER_CORE_COMPONENTS_AUCTION_AUCTION_MANAGER_H

#include <game/server/core/mmo_component.h>

class CAuctionManager : public MmoComponent
{
	~CAuctionManager() override = default;

	void OnPreInit() override;

	void OnCharacterTile(CCharacter* pChr) override;
	bool OnSendMenuVotes(CPlayer* pPlayer, int Menulist) override;
	bool OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, int Extra1, int Extra2, int ReasonNumber, const char* pReason) override;

	void CreateSlot(CPlayer *pPlayer, class CAuctionSlot* pAuctionData) const;

	bool BuySlot(CPlayer* pPlayer, int ID) const;

	void ShowAuction(CPlayer* pPlayer) const;
	void ShowCreateSlot(CPlayer* pPlayer) const;
	void ShowAuctionSlot(CPlayer* pPlayer, int ID) const;

	int GetSlotsCountByAccountID(int AccountID) const;
	int GetTotalSlotsCount() const;
	CAuctionSlot* GetSlot(int ID) const;
	void RemoveSlotByID(int ID) const;
};

#endif

