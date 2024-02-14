/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "warehouse_manager.h"

#include <game/server/gamecontext.h>

#include <game/server/core/components/Inventory/InventoryManager.h>

constexpr auto TW_WAREHOUSE_TABLE = "tw_warehouses";
constexpr auto TW_WAREHOUSE_ITEMS_TABLE = "tw_warehouse_items";

void CWarehouseManager::OnInit()
{
	// init warehouses
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_WAREHOUSE_TABLE);
	std::unordered_map< int, ContainerTradingList > TradesSlots;
	while(pRes->next())
	{
		WarehouseIdentifier ID = pRes->getInt("ID");
		std::string Name = pRes->getString("Name").c_str();
		std::string Properties = pRes->getString("Properties").c_str();
		vec2 Pos = vec2((float)pRes->getInt("PosX"), (float)pRes->getInt("PosY"));
		int Currency = pRes->getInt("Currency");
		int WorldID = pRes->getInt("WorldID");

		// init by server
		CWarehouse::CreateElement(ID)->Init(Name, Properties, Pos, Currency, WorldID);
	}
}

bool CWarehouseManager::OnHandleTile(CCharacter* pChr, int IndexCollision)
{
	CPlayer* pPlayer = pChr->GetPlayer();
	const int ClientID = pPlayer->GetCID();

	// shop zone
	if(pChr->GetHelper()->TileEnter(IndexCollision, TILE_SHOP_ZONE))
	{
		_DEF_TILE_ENTER_ZONE_IMPL(pPlayer, MENU_WAREHOUSE_SHOPPING_LIST);
		return true;
	}
	else if(pChr->GetHelper()->TileExit(IndexCollision, TILE_SHOP_ZONE))
	{
		_DEF_TILE_EXIT_ZONE_IMPL(pPlayer);
		return true;
	}

	// selling zone
	if(pChr->GetHelper()->TileEnter(IndexCollision, TILE_ORE_SELL) || pChr->GetHelper()->TileEnter(IndexCollision, TILE_PLANT_SELL))
	{
		_DEF_TILE_ENTER_ZONE_IMPL(pPlayer, MENU_WAREHOUSE_SELLING_LIST);
		return true;
	}
	else if(pChr->GetHelper()->TileExit(IndexCollision, TILE_ORE_SELL) || pChr->GetHelper()->TileExit(IndexCollision, TILE_PLANT_SELL))
	{
		_DEF_TILE_EXIT_ZONE_IMPL(pPlayer);
		return true;
	}

	return false;
}

bool CWarehouseManager::OnHandleMenulist(CPlayer* pPlayer, int Menulist)
{
	CCharacter* pChr = pPlayer->GetCharacter();

	// shopping type
	if(Menulist == MENU_WAREHOUSE_SHOPPING_LIST)
	{
		CWarehouse* pWarehouse = GetWarehouse(pChr->m_Core.m_Pos);
		ShowWarehouseList(pChr->GetPlayer(), pWarehouse);

		return true;
	}

	// selling type
	if(Menulist == MENU_WAREHOUSE_SELLING_LIST)
	{
		if(pChr->GetHelper()->BoolIndex(TILE_ORE_SELL))
		{
			Core()->InventoryManager()->ShowSellingItemsByFunction(pPlayer, FUNCTION_MINER);
			return true;
		}

		if(pChr->GetHelper()->BoolIndex(TILE_PLANT_SELL))
		{
			Core()->InventoryManager()->ShowSellingItemsByFunction(pPlayer, FUNCTION_PLANT);
			return true;
		}

		return true;
	}

	return false;
}

bool CWarehouseManager::OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, const int VoteID, const int VoteID2, int Get, const char* GetText)
{
	const int ClientID = pPlayer->GetCID();
	if(PPSTR(CMD, "REPAIR_ITEMS") == 0)
	{
		Core()->InventoryManager()->RepairDurabilityItems(pPlayer);
		GS()->Chat(ClientID, "You repaired all items.");
		return true;
	}

	if(PPSTR(CMD, "BUY_ITEM") == 0)
	{
		const WarehouseIdentifier& WarehouseID = VoteID;
		const TradeIdentifier& TradeID = VoteID2;

		CWarehouse* pWarehouse = GetWarehouse(WarehouseID);
		if(BuyItem(pPlayer, pWarehouse, TradeID))
		{
			pPlayer->m_VotesData.UpdateCurrentVotes();
		}

		return true;
	}

	if(PPSTR(CMD, "SELL_ITEM") == 0)
	{
		const WarehouseIdentifier& WarehouseID = VoteID;
		const TradeIdentifier& TradeID = VoteID2;

		CWarehouse* pWarehouse = GetWarehouse(WarehouseID);
		if(SellItem(pPlayer, pWarehouse, TradeID, Get))
		{
			pPlayer->m_VotesData.UpdateCurrentVotes();
		}

		return true;
	}

	return false;
}

void CWarehouseManager::ShowWarehouseSellingList(CPlayer* pPlayer, const CWarehouse* pWarehouse) const
{
	const int ClientID = pPlayer->GetCID();

	CVoteWrapper VItems(ClientID, VWFLAG_SEPARATE_OPEN | VWFLAG_STYLE_SIMPLE, "Sale of items from the list is available!");
	VItems.Add("Choose the item you want to sell");
	{
		VItems.BeginDepthList();
		for(auto& Trade : pWarehouse->GetTradingList())
		{
			const CItem* pTradeItem = Trade.GetTradeItem();
			CPlayerItem* pPlayerItem = pPlayer->GetItem(pTradeItem->GetID());
			const int& Price = Trade.GetPrice();

			VItems.AddOption("SELL_ITEM", pWarehouse->GetID(), Trade.GetID(), "[{VAL}] Sell {STR} ({VAL} gold's per unit)", 
				pPlayerItem->GetValue(), pTradeItem->Info()->GetName(), Price);
		}
		VItems.EndDepthList();
	}
	VItems.AddLine();
}

// Displaying the trading list of the warehouse
void CWarehouseManager::ShowWarehouseTradingList(CPlayer* pPlayer, const CWarehouse* pWarehouse) const
{
	const int ClientID = pPlayer->GetCID();

	// show trade list
	for(auto& Trade : pWarehouse->GetTradingList())
	{
		CItemDescription* pCurrency = pWarehouse->GetCurrency();
		const CItem* pTrade = Trade.GetTradeItem();
		const int& Price = Trade.GetPrice();

		CVoteWrapper VItem(ClientID, VWFLAG_UNIQUE | VWFLAG_STYLE_SIMPLE);
		if(pTrade->Info()->IsEnchantable())
		{
			const bool PlayerHasItem = pPlayer->GetItem(*pTrade)->HasItem();
			VItem.SetTitle("({STR}){STR} {STR} - {VAL} {STR}", (PlayerHasItem ? "✔" : "×"), 
				pTrade->Info()->GetName(), pTrade->StringEnchantLevel().c_str(), Price, pCurrency->GetName());

			char aAttributes[128];
			pTrade->Info()->StrFormatAttributes(pPlayer, aAttributes, sizeof(aAttributes), pTrade->GetEnchant());
			VItem.Add("* {STR}", aAttributes);
		}
		else
		{
			VItem.SetTitle("({VAL}){STR}x{VAL} - {VAL} {STR}",
				pPlayer->GetItem(*pTrade)->GetValue(), pTrade->Info()->GetName(), pTrade->GetValue(), Price, pCurrency->GetName());
		}
		VItem.Add(Instance::Localize(ClientID, pTrade->Info()->GetDescription()));
		VItem.AddOption("BUY_ITEM", pWarehouse->GetID(), Trade.GetID(), "Buy {STR}x{VAL}", pTrade->Info()->GetName(), pTrade->GetValue());
	}
}

void CWarehouseManager::ShowWarehouseList(CPlayer* pPlayer, CWarehouse* pWarehouse) const
{
	const int ClientID = pPlayer->GetCID();

	// Check if the pWarehouse object is null
	if(!pWarehouse)
	{
		CVoteWrapper(ClientID).Add("Warehouse don't work");
		return;
	}

	// show base shop functions
	CVoteWrapper VShop(ClientID, VWFLAG_SEPARATE_OPEN | VWFLAG_STYLE_SIMPLE, "Warehouse :: {STR}", pWarehouse->GetName());
	VShop.Add("Repair all items - FREE", "REPAIR_ITEMS");
	VShop.AddLine();
	VShop.AddItemValue(pWarehouse->GetCurrency()->GetID());
	VShop.AddLine();
	CVoteWrapper::AddEmptyline(ClientID);

	if(pWarehouse->GetType() == WarehouseType::Shop)
		ShowWarehouseTradingList(pPlayer, pWarehouse);
	else if(pWarehouse->GetType() == WarehouseType::Selling)
		ShowWarehouseSellingList(pPlayer, pWarehouse);
}

// Buying an item from a warehouse
bool CWarehouseManager::BuyItem(CPlayer* pPlayer, CWarehouse* pWarehouse, TradeIdentifier ID) const
{
	// Finding a warehouse by ID
	if(!pWarehouse || !pWarehouse->GetTradeSlot(ID))
		return false;

	// Finding a trade slot by ID
	CTradeSlot* pTradeSlot = pWarehouse->GetTradeSlot(ID);
	CPlayerItem* pPlayerItem = pPlayer->GetItem(pTradeSlot->GetTradeItem()->GetID());

	// Check if the player has an item in the backpack and the item is enchantable
	const int ClientID = pPlayer->GetCID();
	if(pPlayerItem->HasItem() && pPlayerItem->Info()->IsEnchantable())
	{
		GS()->Chat(ClientID, "Enchant item maximal count x1 in a backpack!");
		return false;
	}

	// Purchase of an item
	if(pPlayer->Account()->SpendCurrency(pTradeSlot->GetPrice(), pWarehouse->GetCurrency()->GetID()))
	{
		CItem* pTradeItem = pTradeSlot->GetTradeItem();
		pPlayerItem->Add(pTradeItem->GetValue(), 0, pTradeItem->GetEnchant());
		GS()->Chat(ClientID, "You exchanged {STR}x{VAL} for {STR}x{VAL}.", 
			pWarehouse->GetCurrency()->GetName(), pTradeSlot->GetPrice(), pTradeItem->Info()->GetName(), pTradeItem->GetValue());
		return true;
	}

	// Failed to purchase an item
	return false;
}

bool CWarehouseManager::SellItem(CPlayer* pPlayer, CWarehouse* pWarehouse, TradeIdentifier ID, int Value) const
{
	// Finding a warehouse by ID
	if(!pWarehouse || !pWarehouse->GetTradeSlot(ID))
		return false;

	CTradeSlot* pTradeSlot = pWarehouse->GetTradeSlot(ID);
	CItem* pTradeItem = pTradeSlot->GetTradeItem();

	// If the available value is less than or equal to 0, return true
	int AvailableValue = Core()->InventoryManager()->GetUnfrozenItemValue(pPlayer, pTradeItem->GetID());
	if(AvailableValue <= 0)
		return true;

	// Set Get to the minimum value between Get and AvailableValue
	Value = minimum(Value, AvailableValue);

	const int& ClientID = pPlayer->GetCID();
	const int& Price = pTradeSlot->GetPrice();
	int FinalPrice = Value * Price;

	// If the player's account has enough currency to spend, proceed with the transaction
	if(pPlayer->Account()->SpendCurrency(Value, pTradeItem->GetID()))
	{
		// Add the total price to the player's gold
		pPlayer->GetItem(pWarehouse->GetCurrency()->GetID())->Add(FinalPrice);
		GS()->Chat(ClientID, "You sold {STR}x{VAL} for {VAL} {STR}.", pTradeItem->Info()->GetName(), Value, FinalPrice, pWarehouse->GetCurrency()->GetName());
		return true;
	}

	return false;
}

// Finding a warehouse by position
CWarehouse* CWarehouseManager::GetWarehouse(vec2 Pos) const
{
	auto iter = std::find_if(CWarehouse::Data().begin(), CWarehouse::Data().end(), [Pos](const CWarehouse* pItem)
		{ return distance(pItem->GetPos(), Pos) < 320; });
	return iter != CWarehouse::Data().end() ? *iter : nullptr;
}

// Finding a warehouse by ID
CWarehouse* CWarehouseManager::GetWarehouse(WarehouseIdentifier ID) const
{
	auto iter = std::find_if(CWarehouse::Data().begin(), CWarehouse::Data().end(), [ID](const CWarehouse* pItem){ return pItem->GetID() == ID; });
	return iter != CWarehouse::Data().end() ? *iter : nullptr;
}
