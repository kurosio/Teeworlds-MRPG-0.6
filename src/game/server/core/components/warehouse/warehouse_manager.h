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

	void ShowWarehouseList(CPlayer* pPlayer, CWarehouse* pWarehouse) const;
	void ShowGroupedSelector(CPlayer* pPlayer, CWarehouse* pWarehouse, bool IsBuyingAction) const;
	void ShowTrade(CPlayer* pPlayer, CWarehouse* pWarehouse, int TradeID) const;

	bool BuyItem(CPlayer* pPlayer, CWarehouse* pWarehouse, int TradeID) const;
	bool SellItem(CPlayer* pPlayer, CWarehouse* pWarehouse, int TradeID, int Value) const;

public:
	CWarehouse* GetWarehouse(vec2 Pos) const;
	CWarehouse* GetWarehouse(int WarehouseID) const;
};

#endif