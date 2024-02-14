/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "WarehouseManager.h"

#include <game/server/gamecontext.h>

#include <game/server/core/components/Inventory/InventoryManager.h>

constexpr auto TW_WAREHOUSE_TABLE = "tw_warehouses";
constexpr auto TW_WAREHOUSE_ITEMS_TABLE = "tw_warehouse_items";

// Optimized
void CWarehouseManager::OnInit()
{
	// init warehouses
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_WAREHOUSE_TABLE);
	std::unordered_map< int, CWarehouse::ContainerTradingSlots > TradesSlots;
	while(pRes->next())
	{
		WarehouseIdentifier ID = pRes->getInt("ID");
		std::string Name = pRes->getString("Name").c_str();
		vec2 Pos = vec2((float)pRes->getInt("PosX"), (float)pRes->getInt("PosY"));
		int Currency = pRes->getInt("Currency");
		int WorldID = pRes->getInt("WorldID");

		// init by server
		CWarehouse(ID).Init(Name, Pos, Currency, WorldID);
	}

	// init trades slots
	ResultPtr pResStore = Database->Execute<DB::SELECT>("*", TW_WAREHOUSE_ITEMS_TABLE);
	while(pResStore->next())
	{
		TradeIdentifier ID = pResStore->getInt("ID");
		ItemIdentifier ItemID = pResStore->getInt("ItemID");
		int ItemValue = pResStore->getInt("ItemValue");
		ItemIdentifier RequiredItemID = pResStore->getInt("RequiredItemID");
		int Price = pResStore->getInt("Price");
		int Enchant = pResStore->getInt("Enchant");
		int WarehouseID = pResStore->getInt("WarehouseID");

		// init by server
		CTradingSlot TradeSlot(ID);
		std::shared_ptr<CItem> pItem = std::make_shared<CItem>(CItem(ItemID, ItemValue, Enchant));
		TradeSlot.Init(pItem, &CItemDescription::Data()[RequiredItemID], Price);
		TradesSlots[WarehouseID].push_back(TradeSlot);
	}

	// init trades slots for warehouses
	for(auto& [WarehouseID, DataContainer] : TradesSlots)
		CWarehouse::Data()[WarehouseID].m_aTradingSlots = DataContainer;
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
	const int ClientID = pPlayer->GetCID();
	CCharacter* pChr = pPlayer->GetCharacter();

	if(Menulist == MENU_WAREHOUSE_SHOPPING_LIST)
	{
		CWarehouse* pWarehouse = GetWarehouse(pChr->m_Core.m_Pos);
		ShowWarehouseMenu(pChr->GetPlayer(), pWarehouse);

		return true;
	}

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

	if(PPSTR(CMD, "SHOP_BUY") == 0)
	{
		if(BuyItem(pPlayer, VoteID, VoteID2))
			pPlayer->m_VotesData.UpdateVotes(MENU_MAIN);
		return true;
	}

	if(PPSTR(CMD, "SELL_ITEM") == 0)
	{
		// Set the item identifier to VoteID
		ItemIdentifier ID = VoteID;

		// If the available value is less than or equal to 0, return true
		int AvailableValue = Core()->InventoryManager()->GetUnfrozenItemValue(pPlayer, ID);
		if(AvailableValue <= 0)
			return true;

		// Set Get to the minimum value between Get and AvailableValue
		Get = minimum(Get, AvailableValue);

		int Price = VoteID2;
		int CountPrice = Get * Price;

		// If the player's account has enough currency to spend, proceed with the transaction
		if(pPlayer->Account()->SpendCurrency(Get, ID))
		{
			// Add the total price to the player's gold
			pPlayer->Account()->AddGold(CountPrice);
			GS()->Chat(ClientID, "You sold {STR}x{VAL} for {VAL} gold.", GS()->GetItemInfo(ID)->GetName(), Get, CountPrice);
			pPlayer->m_VotesData.UpdateVotes(MENU_MAIN);
		}

		return true;
	}

	return false;
}

CWarehouse* CWarehouseManager::GetWarehouse(vec2 Pos) const
{
	for(auto& pItem : CWarehouse::Data())
	{
		if(distance(pItem.second.GetPos(), Pos) < 320)
			return &pItem.second;
	}
	return nullptr;
}

void CWarehouseManager::ShowWarehouseMenu(CPlayer* pPlayer, const CWarehouse* pWarehouse) const
{
	const int ClientID = pPlayer->GetCID();

	// Check if the pWarehouse object is null
	if(!pWarehouse)
	{
		CVoteWrapper(ClientID).Add("Warehouse don't work");
		return;
	}

	// show base shop functions
	CVoteWrapper VShop(ClientID, VWFLAG_SEPARATE_OPEN|VWFLAG_STYLE_SIMPLE, "Warehouse :: {STR}", pWarehouse->GetName());
	VShop.Add("Repair all items - FREE", "REPAIR_ITEMS");
	VShop.AddLine();
	VShop.AddItemValue(pWarehouse->GetCurrency()->GetID());
	VShop.AddLine();
	CVoteWrapper::AddEmptyline(ClientID);

	// show trade list
	for(auto& Trade : pWarehouse->m_aTradingSlots)
	{
		int Price = Trade.GetPrice();
		const CItemDescription* pCurrencyItem = Trade.GetCurrency();
		const CItem* pItem = Trade.GetItem();

		CVoteWrapper VItem(ClientID, VWFLAG_UNIQUE|VWFLAG_STYLE_SIMPLE);
		if(pItem->Info()->IsEnchantable())
		{
			const bool PlayerHasItem = pPlayer->GetItem(*pItem)->HasItem();
			VItem.SetTitle("({STR}){STR} {STR} - {VAL} {STR}", (PlayerHasItem ? "✔" : "×"), 
				pItem->Info()->GetName(), pItem->StringEnchantLevel().c_str(), Price, pCurrencyItem->GetName());

			char aAttributes[128];
			pItem->Info()->StrFormatAttributes(pPlayer, aAttributes, sizeof(aAttributes), pItem->GetEnchant());
			VItem.Add("* {STR}", aAttributes);
		}
		else
		{
			VItem.SetTitle("({VAL}){STR}x{VAL} - {VAL} {STR}",
				pPlayer->GetItem(*pItem)->GetValue(), pItem->Info()->GetName(), pItem->GetValue(), Price, pCurrencyItem->GetName());
		}
		VItem.Add(Instance::Localize(ClientID, pItem->Info()->GetDescription()));
		VItem.Add("Buy {STR}x{VAL}", pItem->Info()->GetName(), pItem->GetValue());
	}
}

bool CWarehouseManager::BuyItem(CPlayer* pPlayer, int WarehouseID, TradeIdentifier ID) const
{
	// finding a trade slot
	CWarehouse::ContainerTradingSlots& pContainer = GS()->GetWarehouse(WarehouseID)->m_aTradingSlots;
	auto Iter = std::find_if(pContainer.begin(), pContainer.end(), [ID](const CTradingSlot& p){ return p.GetID() == ID; });
	CTradingSlot* pTradeSlot = Iter != pContainer.end() ? &(*Iter) : nullptr;
	if(!pTradeSlot || !pTradeSlot->GetItem()->IsValid())
		return false;

	// check for enchantment
	const int ClientID = pPlayer->GetCID();
	CPlayerItem* pPlayerItem = pPlayer->GetItem(pTradeSlot->GetItem()->GetID());
	if(pPlayerItem->HasItem() && pPlayerItem->Info()->IsEnchantable())
	{
		GS()->Chat(ClientID, "Enchant item maximal count x1 in a backpack!");
		return false;
	}

	// purchase
	if(!pPlayer->Account()->SpendCurrency(pTradeSlot->GetPrice(), pTradeSlot->GetCurrency()->GetID()))
		return false;

	// give trade slot for player
	CItem* pTradeItem = pTradeSlot->GetItem();
	pPlayerItem->Add(pTradeItem->GetValue(), 0, pTradeItem->GetEnchant());
	GS()->Chat(ClientID, "You exchanged {STR}x{VAL} for {STR}x{VAL}.", pTradeSlot->GetCurrency()->GetName(), pTradeSlot->GetPrice(), pTradeItem->Info()->GetName(), pTradeItem->GetValue());
	return true;
}