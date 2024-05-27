/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>
#include "warehouse_manager.h"

#include <game/server/gamecontext.h>
#include <game/server/core/components/Inventory/InventoryManager.h>

constexpr int g_UpdateTextLifeTime = SERVER_TICK_SPEED * 3;

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
	// Update warehouse text storage value
	if(Server()->Tick() % g_UpdateTextLifeTime == 0)
	{
		for(const auto pWarehouse : CWarehouse::Data())
		{
			if(pWarehouse->IsHasFlag(WF_STORAGE))
			{
				pWarehouse->Storage().UpdateText(g_UpdateTextLifeTime);
			}
		}
	}
}

// Warehouse manager handle tile
bool CWarehouseManager::OnCharacterTile(CCharacter* pChr)
{
	CPlayer* pPlayer = pChr->GetPlayer();

	// shop zone
	if(pChr->GetTiles()->IsEnter(TILE_SHOP_ZONE))
	{
		DEF_TILE_ENTER_ZONE_IMPL(pPlayer, MENU_WAREHOUSE);
		return true;
	}
	else if(pChr->GetTiles()->IsExit(TILE_SHOP_ZONE))
	{
		DEF_TILE_EXIT_ZONE_IMPL(pPlayer);
		return true;
	}

	return false;
}

// Warehouse manager handle menulist
bool CWarehouseManager::OnPlayerMenulist(CPlayer* pPlayer, int Menulist)
{
	CCharacter* pChr = pPlayer->GetCharacter();

	if(Menulist == MENU_WAREHOUSE)
	{
		// show warehouse list by position
		CWarehouse* pWarehouse = GetWarehouse(pChr->m_Core.m_Pos);
		ShowWarehouseList(pChr->GetPlayer(), pWarehouse);
		return true;
	}

	if(Menulist == MENU_WAREHOUSE_BUY_ITEM_SELECTED)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_WAREHOUSE);

		// try show trade item
		if(pPlayer->m_VotesData.GetGroupID() >= 0)
		{
			CWarehouse* pWarehouse = GetWarehouse(pChr->m_Core.m_Pos);
			ShowTrade(pPlayer, pWarehouse, pPlayer->m_VotesData.GetGroupID());
		}

		// add backpage
		VoteWrapper::AddBackpage(pPlayer->GetCID());
		return true;
	}

	return false;
}

// Warehouse manager handle vote commands
bool CWarehouseManager::OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, const int Extra1, const int Extra2, int ReasonNumber, const char* pReason)
{
	const int ClientID = pPlayer->GetCID();

	// Repair all items
	if(PPSTR(pCmd, "REPAIR_ITEMS") == 0)
	{
		Core()->InventoryManager()->RepairDurabilityItems(pPlayer);
		GS()->Chat(ClientID, "All items have been repaired.");
		return true;
	}

	// Buying an item from a warehouse
	if(PPSTR(pCmd, "WAREHOUSE_BUY_ITEM") == 0)
	{
		const WarehouseIdentifier& WarehouseID = Extra1;
		const TradeIdentifier& TradeID = Extra2;

		CWarehouse* pWarehouse = GetWarehouse(WarehouseID);
		if(BuyItem(pPlayer, pWarehouse, TradeID))
		{
			pPlayer->m_VotesData.UpdateCurrentVotes();
		}

		return true;
	}

	// Selling items for the warehouse
	if(PPSTR(pCmd, "WAREHOUSE_SELL_ITEM") == 0)
	{
		const WarehouseIdentifier& WarehouseID = Extra1;
		const TradeIdentifier& TradeID = Extra2;

		CWarehouse* pWarehouse = GetWarehouse(WarehouseID);
		if(SellItem(pPlayer, pWarehouse, TradeID, ReasonNumber))
		{
			pPlayer->m_VotesData.UpdateCurrentVotes();
		}

		return true;
	}

	// Load products into the warehouse
	if(PPSTR(pCmd, "WAREHOUSE_LOAD_PRODUCTS") == 0)
	{
		WarehouseIdentifier WarehouseID = Extra1;
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
			GS()->Chat(ClientID, "You loaded {} products. Got {} gold.", Value, Value);
			pPlayer->m_VotesData.UpdateCurrentVotes();
		}
		return true;
	}

	// Unloading products from the warehouse
	if(PPSTR(pCmd, "WAREHOUSE_UNLOAD_PRODUCTS") == 0)
	{
		WarehouseIdentifier WarehouseID = Extra1;
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
			GS()->Chat(ClientID, "You can't take more than {} products.", g_Config.m_SvWarehouseProductsCanTake);
			return true;
		}

		// Unloading products from the warehouse
		int Value = minimum(pWarehouse->Storage().GetValue(), g_Config.m_SvWarehouseProductsCanTake - pProducts->GetValue());
		if(pWarehouse->Storage().Remove(Value))
		{
			pProducts->Add(Value);
			GS()->Chat(ClientID, "You unloaded {} products.", Value);
			pPlayer->m_VotesData.UpdateCurrentVotes();
		}
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
		VoteWrapper(ClientID).Add("Warehouse don't work");
		return;
	}

	CItemDescription* pCurrency = pWarehouse->GetCurrency();

	// show base shop functions
	VoteWrapper VStorage(ClientID, VWF_SEPARATE | VWF_STYLE_STRICT_BOLD, "Warehouse :: {}", pWarehouse->GetName());
	if(pWarehouse->IsHasFlag(WF_STORAGE))
	{
		// storage functional
		VStorage.AddLine();
		VStorage.Add("\u2727 Your: {} | Storage: {} products", pPlayer->GetItem(itProduct)->GetValue(), pWarehouse->Storage().GetValue());
		{
			VStorage.BeginDepth();
			VStorage.AddIfOption(pWarehouse->IsHasFlag(WF_BUY), "WAREHOUSE_LOAD_PRODUCTS", pWarehouse->GetID(), "Load products");
			VStorage.AddIfOption(pWarehouse->IsHasFlag(WF_SELL), "WAREHOUSE_UNLOAD_PRODUCTS", pWarehouse->GetID(), "Unload products");
			VStorage.EndDepth();
		}
		VStorage.AddLine();
	}
	VStorage.AddItemValue(pCurrency->GetID());
	VStorage.AddOption("REPAIR_ITEMS", "Repair all items - FREE");

	/*
	 * Show trading list
	 */
	ShowTradeList(pWarehouse, pPlayer, "Can be used's", ItemType::TYPE_USED);
	ShowTradeList(pWarehouse, pPlayer, "Potion's", ItemType::TYPE_POTION);
	ShowTradeList(pWarehouse, pPlayer, "Equipment's", ItemType::TYPE_EQUIP);
	ShowTradeList(pWarehouse, pPlayer, "Module's", ItemType::TYPE_MODULE);
	ShowTradeList(pWarehouse, pPlayer, "Decoration's", ItemType::TYPE_DECORATION);
	ShowTradeList(pWarehouse, pPlayer, "Craft's", ItemType::TYPE_CRAFT);
	ShowTradeList(pWarehouse, pPlayer, "Other's", ItemType::TYPE_OTHER);
	ShowTradeList(pWarehouse, pPlayer, "Quest and all the rest's", ItemType::TYPE_INVISIBLE);

	/*
	 * Show selling list
	 */
	if(pWarehouse->IsHasFlag(WF_SELL))
	{
		VoteWrapper VItems(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_SIMPLE, "\u2725 Choose the item you want to sell");
		for(const auto& Trade : pWarehouse->GetTradingList())
		{
			const CItem* pItem = Trade.GetItem();
			const int Price = Trade.GetPrice();

			VItems.AddOption("WAREHOUSE_SELL_ITEM", pWarehouse->GetID(), Trade.GetID(), "[{}] Sell {} - {} {} per unit",
				pPlayer->GetItem(*pItem)->GetValue(), pItem->Info()->GetName(), Price, pCurrency->GetName());
		}
	}
}

void CWarehouseManager::ShowTradeList(CWarehouse* pWarehouse, CPlayer* pPlayer, const char* TypeName, ItemType Type) const
{
	const int ClientID = pPlayer->GetCID();

	// Check if the pWarehouse object is null
	if(!pWarehouse)
	{
		VoteWrapper(ClientID).Add("Warehouse don't work");
		return;
	}

	// order only by type
	if(std::all_of(pWarehouse->GetTradingList().begin(), pWarehouse->GetTradingList().end(), [Type](const CTrade& p)
	{ return p.GetItem()->Info()->GetType() != Type; }))
		return;

	// add empty line
	VoteWrapper::AddEmptyline(ClientID);

	// show trading list
	CItemDescription* pCurrency = pWarehouse->GetCurrency();
	VoteWrapper VItems(ClientID, VWF_SEPARATE_OPEN, TypeName);
	for(const auto& Trade : pWarehouse->GetTradingList())
	{
		const CItem* pItem = Trade.GetItem();
		CItemDescription* pItemInfo = pItem->Info();
		const int Price = Trade.GetPrice();

		// check by type
		if(pItemInfo->GetType() != Type)
			continue;

		// set title name by enchant type (or stack item, or only once)
		if(pItem->Info()->IsEnchantable())
		{
			const bool HasItem = pPlayer->GetItem(*pItem)->HasItem();
			VItems.AddMenu(MENU_WAREHOUSE_BUY_ITEM_SELECTED, Trade.GetID(), "[{}] {} {} - {} {}", (HasItem ? "✔" : "×"),
				pItem->Info()->GetName(), pItem->StringEnchantLevel().c_str(), Price, pCurrency->GetName());
		}
		else
		{
			VItems.AddMenu(MENU_WAREHOUSE_BUY_ITEM_SELECTED, Trade.GetID(), "[{}] {}x{} - {} {}",
				pPlayer->GetItem(*pItem)->GetValue(), pItem->Info()->GetName(), pItem->GetValue(), Price, pCurrency->GetName());
		}
	}

	// add line
	VoteWrapper::AddLine(ClientID);
}

void CWarehouseManager::ShowTrade(CPlayer* pPlayer, CWarehouse* pWarehouse, const TradeIdentifier& TradeID) const
{
	int ClientID = pPlayer->GetCID();

	// Check if the pWarehouse object is null
	if(!pWarehouse || !pWarehouse->GetTrade(TradeID))
	{
		VoteWrapper(ClientID).Add("Warehouse don't work");
		return;
	}

	// Variables
	CTrade* pTrade = pWarehouse->GetTrade(TradeID);
	const CItem* pItem = pTrade->GetItem();
	const bool HasItem = pPlayer->GetItem(pItem->GetID())->HasItem();

	// Show item information
	VoteWrapper VItem(ClientID, VWF_SEPARATE|VWF_STYLE_STRICT_BOLD, "Do you want to buy?");
	if(pItem->Info()->IsEnchantable())
	{
		VItem.Add("{} {}", (HasItem ? "✔" : "×"), pItem->Info()->GetName());
	}
	else
	{
		int Value = pPlayer->GetItem(pItem->GetID())->GetValue();
		VItem.Add("{} (has {})", pItem->Info()->GetName(), Value);
	}
	VItem.Add(pItem->Info()->GetDescription());
	if(pItem->Info()->HasAttributes())
	{
		char aAttributes[128];
		pItem->Info()->StrFormatAttributes(pPlayer, aAttributes, sizeof(aAttributes), pItem->GetEnchant());
		VItem.Add("* {}", aAttributes);
	}
	VoteWrapper::AddEmptyline(ClientID);

	// show information about the cost of the item
	VoteWrapper VRequired(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_STRICT, "Required");
	VRequired.ReinitNumeralDepthStyles(
		{
			{ DEPTH_LVL1, DEPTH_LIST_STYLE_BOLD }
		}
	);
	if(pWarehouse->IsHasFlag(WF_STORAGE))
	{
		int ProductCost = pTrade->GetProductsCost();
		CItemDescription* pProductInfo = GS()->GetItemInfo(itProduct);
		bool MarkHas = pWarehouse->Storage().GetValue() >= ProductCost;
		VRequired.MarkList().Add("{} {}x{} ({})", MarkHas ? "\u2714" : "\u2718", pProductInfo->GetName(), ProductCost, pWarehouse->Storage().GetValue());
	}
	CItemDescription* pCurrency = pWarehouse->GetCurrency();
	CPlayerItem* pPlayerItem = pPlayer->GetItem(pCurrency->GetID());
	bool MarkHas = pPlayerItem->GetValue() >= pTrade->GetPrice();
	VRequired.MarkList().Add("{} {}x{} ({})", MarkHas ? "\u2714" : "\u2718", pCurrency->GetName(), pTrade->GetPrice(), pPlayerItem->GetValue());
	VoteWrapper::AddEmptyline(ClientID);

	// show status and button buy
	if(pItem->Info()->IsEnchantable() && HasItem)
		VoteWrapper(ClientID).Add("- You can't buy more than one item");
	else if(pWarehouse->IsHasFlag(WF_STORAGE) && pWarehouse->Storage().GetValue() < pTrade->GetProductsCost())
		VoteWrapper(ClientID).Add("- Not enough products to buy");
	else if(pPlayer->GetItem(itGold)->GetValue() < pTrade->GetPrice())
		VoteWrapper(ClientID).Add("- Not enough gold to buy");
	else
		VoteWrapper(ClientID).AddOption("WAREHOUSE_BUY_ITEM", pWarehouse->GetID(), TradeID, "Buy");

	// add backpage
	VoteWrapper::AddEmptyline(ClientID);
}

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
			GS()->Chat(ClientID, "Not enought materials in storage! Required {} materials.", MaterialCost);
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
		GS()->Chat(ClientID, "You exchanged {}x{} for {}x{}.",
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
		GS()->Chat(ClientID, "You sold {}x{} for {} {}.", pItem->Info()->GetName(), Value, FinalPrice, pWarehouse->GetCurrency()->GetName());
		return true;
	}

	return false;
}

CWarehouse* CWarehouseManager::GetWarehouse(vec2 Pos) const
{
	auto iter = std::find_if(CWarehouse::Data().begin(), CWarehouse::Data().end(), [Pos](const CWarehouse* pItem)
	{ return distance(pItem->GetPos(), Pos) < 320; });
	return iter != CWarehouse::Data().end() ? *iter : nullptr;
}

CWarehouse* CWarehouseManager::GetWarehouse(WarehouseIdentifier ID) const
{
	auto iter = std::find_if(CWarehouse::Data().begin(), CWarehouse::Data().end(), [ID](const CWarehouse* pItem){ return pItem->GetID() == ID; });
	return iter != CWarehouse::Data().end() ? *iter : nullptr;
}
