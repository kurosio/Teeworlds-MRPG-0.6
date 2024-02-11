/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_WAREHOUSE_CORE_H
#define GAME_SERVER_COMPONENT_WAREHOUSE_CORE_H
#include <game/server/core/mmo_component.h>

#include "WarehouseData.h"

class CWarehouseManager : public MmoComponent
{
	~CWarehouseManager() override
	{
		CWarehouse::Data().clear();
	}

	void OnInit() override;
	bool OnHandleTile(CCharacter* pChr, int IndexCollision) override;
	bool OnHandleMenulist(CPlayer* pPlayer, int Menulist) override;
	bool OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, int VoteID, int VoteID2, int Get, const char* GetText) override;

	void ShowWarehouseMenu(CPlayer *pPlayer, const CWarehouse* pWarehouse) const;
	bool BuyItem(CPlayer* pPlayer, int WarehouseID, TradeIdentifier ID) const;

public:
	CWarehouse* GetWarehouse(vec2 Pos) const;
};

#endif