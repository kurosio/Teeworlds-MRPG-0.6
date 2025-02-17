/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_WAREHOUSE_CORE_H
#define GAME_SERVER_COMPONENT_WAREHOUSE_CORE_H
#include <game/server/core/mmo_component.h>

#include "warehouse_data.h"

class CWarehouseManager : public MmoComponent
{
	~CWarehouseManager() override
	{
		mystd::freeContainer(CWarehouse::Data());
	}

	void OnPreInit() override;
	void OnTick() override;
	void OnCharacterTile(CCharacter* pChr) override;
	bool OnSendMenuVotes(CPlayer* pPlayer, int Menulist) override;
	bool OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, int Extra1, int Extra2, int ReasonNumber, const char* pReason) override;

	// vote menus
	void ShowWarehouseList(CPlayer* pPlayer, CWarehouse* pWarehouse) const;
	void ShowTradeList(CWarehouse* pWarehouse, CPlayer* pPlayer, const char* TypeName, ItemGroup Type) const;
	void ShowTrade(CPlayer* pPlayer, CWarehouse* pWarehouse, int TradeID) const;

	// functions
	bool BuyItem(CPlayer* pPlayer, CWarehouse* pWarehouse, int TradeID) const;
	bool SellItem(CPlayer* pPlayer, CWarehouse* pWarehouse, int TradeID, int Value) const;

public:
	CWarehouse* GetWarehouse(vec2 Pos) const;
	CWarehouse* GetWarehouse(int WarehouseID) const;
};

#endif