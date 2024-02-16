/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "warehouse_manager.h"

#include <game/server/gamecontext.h>

#include <game/server/core/components/Inventory/InventoryManager.h>

#include "engine/shared/config.h"

// Initialize the warehouse manager
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

// Warehouse manager tick
void CWarehouseManager::OnTick()
{
	constexpr int LifeTime = SERVER_TICK_SPEED * 3; // 3 sec
	if(Server()->Tick() % LifeTime == 0)
	{
		for(const auto pWarehouse : CWarehouse::Data())
		{
			if(pWarehouse->IsHasFlag(WF_STORAGE))
				pWarehouse->Storage().UpdateText(LifeTime);
		}
	}
}

// Warehouse manager handle tile
bool CWarehouseManager::OnHandleTile(CCharacter* pChr, int IndexCollision)
{
	CPlayer* pPlayer = pChr->GetPlayer();

	// shop zone
	if(pChr->GetHelper()->TileEnter(IndexCollision, TILE_SHOP_ZONE))
	{
		_DEF_TILE_ENTER_ZONE_IMPL(pPlayer, MENU_WAREHOUSE);
		return true;
	}
	else if(pChr->GetHelper()->TileExit(IndexCollision, TILE_SHOP_ZONE))
	{
		_DEF_TILE_EXIT_ZONE_IMPL(pPlayer);
		return true;
	}

	return false;
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
	CVoteWrapper VShop(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_SIMPLE, "Warehouse :: {STR}", pWarehouse->GetName());
	if(pWarehouse->IsHasFlag(WF_STORAGE))
	{
		VShop.Add("Information:");
		{
			VShop.BeginDepthList();
			VShop.Add("You can load and unload products in stores");
			VShop.Add("Maximum of {INT} products with you", g_Config.m_SvWarehouseProductsCanTake);
			VShop.Add("Unloading rate 1 product - 1 gold");
			VShop.EndDepthList();
		}
		VShop.AddLine();
		VShop.Add("Main:");
		{
			VShop.BeginDepthList();
			VShop.Add("\u2727 Your: {VAL} | Storage: {VAL} products", pPlayer->GetItem(itProduct)->GetValue(), pWarehouse->Storage().GetValue());
			if(pWarehouse->IsHasFlag(WF_BUY))
				VShop.AddOption("WAREHOUSE_LOAD_PRODUCTS", pWarehouse->GetID(), "Load products");
			else if(pWarehouse->IsHasFlag(WF_SELL))
				VShop.AddOption("WAREHOUSE_UNLOAD_PRODUCTS", pWarehouse->GetID(), "Unload products");
			VShop.EndDepthList();
		}
		VShop.AddLine();
	}
	VShop.AddOption("REPAIR_ITEMS", "Repair all items - FREE");
	VShop.AddLine();
	VShop.AddItemValue(pWarehouse->GetCurrency()->GetID());
	VShop.AddLine();
	CVoteWrapper::AddEmptyline(ClientID);

	/*
	 * Show trading list
	 */
	CItemDescription* pCurrency = pWarehouse->GetCurrency();
	if(pWarehouse->IsHasFlag(WF_BUY))
	{
		CVoteWrapper VItems(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_SIMPLE, "\u2725 Choose the item you want to buy");
		for(const auto& Trade : pWarehouse->GetTradingList())
		{
			const CItem* pItem = Trade.GetItem();
			const int Price = Trade.GetPrice();

			if(pItem->Info()->IsEnchantable())
			{
				const bool HasItem = pPlayer->GetItem(*pItem)->HasItem();
				VItems.AddMenu(MENU_WAREHOUSE_BUY_ITEM_SELECTED, Trade.GetID(), "[{STR}] {STR} {STR} - {VAL} {STR}", (HasItem ? "✔" : "×"),
					pItem->Info()->GetName(), pItem->StringEnchantLevel().c_str(), Price, pCurrency->GetName());
				continue;
			}

			VItems.AddMenu(MENU_WAREHOUSE_BUY_ITEM_SELECTED, Trade.GetID(), "[{VAL}] {STR}x{VAL} - {VAL} {STR}",
				pPlayer->GetItem(*pItem)->GetValue(), pItem->Info()->GetName(), pItem->GetValue(), Price, pCurrency->GetName());
		}
	}

	/*
	 * Show selling list
	 */
	if(pWarehouse->IsHasFlag(WF_SELL))
	{
		CVoteWrapper VItems(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_SIMPLE, "\u2725 Choose the item you want to sell");
		for(const auto& Trade : pWarehouse->GetTradingList())
		{
			const CItem* pItem = Trade.GetItem();
			const int Price = Trade.GetPrice();

			VItems.AddOption("WAREHOUSE_SELL_ITEM", pWarehouse->GetID(), Trade.GetID(), "[{VAL}] Sell {STR} - {VAL} {STR} per unit",
				pPlayer->GetItem(*pItem)->GetValue(), pItem->Info()->GetName(), Price, pCurrency->GetName());
		}
	}
}

void CWarehouseManager::ShowTrade(CPlayer* pPlayer, CWarehouse* pWarehouse, const TradeIdentifier& TradeID) const
{
	int ClientID = pPlayer->GetCID();

	// Check if the pWarehouse object is null
	if(!pWarehouse || !pWarehouse->GetTrade(TradeID))
	{
		CVoteWrapper(ClientID).Add("Warehouse don't work");
		return;
	}

	// Information
	CVoteWrapper VInfo(ClientID, VWF_SEPARATE_OPEN|VWF_STYLE_SIMPLE, "Information");
	VInfo.Add("You can write to reason value");
	CVoteWrapper::AddEmptyline(ClientID);

	// Variables
	CTrade* pTrade = pWarehouse->GetTrade(TradeID);
	const CItem* pItem = pTrade->GetItem();
	const int& PlayerValue = pPlayer->GetItem(pWarehouse->GetCurrency()->GetID())->GetValue();
	const bool HasItem = pPlayer->GetItem(pItem->GetID())->HasItem();
	// Show the purchase button
	CVoteWrapper VWant(ClientID, VWF_STYLE_STRICT_BOLD, "Do you want to buy?");
	if(pItem->Info()->IsEnchantable())
	{
		VWant.Add("{STR} {STR}", (HasItem ? "✔" : "×"), pItem->Info()->GetName());
	}
	else
	{
		int Value = pPlayer->GetItem(pItem->GetID())->GetValue();
		VWant.Add("{STR} (has {VAL})", pItem->Info()->GetName(), Value);
	}

	VWant.Add(pItem->Info()->GetDescription());

	if(pItem->Info()->HasAttributes())
	{
		char aAttributes[128];
		pItem->Info()->StrFormatAttributes(pPlayer, aAttributes, sizeof(aAttributes), pItem->GetEnchant());
		VWant.Add("* {STR}", aAttributes);
	}

	VWant.AddLine();

	if(pWarehouse->IsHasFlag(WF_STORAGE))
	{
		VWant.BeginDepthList();
		int MaterialCost = pTrade->GetProductsCost();
		VWant.Add("The item costs {VAL} products", MaterialCost);
		VWant.Add("Storage has {VAL} products", pWarehouse->Storage().GetValue());
		VWant.EndDepthList();
	}

	VWant.AddLine();

	{
		VWant.BeginDepthList();
		VWant.Add("The item costs {VAL} {STR}", pTrade->GetPrice(), pWarehouse->GetCurrency()->GetName());
		VWant.Add("You has {VAL} {STR}", PlayerValue, pWarehouse->GetCurrency()->GetName());
		VWant.EndDepthList();
	}

	VWant.AddLine();

	if(pItem->Info()->IsEnchantable() && HasItem)
	{
		VWant.Add("You can't buy more than one item");
		VWant.AddMenu(MENU_WAREHOUSE, "Back");
	}
	else
	{
		VWant.AddOption("WAREHOUSE_BUY_ITEM", pWarehouse->GetID(), TradeID, "Yes");
		VWant.AddMenu(MENU_WAREHOUSE, "No");
	}
	VWant.AddLine();
}

// Warehouse manager handle menulist
bool CWarehouseManager::OnHandleMenulist(CPlayer* pPlayer, int Menulist)
{
	CCharacter* pChr = pPlayer->GetCharacter();

	if(Menulist == MENU_WAREHOUSE)
	{
		CWarehouse* pWarehouse = GetWarehouse(pChr->m_Core.m_Pos);
		ShowWarehouseList(pChr->GetPlayer(), pWarehouse);
		return true;
	}

	if(Menulist == MENU_WAREHOUSE_BUY_ITEM_SELECTED)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_WAREHOUSE);

		if(pPlayer->m_VotesData.GetMenuTemporaryInteger() >= 0)
		{
			CWarehouse* pWarehouse = GetWarehouse(pChr->m_Core.m_Pos);
			ShowTrade(pPlayer, pWarehouse, pPlayer->m_VotesData.GetMenuTemporaryInteger());
		}

		return true;
	}

	return false;
}

// Warehouse manager handle vote commands
bool CWarehouseManager::OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, const int VoteID, const int VoteID2, int Get, const char* GetText)
{
	const int ClientID = pPlayer->GetCID();

	// Repair all items
	if(PPSTR(CMD, "REPAIR_ITEMS") == 0)
	{
		Core()->InventoryManager()->RepairDurabilityItems(pPlayer);
		GS()->Chat(ClientID, "All items have been repaired.");
		return true;
	}

	// Buying an item from a warehouse
	if(PPSTR(CMD, "WAREHOUSE_BUY_ITEM") == 0)
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

	// Selling items for the warehouse
	if(PPSTR(CMD, "WAREHOUSE_SELL_ITEM") == 0)
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

	// Load products into the warehouse
	if(PPSTR(CMD, "WAREHOUSE_LOAD_PRODUCTS") == 0)
	{
		WarehouseIdentifier WarehouseID = VoteID;
		CWarehouse* pWarehouse = GetWarehouse(WarehouseID);
		if(!pWarehouse || !pWarehouse->IsHasFlag(WF_STORAGE))
			return true;

		// Check if the player has a product in the backpack
		CPlayerItem* pProducts = pPlayer->GetItem(itProduct);
		if(!pProducts->HasItem())
		{
			GS()->Chat(ClientID, "You don't have a product in your backpack.");
			return true;
		}

		// Loading products into the warehouse
		int Value = pProducts->GetValue();
		if(pPlayer->Account()->SpendCurrency(Value, itProduct))
		{
			pWarehouse->Storage().Add(Value);
			pPlayer->Account()->AddGold(Value);
			GS()->Chat(ClientID, "You loaded {VAL} products. Got {VAL} gold.", Value, Value);
			pPlayer->m_VotesData.UpdateCurrentVotes();
		}
		return true;
	}

	// Unloading products from the warehouse
	if(PPSTR(CMD, "WAREHOUSE_UNLOAD_PRODUCTS") == 0)
	{
		WarehouseIdentifier WarehouseID = VoteID;
		CWarehouse* pWarehouse = GetWarehouse(WarehouseID);
		if(!pWarehouse || !pWarehouse->IsHasFlag(WF_STORAGE))
			return true;

		// Check if the warehouse storage is empty
		if(pWarehouse->Storage().GetValue() <= 0)
		{
			GS()->Chat(ClientID, "Warehouse storage is empty.");
			return true;
		}

		// Check if the player has maximum products
		CPlayerItem* pProducts = pPlayer->GetItem(itProduct);
		if(pProducts->GetValue() >= g_Config.m_SvWarehouseProductsCanTake)
		{
			GS()->Chat(ClientID, "You can't take more than {VAL} products.", g_Config.m_SvWarehouseProductsCanTake);
			return true;
		}

		// Unloading products from the warehouse
		int Value = minimum(pWarehouse->Storage().GetValue(), g_Config.m_SvWarehouseProductsCanTake);
		if(pWarehouse->Storage().Remove(Value))
		{
			pProducts->Add(Value);
			GS()->Chat(ClientID, "You unloaded {VAL} products.", Value);
			pPlayer->m_VotesData.UpdateCurrentVotes();
		}
		return true;
	}
	return false;
}

// Buying an item from a warehouse
bool CWarehouseManager::BuyItem(CPlayer* pPlayer, CWarehouse* pWarehouse, TradeIdentifier ID) const
{
	// Finding a warehouse by ID
	if(!pWarehouse || !pWarehouse->GetTrade(ID))
		return false;

	// Finding a trade slot by ID
	CTrade* pTradeSlot = pWarehouse->GetTrade(ID);
	CPlayerItem* pPlayerItem = pPlayer->GetItem(pTradeSlot->GetItem()->GetID());

	// Check if the player has an item in the backpack and the item is enchantable
	const int ClientID = pPlayer->GetCID();
	if(pPlayerItem->HasItem() && pPlayerItem->Info()->IsEnchantable())
	{
		GS()->Chat(ClientID, "Enchant item maximal count x1 in a backpack!");
		return false;
	}

	// Check materials in storage
	if(pWarehouse->IsHasFlag(WF_STORAGE))
	{
		int MaterialCost = pTradeSlot->GetProductsCost();
		if(pWarehouse->Storage().GetValue() < MaterialCost)
		{
			GS()->Chat(ClientID, "Not enought materials in storage! Required {VAL} materials.", MaterialCost);
			return false;
		}
	}

	// Purchase of an item
	if(pPlayer->Account()->SpendCurrency(pTradeSlot->GetPrice(), pWarehouse->GetCurrency()->GetID()))
	{
		// Remove material cost from storage
		if(pWarehouse->IsHasFlag(WF_STORAGE))
			pWarehouse->Storage().Remove(pTradeSlot->GetProductsCost());

		CItem* pItem = pTradeSlot->GetItem();
		pPlayerItem->Add(pItem->GetValue(), 0, pItem->GetEnchant());
		GS()->Chat(ClientID, "You exchanged {STR}x{VAL} for {STR}x{VAL}.",
			pWarehouse->GetCurrency()->GetName(), pTradeSlot->GetPrice(), pItem->Info()->GetName(), pItem->GetValue());
		return true;
	}

	// Failed to purchase an item
	return false;
}

bool CWarehouseManager::SellItem(CPlayer* pPlayer, CWarehouse* pWarehouse, TradeIdentifier ID, int Value) const
{
	// Finding a warehouse by ID
	if(!pWarehouse || !pWarehouse->GetTrade(ID))
		return false;

	CTrade* pTrade = pWarehouse->GetTrade(ID);
	CItem* pItem = pTrade->GetItem();

	// If the available value is less than or equal to 0, return true
	int AvailableValue = Core()->InventoryManager()->GetUnfrozenItemValue(pPlayer, pItem->GetID());
	if(AvailableValue <= 0)
		return true;

	// Set Get to the minimum value between Get and AvailableValue
	Value = minimum(Value, AvailableValue);

	const int& ClientID = pPlayer->GetCID();
	const int& Price = pTrade->GetPrice();
	int FinalPrice = Value * Price;

	// If the player's account has enough currency to spend, proceed with the transaction
	if(pPlayer->Account()->SpendCurrency(Value, pItem->GetID()))
	{
		// Add material cost to storage
		if(pWarehouse->IsHasFlag(WF_STORAGE))
		{
			int MaterialCost = pTrade->GetProductsCost() * Value;
			pWarehouse->Storage().Add(MaterialCost);
		}

		// Add the total price to the player's
		pPlayer->GetItem(pWarehouse->GetCurrency()->GetID())->Add(FinalPrice);
		GS()->Chat(ClientID, "You sold {STR}x{VAL} for {VAL} {STR}.", pItem->Info()->GetName(), Value, FinalPrice, pWarehouse->GetCurrency()->GetName());
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
