/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "warehouse_manager.h"

#include <game/server/gamecontext.h>
#include <game/server/core/components/Inventory/InventoryManager.h>

constexpr int g_UpdateTextLifeTime = SERVER_TICK_SPEED * 3;

void CWarehouseManager::OnPreInit()
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_WAREHOUSE_TABLE);
	while(pRes->next())
	{
		const auto ID = pRes->getInt("ID");
		const auto Name = pRes->getString("Name");
		const auto Properties = pRes->getString("Properties");
		const auto Pos = vec2((float)pRes->getInt("PosX"), (float)pRes->getInt("PosY"));
		const auto Currency = pRes->getInt("Currency");
		const auto WorldID = pRes->getInt("WorldID");

		CWarehouse::CreateElement(ID)->Init(Name, Properties, Pos, Currency, WorldID);
	}
}

void CWarehouseManager::OnTick()
{
	if(Server()->Tick() % g_UpdateTextLifeTime != 0)
		return;

	for(const auto pWarehouse : CWarehouse::Data())
	{
		if(!pWarehouse->IsHasFlag(WF_STORAGE))
			continue;

		pWarehouse->Storage().UpdateText(g_UpdateTextLifeTime);
	}
}

void CWarehouseManager::OnCharacterTile(CCharacter* pChr)
{
	CPlayer* pPlayer = pChr->GetPlayer();

	HANDLE_TILE_VOTE_MENU(pPlayer, pChr, TILE_SHOP_ZONE, MENU_WAREHOUSE, {}, {});
}

bool CWarehouseManager::OnSendMenuVotes(CPlayer* pPlayer, int Menulist)
{
	const auto* pChr = pPlayer->GetCharacter();

	// menu warehouse
	if(Menulist == MENU_WAREHOUSE)
	{
		CWarehouse* pWarehouse = GetWarehouse(pChr->m_Core.m_Pos);
		ShowWarehouseList(pPlayer, pWarehouse);
		return true;
	}

	// menu warehouse item select
	if(Menulist == MENU_WAREHOUSE_ITEM_SELECT)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_WAREHOUSE);

		// try show trade item
		if(const auto TradeID = pPlayer->m_VotesData.GetExtraID())
		{
			CWarehouse* pWarehouse = GetWarehouse(pChr->m_Core.m_Pos);
			ShowTrade(pPlayer, pWarehouse, TradeID.value());
		}

		// add backpage
		VoteWrapper::AddBackpage(pPlayer->GetCID());
		return true;
	}

	return false;
}

bool CWarehouseManager::OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, const int Extra1, const int Extra2, int ReasonNumber, const char* pReason)
{
	const int ClientID = pPlayer->GetCID();

	// repair all items
	if(PPSTR(pCmd, "REPAIR_ITEMS") == 0)
	{
		Core()->InventoryManager()->RepairDurabilityItems(pPlayer);
		GS()->Chat(ClientID, "All items have been repaired.");
		return true;
	}

	// buying items from the warehouse
	if(PPSTR(pCmd, "WAREHOUSE_BUY_ITEM") == 0)
	{
		const auto WarehouseID = Extra1;
		const auto TradeID = Extra2;

		// check if the warehouse exists
		CWarehouse* pWarehouse = GetWarehouse(WarehouseID);
		if(!pWarehouse)
			return true;

		// buy item
		if(BuyItem(pPlayer, pWarehouse, TradeID))
		{
			pPlayer->m_VotesData.UpdateCurrentVotes();
		}

		return true;
	}

	// selling items to the warehouse
	if(PPSTR(pCmd, "WAREHOUSE_SELL_ITEM") == 0)
	{
		const auto WarehouseID = Extra1;
		const auto TradeID = Extra2;

		// check if the warehouse exists
		CWarehouse* pWarehouse = GetWarehouse(WarehouseID);
		if(!pWarehouse)
			return true;

		// sell item
		if(SellItem(pPlayer, pWarehouse, TradeID, ReasonNumber))
		{
			pPlayer->m_VotesData.UpdateCurrentVotes();
		}

		return true;
	}

	// loading products into the warehouse
	if(PPSTR(pCmd, "WAREHOUSE_LOAD_PRODUCTS") == 0)
	{
		const auto WarehouseID = Extra1;

		// check if the warehouse exists
		CWarehouse* pWarehouse = GetWarehouse(WarehouseID);
		if(!pWarehouse)
			return true;

		// check if the warehouse has a storage
		if(!pWarehouse->IsHasFlag(WF_STORAGE))
			return true;

		// check if the player has a product in the backpack
		CPlayerItem* pProducts = pPlayer->GetItem(itProduct);
		if(!pProducts->HasItem())
		{
			GS()->Chat(ClientID, "You don't have a product in your backpack.");
			return true;
		}

		// loading products
		const auto Value = pProducts->GetValue();
		if(pPlayer->Account()->SpendCurrency(Value, itProduct))
		{
			pWarehouse->Storage().Add(Value);
			pPlayer->Account()->AddGold(Value);
			GS()->Chat(ClientID, "You loaded {} products. Got {$} gold.", Value, Value);
			pPlayer->m_VotesData.UpdateCurrentVotes();
		}

		return true;
	}

	// unload products from the warehouse
	if(PPSTR(pCmd, "WAREHOUSE_UNLOAD_PRODUCTS") == 0)
	{
		const auto WarehouseID = Extra1;

		// check if the warehouse exists
		CWarehouse* pWarehouse = GetWarehouse(WarehouseID);
		if(!pWarehouse)
			return true;

		// check if the warehouse has a storage
		if(!pWarehouse->IsHasFlag(WF_STORAGE))
			return true;

		// check if the warehouse storage is empty
		const auto Value = pWarehouse->Storage().GetValue();
		if(Value <= 0)
		{
			GS()->Chat(ClientID, "Warehouse storage is empty.");
			return true;
		}

		// check is player has maximum products
		CPlayerItem* pProducts = pPlayer->GetItem(itProduct);
		if(pProducts->GetValue() >= g_Config.m_SvWarehouseProductsCanTake)
		{
			GS()->Chat(ClientID, "You can't take more than {} products.", g_Config.m_SvWarehouseProductsCanTake);
			return true;
		}

		// unload products
		const auto FinalValue = minimum(pWarehouse->Storage().GetValue().to_clamp<int>(), g_Config.m_SvWarehouseProductsCanTake - pProducts->GetValue());
		if(pWarehouse->Storage().Remove(FinalValue))
		{
			pProducts->Add(FinalValue);
			GS()->Chat(ClientID, "You unloaded {} products.", FinalValue);
			pPlayer->m_VotesData.UpdateCurrentVotes();
		}

		return true;
	}

	return false;
}

void CWarehouseManager::ShowWarehouseList(CPlayer* pPlayer, CWarehouse* pWarehouse) const
{
	if(!pWarehouse)
		return;

	const int ClientID = pPlayer->GetCID();
	const auto* pCurrency = pWarehouse->GetCurrency();

	// show base shop functions
	VoteWrapper VStorage(ClientID, VWF_SEPARATE | VWF_STYLE_STRICT_BOLD, "Warehouse :: {}", pWarehouse->GetName());
	if(pWarehouse->IsHasFlag(WF_STORAGE))
	{
		VStorage.AddLine();
		VStorage.Add("\u2727 Your: {} | Storage: {} products", pPlayer->GetItem(itProduct)->GetValue(), pWarehouse->Storage().GetValue());
		{
			VStorage.BeginDepth();

			if(pWarehouse->IsHasFlag(WF_BUY))
			{
				VStorage.AddOption("WAREHOUSE_LOAD_PRODUCTS", pWarehouse->GetID(), "Load products");
			}

			if(pWarehouse->IsHasFlag(WF_SELL))
			{
				VStorage.AddOption("WAREHOUSE_UNLOAD_PRODUCTS", pWarehouse->GetID(), "Unload products");
			}

			VStorage.EndDepth();
		}
		VStorage.AddLine();
	}
	VStorage.AddItemValue(pCurrency->GetID());
	VStorage.AddOption("REPAIR_ITEMS", "Repair all items - FREE");

	// show trade list by groups
	ShowTradeList(pWarehouse, pPlayer, "Can be used's", ItemType::TYPE_USED);
	ShowTradeList(pWarehouse, pPlayer, "Potion's", ItemType::TYPE_POTION);
	ShowTradeList(pWarehouse, pPlayer, "Equipment's", ItemType::TYPE_EQUIP);
	ShowTradeList(pWarehouse, pPlayer, "Module's", ItemType::TYPE_MODULE);
	ShowTradeList(pWarehouse, pPlayer, "Decoration's", ItemType::TYPE_DECORATION);
	ShowTradeList(pWarehouse, pPlayer, "Craft's", ItemType::TYPE_CRAFT);
	ShowTradeList(pWarehouse, pPlayer, "Other's", ItemType::TYPE_OTHER);
	ShowTradeList(pWarehouse, pPlayer, "Quest and all the rest's", ItemType::TYPE_INVISIBLE);

	// selling list
	if(pWarehouse->IsHasFlag(WF_SELL))
	{
		VoteWrapper VItems(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_SIMPLE, "\u2725 Choose the item you want to sell");
		for(const auto& Trade : pWarehouse->GetTradingList())
		{
			const auto* pItem = Trade.GetItem();
			const auto* pPlayerItem = pPlayer->GetItem(*pItem);
			const auto Price = Trade.GetPrice();

			VItems.AddOption("WAREHOUSE_SELL_ITEM", pWarehouse->GetID(), Trade.GetID(), "[{}] Sell {} - {$} {} per unit",
				pPlayerItem->GetValue(), pItem->Info()->GetName(), Price, pCurrency->GetName());
		}
	}
}

void CWarehouseManager::ShowTradeList(CWarehouse* pWarehouse, CPlayer* pPlayer, const char* TypeName, ItemType Type) const
{
	if(!pWarehouse)
		return;

	const int ClientID = pPlayer->GetCID();
	const auto* pCurrency = pWarehouse->GetCurrency();
	auto vTradeListByType = pWarehouse->GetTradingList() | std::views::filter([Type](const CTrade& Trade)
	{
		return Trade.GetItem()->Info()->GetType() == Type;
	});

	// check if the warehouse has a trading list by type
	if(std::ranges::empty(vTradeListByType))
		return;

	// show trading list
	VoteWrapper::AddEmptyline(ClientID);
	VoteWrapper VItems(ClientID, VWF_SEPARATE_OPEN, TypeName);
	for(const auto& Trade : vTradeListByType)
	{
		const auto* pItem = Trade.GetItem();
		const auto* pItemInfo = pItem->Info();
		const int Price = Trade.GetPrice();

		// set title name by enchant type (or stack item, or only once)
		if(pItemInfo->IsEnchantable())
		{
			const bool HasItem = pPlayer->GetItem(*pItem)->HasItem();
			VItems.AddMenu(MENU_WAREHOUSE_ITEM_SELECT, Trade.GetID(), "[{}] {} {} - {$} {}", (HasItem ? "✔" : "×"),
				pItemInfo->GetName(), pItem->GetStringEnchantLevel().c_str(), Price, pCurrency->GetName());
		}
		else
		{
			VItems.AddMenu(MENU_WAREHOUSE_ITEM_SELECT, Trade.GetID(), "[{}] {}x{} - {$} {}",
				pPlayer->GetItem(*pItem)->GetValue(), pItemInfo->GetName(), pItem->GetValue(), Price, pCurrency->GetName());
		}
	}
	VoteWrapper::AddLine(ClientID);
}

void CWarehouseManager::ShowTrade(CPlayer* pPlayer, CWarehouse* pWarehouse, int TradeID) const
{
	if(!pWarehouse || !pWarehouse->GetTrade(TradeID))
		return;

	const int ClientID = pPlayer->GetCID();
	const auto* pTrade = pWarehouse->GetTrade(TradeID);
	const auto* pItem = pTrade->GetItem();
	const auto* pPlayerItem = pPlayer->GetItem(*pItem);
	const auto HasItem = pPlayerItem->HasItem();
	const auto* pCurrency = pWarehouse->GetCurrency();
	const auto* pPlayerItemCurrency = pPlayer->GetItem(pCurrency->GetID());

	// trade item informaton
	VoteWrapper VItem(ClientID, VWF_SEPARATE | VWF_STYLE_STRICT_BOLD, "Do you want to buy?");
	if(pItem->Info()->IsEnchantable())
	{
		VItem.Add("{} {}", (HasItem ? "✔" : "×"), pItem->Info()->GetName());
	}
	else
	{
		const int Value = pPlayerItem->GetValue();
		VItem.Add("{} (has {})", pItem->Info()->GetName(), Value);
	}

	VItem.Add(pItem->Info()->GetDescription());

	if(pItem->Info()->HasAttributes())
	{
		VItem.Add("* {}", pItem->Info()->GetStringAttributesInfo(pPlayer, pItem->GetEnchant()));
	}
	VoteWrapper::AddEmptyline(ClientID);

	// show required information
	VoteWrapper VRequired(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_STRICT, "Required");
	VRequired.ReinitNumeralDepthStyles({{ DEPTH_LVL1, DEPTH_LIST_STYLE_BOLD }});

	// show required products
	if(pWarehouse->IsHasFlag(WF_STORAGE))
	{
		const auto* pProductInfo = GS()->GetItemInfo(itProduct);
		const auto ProductCost = pTrade->GetProductsCost();
		const auto MarkHas = pWarehouse->Storage().GetValue() >= ProductCost;
		const char* pLabelMark = MarkHas ? "\u2714" : "\u2718";

		VRequired.MarkList().Add("{} {}x{$} ({})", pLabelMark, pProductInfo->GetName(), ProductCost, pWarehouse->Storage().GetValue());
	}

	// show required currency items
	const auto MarkHas = pPlayerItemCurrency->GetValue() >= pTrade->GetPrice();
	const char* pLabelMark = MarkHas ? "\u2714" : "\u2718";

	VRequired.MarkList().Add("{} {}x{$} ({})", pLabelMark, pCurrency->GetName(), pTrade->GetPrice(), pPlayerItemCurrency->GetValue());
	VoteWrapper::AddEmptyline(ClientID);

	// show status and button buy
	if(pItem->Info()->IsEnchantable() && HasItem)
	{
		VoteWrapper(ClientID).Add("- You can't buy more than one item");
	}
	else if(pWarehouse->IsHasFlag(WF_STORAGE) && pWarehouse->Storage().GetValue() < pTrade->GetProductsCost())
	{
		VoteWrapper(ClientID).Add("- Not enough products to buy");
	}
	else if(pPlayer->Account()->GetTotalGold() < pTrade->GetPrice())
	{
		VoteWrapper(ClientID).Add("- Not enough gold to buy");
	}
	else
	{
		VoteWrapper(ClientID).AddOption("WAREHOUSE_BUY_ITEM", pWarehouse->GetID(), TradeID, "Buy");
	}

	// add backpage
	VoteWrapper::AddEmptyline(ClientID);
}

bool CWarehouseManager::BuyItem(CPlayer* pPlayer, CWarehouse* pWarehouse, int TradeID) const
{
	if(!pWarehouse || !pWarehouse->GetTrade(TradeID))
		return false;

	const int ClientID = pPlayer->GetCID();
	const auto* pTrade = pWarehouse->GetTrade(TradeID);
	const auto* pItem = pTrade->GetItem();
	const auto* pCurrency = pWarehouse->GetCurrency();
	auto* pPlayerItem = pPlayer->GetItem(*pItem);

	// check if the player has the enchanted item in the backpack
	if(pPlayerItem->HasItem() && pPlayerItem->Info()->IsEnchantable())
	{
		GS()->Chat(ClientID, "Enchant item maximal count x1 in a backpack!");
		return false;
	}

	// check products
	if(pWarehouse->IsHasFlag(WF_STORAGE))
	{
		const auto StorageValue = pWarehouse->Storage().GetValue();
		const auto ProductsCost = pTrade->GetProductsCost();

		if(StorageValue < ProductsCost)
		{
			GS()->Chat(ClientID, "Not enought products in storage! Required {} products.", ProductsCost);
			return false;
		}
	}

	// purchase an item
	if(pPlayer->Account()->SpendCurrency(pTrade->GetPrice(), pCurrency->GetID()))
	{
		// storage remove products
		if(pWarehouse->IsHasFlag(WF_STORAGE))
		{
			pWarehouse->Storage().Remove(pTrade->GetProductsCost());
		}

		// add items
		pPlayerItem->Add(pItem->GetValue(), 0, pItem->GetEnchant());
		GS()->Chat(ClientID, "You exchanged {}x{$} for {}x{}.", pCurrency->GetName(), pTrade->GetPrice(), pItem->Info()->GetName(), pItem->GetValue());
		return true;
	}

	return false;
}

bool CWarehouseManager::SellItem(CPlayer* pPlayer, CWarehouse* pWarehouse, int TradeID, int Value) const
{
	if(!pWarehouse || !pWarehouse->GetTrade(TradeID))
		return false;

	const int ClientID = pPlayer->GetCID();
	const auto* pTrade = pWarehouse->GetTrade(TradeID);
	const int& Price = pTrade->GetPrice();
	const auto* pItem = pTrade->GetItem();
	const auto* pCurrency = pWarehouse->GetCurrency();
	auto* pPlayerCurrencyItem = pPlayer->GetItem(pCurrency->GetID());

	Value = minimum(Value, Core()->InventoryManager()->GetUnfrozenItemValue(pPlayer, pItem->GetID()));
	const auto FinalPrice = Value * Price;

	// try spend by item
	if(pPlayer->Account()->SpendCurrency(Value, pItem->GetID()))
	{
		// storage add products
		if(pWarehouse->IsHasFlag(WF_STORAGE))
		{
			const auto ProductsCost = pTrade->GetProductsCost() * Value;
			pWarehouse->Storage().Add(ProductsCost);
		}

		// add currency for player
		pPlayerCurrencyItem->Add(FinalPrice);
		GS()->Chat(ClientID, "You sold {}x{} for {$} {}.", pItem->Info()->GetName(), Value, FinalPrice, pCurrency->GetName());
		return true;
	}

	return false;
}

CWarehouse* CWarehouseManager::GetWarehouse(vec2 Pos) const
{
	const auto iter = std::ranges::find_if(CWarehouse::Data(), [Pos](const CWarehouse* pItem)
	{
		return distance(pItem->GetPos(), Pos) < 320;
	});

	return iter != CWarehouse::Data().end() ? *iter : nullptr;
}

CWarehouse* CWarehouseManager::GetWarehouse(int WarehouseID) const
{
	const auto iter = std::ranges::find_if(CWarehouse::Data(), [WarehouseID](const CWarehouse* pItem)
	{
		return pItem->GetID() == WarehouseID;
	});

	return iter != CWarehouse::Data().end() ? *iter : nullptr;
}
