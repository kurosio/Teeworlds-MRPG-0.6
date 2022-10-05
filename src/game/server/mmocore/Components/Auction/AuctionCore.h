/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_AUCTION_CORE_H
#define GAME_SERVER_COMPONENT_AUCTION_CORE_H

#include <game/server/mmocore/MmoComponent.h>

class CAuctionCore : public MmoComponent
{
	~CAuctionCore() override = default;

	void OnTick() override;
	bool OnHandleTile(CCharacter* pChr, int IndexCollision) override;
	bool OnHandleMenulist(CPlayer* pPlayer, int Menulist, bool ReplaceMenu) override;
	bool OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, int VoteID, int VoteID2, int Get, const char* GetText) override;

private:
	void CreateAuctionSlot(CPlayer *pPlayer, class CAuctionSlot* pAuctionData);
	void CheckAuctionTime() const;

	bool BuyItem(CPlayer* pPlayer, int ID);
	void ShowAuction(CPlayer* pPlayer);
};

#endif

