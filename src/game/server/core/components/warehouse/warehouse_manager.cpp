#include "warehouse_manager.h"

#include <game/server/gamecontext.h>
#include <game/server/core/components/inventory/inventory_manager.h>

constexpr int g_UpdateTextLifeTime = SERVER_TICK_SPEED * 3;

void CWarehouseManager::OnPreInit()
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_WAREHOUSE_TABLE);
	while(pRes->next())
	{
		const auto ID = pRes->getInt("ID");
		const auto Name = pRes->getString("Name");
		const auto TypeSet = DBSet(pRes->getString("Type"));
		const auto TradesStr = pRes->getString("Trades");
		const auto StorageData = pRes->getJson("StorageData");
		const auto Pos = vec2((float)pRes->getInt("PosX"), (float)pRes->getInt("PosY"));
		const auto Currency = pRes->getInt("Currency");
		const auto WorldID = pRes->getInt("WorldID");

		CWarehouse::CreateElement(ID)->Init(Name, TypeSet, TradesStr, StorageData, Pos, Currency, WorldID);
	}
}

void CWarehouseManager::OnTick()
{
	if(Server()->Tick() % g_UpdateTextLifeTime == 0)
	{
		for(const auto pWarehouse : CWarehouse::Data())
		{
			if(pWarehouse->IsHasFlag(WF_STORAGE))
				pWarehouse->Storage().UpdateText(g_UpdateTextLifeTime);
		}
	}
}

void CWarehouseManager::OnCharacterTile(CCharacter* pChr)
{
	auto* pPlayer = pChr->GetPlayer();
	HANDLE_TILE_VOTE_MENU(pPlayer, pChr, TILE_SHOP_ZONE, MENU_WAREHOUSE, {}, {});
}


bool CWarehouseManager::OnSendMenuVotes(CPlayer* pPlayer, int Menulist)
{
	const auto* pChr = pPlayer->GetCharacter();

	// menu warehouse
	if(Menulist == MENU_WAREHOUSE)
	{
		auto* pWarehouse = GetWarehouse(pChr->m_Core.m_Pos);
		ShowWarehouseList(pPlayer, pWarehouse);
		return true;
	}

	// menu warehouse item select
	if(Menulist == MENU_WAREHOUSE_ITEM_SELECT)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_WAREHOUSE);

		if(const auto TradeID = pPlayer->m_VotesData.GetExtraID())
		{
			auto* pWarehouse = GetWarehouse(pChr->m_Core.m_Pos);
			ShowTrade(pPlayer, pWarehouse, TradeID.value());
		}

		VoteWrapper::AddBackpage(pPlayer->GetCID());
		return true;
	}

	return false;
}


bool CWarehouseManager::OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, const int Extra1, const int Extra2, int ReasonNumber, const char* pReason)
{
	if(!pPlayer) return false;
	const int ClientID = pPlayer->GetCID();

	// repair all items
	if(PPSTR(pCmd, "REPAIR_ITEMS") == 0)
	{
		Core()->InventoryManager()->RepairDurabilityItems(pPlayer);
		GS()->CreateSound(pPlayer->m_ViewPos, SOUND_VOTE_WAREHOUSE_REPAIR_ITEMS);
		GS()->Chat(ClientID, "All items have been repaired.");
		return true;
	}

	// buying items from the warehouse
	if(PPSTR(pCmd, "WAREHOUSE_BUY_ITEM") == 0)
	{
		const auto WarehouseID = Extra1;
		const auto TradeID = Extra2;
		auto* pWarehouse = GetWarehouse(WarehouseID);
		if(!pWarehouse)
			return true;

		if(BuyItem(pPlayer, pWarehouse, TradeID))
		{
			GS()->CreateSound(pPlayer->m_ViewPos, SOUND_VOTE_WAREHOUSE_BUY);
			pPlayer->m_VotesData.UpdateCurrentVotes();
		}

		return true;
	}

	// selling items to the warehouse
	if(PPSTR(pCmd, "WAREHOUSE_SELL_ITEM") == 0)
	{
		const auto WarehouseID = Extra1;
		const auto TradeID = Extra2;
		const int ValueToSell = ReasonNumber;
		auto* pWarehouse = GetWarehouse(WarehouseID);
		if(!pWarehouse)
			return true;

		if(SellItem(pPlayer, pWarehouse, TradeID, ValueToSell))
		{
			GS()->CreateSound(pPlayer->m_ViewPos, SOUND_VOTE_WAREHOUSE_SELL);
			pPlayer->m_VotesData.UpdateCurrentVotes();
		}

		return true;
	}

	// loading products into the warehouse
	if(PPSTR(pCmd, "WAREHOUSE_LOAD_PRODUCTS") == 0)
	{
		const auto WarehouseID = Extra1;
		auto* pWarehouse = GetWarehouse(WarehouseID);
		if(!pWarehouse || !pWarehouse->IsHasFlag(WF_STORAGE))
			return true;

		auto* pProductsItem = pPlayer->GetItem(itProduct);
		if(!pProductsItem->HasItem())
		{
			GS()->Chat(ClientID, "You don't have a product in your backpack.");
			return true;
		}

		const auto Value = pProductsItem->GetValue();
		if(pPlayer->Account()->SpendCurrency(Value, itProduct))
		{
			// add experience to loader
			auto Experience = translate_to_percent_rest(Value, 10);
			auto* pLoader = pPlayer->Account()->GetProfession(ProfessionIdentifier::Loader);
			pLoader->AddExperience(Experience);

			// load to warehouse
			pWarehouse->Storage().Add(Value);
			pPlayer->Account()->AddGold(Value);
			GS()->Chat(ClientID, "You loaded '{} products'. Got '{$} gold'.", Value, Value);
			GS()->CreateSound(pPlayer->m_ViewPos, SOUND_VOTE_WAREHOUSE_PRODUCT_LOAD);
			pPlayer->m_VotesData.UpdateCurrentVotes();
		}

		return true;
	}

	// unload products from the warehouse
	if(PPSTR(pCmd, "WAREHOUSE_UNLOAD_PRODUCTS") == 0)
	{
		const auto WarehouseID = Extra1;
		auto* pWarehouse = GetWarehouse(WarehouseID);
		if(!pWarehouse || !pWarehouse->IsHasFlag(WF_STORAGE))
			return true;

		const auto StorageValue = pWarehouse->Storage().GetValue();
		if(StorageValue <= 0)
		{
			GS()->Chat(ClientID, "Warehouse storage is empty.");
			return true;
		}

		auto EnduranceValue = pPlayer->GetTotalAttributeValue(AttributeIdentifier::ProductCapacity);
		auto MaxCanTake = g_Config.m_SvWarehouseProductsCanTake + EnduranceValue;

		// check is player has maximum products
		auto* pProductsItem = pPlayer->GetItem(itProduct);
		if(pProductsItem->GetValue() >= MaxCanTake)
		{
			GS()->Chat(ClientID, "You can't take more than '{} products'.", MaxCanTake);
			return true;
		}

		// unload products
		const auto FinalValue = minimum(pWarehouse->Storage().GetValue().to_clamp<int>(), (int)(MaxCanTake - pProductsItem->GetValue()));
		if(FinalValue > 0 && pWarehouse->Storage().Remove(FinalValue))
		{
			// add experience to loader
			auto Experience = translate_to_percent_rest(FinalValue, 10);
			auto* pLoader = pPlayer->Account()->GetProfession(ProfessionIdentifier::Loader);
			pLoader->AddExperience(Experience);

			pProductsItem->Add(FinalValue);
			GS()->Chat(ClientID, "You unloaded '{} products'.", FinalValue);
			GS()->CreateSound(pPlayer->m_ViewPos, SOUND_VOTE_WAREHOUSE_PRODUCT_UNLOAD);
			pPlayer->m_VotesData.UpdateCurrentVotes();
		}

		return true;
	}

	if(PPSTR(pCmd, "WAREHOUSE_SELECTOR_GROUP") == 0)
	{
		pPlayer->m_GroupFilter = Extra1;
		pPlayer->m_SubgroupFilter = std::nullopt;
		pPlayer->m_VotesData.UpdateCurrentVotes();
		return true;
	}

	if(PPSTR(pCmd, "WAREHOUSE_SELECTOR_SUBGROUP") == 0)
	{
		pPlayer->m_SubgroupFilter = Extra1;
		pPlayer->m_VotesData.UpdateCurrentVotes();
		return true;
	}

	return false;
}


void CWarehouseManager::ShowWarehouseList(CPlayer* pPlayer, CWarehouse* pWarehouse) const
{
	if(!pPlayer || !pWarehouse)
		return;

	// initialize variables
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
				VStorage.AddOption("WAREHOUSE_LOAD_PRODUCTS", pWarehouse->GetID(), "Load products");
			if(pWarehouse->IsHasFlag(WF_SELL))
				VStorage.AddOption("WAREHOUSE_UNLOAD_PRODUCTS", pWarehouse->GetID(), "Unload products");
			VStorage.EndDepth();
		}
		VStorage.AddLine();
	}
	VStorage.AddItemValue(pCurrency->GetID());
	VStorage.AddOption("REPAIR_ITEMS", "Repair all items - FREE");

	// trading list
	if(pWarehouse->IsHasFlag(WF_BUY))
	{
		VoteWrapper::AddEmptyline(ClientID);
		ShowGroupedSelector(pPlayer, pWarehouse, true);
	}

	// selling list
	if(pWarehouse->IsHasFlag(WF_SELL))
	{
		VoteWrapper::AddEmptyline(ClientID);
		ShowGroupedSelector(pPlayer, pWarehouse, false);
	}
}


void CWarehouseManager::ShowGroupedSelector(CPlayer* pPlayer, CWarehouse* pWarehouse, bool IsBuyingAction) const
{
	if(!pPlayer || !pWarehouse)
		return;

	// initialize variables
	const auto ClientID = pPlayer->GetCID();
	const auto& groupedTradesContainer = pWarehouse->GetGroupedTrades();
	const auto& allGroupData = groupedTradesContainer.get_all_data();
	const auto& groupIdOpt = pPlayer->m_GroupFilter;
	const auto& subgroupIdOpt = pPlayer->m_SubgroupFilter;

	// show selector by group
	VoteWrapper VSG(ClientID, VWF_ALIGN_TITLE | VWF_SEPARATE_OPEN | VWF_STYLE_SIMPLE, "\u2636 Select a group");
	for(const auto& [groupName, subGroupMap] : allGroupData)
	{
		const auto GroupID = pPlayer->m_VotesData.GetStringMapper().string_to_id(groupName);
		const char* pSelectStr = GetSelectorStringByCondition(groupIdOpt && (*groupIdOpt) == GroupID);
		const auto countItemByGroup = groupedTradesContainer.get_item_group_count(groupName);
		VSG.AddOption("WAREHOUSE_SELECTOR_GROUP", GroupID, "{}({}){SELECTOR}", Instance::Localize(ClientID, groupName.c_str()), countItemByGroup, pSelectStr);
	}

	// show selector by subgroup
	if(groupIdOpt)
	{
		// check valid id by group name and valid group container
		const auto groupNameOpt = pPlayer->m_VotesData.GetStringMapper().id_to_string(*groupIdOpt);
		if(!groupNameOpt || !groupedTradesContainer.has_group(*groupNameOpt))
			return;

		const auto* pSubGroupMap = groupedTradesContainer.get_subgroups(*groupNameOpt);
		if(!pSubGroupMap)
			return;

		bool HasOnlyDefaultGroup = (pSubGroupMap->size() <= 1 &&
			std::ranges::any_of(*pSubGroupMap, [&](const auto& pair) { return pair.first == groupedTradesContainer.get_default_subgroup_key(); }));

		if(!HasOnlyDefaultGroup)
		{
			VSG.AddLine();
			for(const auto& [subGroupName, itemList] : *pSubGroupMap)
			{
				const auto SubgroupID = pPlayer->m_VotesData.GetStringMapper().string_to_id(subGroupName);
				const char* pSelectStr = GetSelectorStringByCondition(subgroupIdOpt && (*subgroupIdOpt) == SubgroupID);
				VSG.AddOption("WAREHOUSE_SELECTOR_SUBGROUP", SubgroupID, "{}({}){SELECTOR}", subGroupName.c_str(), itemList.size(), pSelectStr);
			}
		}
		else
		{
			pPlayer->m_SubgroupFilter = pPlayer->m_VotesData.GetStringMapper().string_to_id(groupedTradesContainer.get_default_subgroup_key());
		}
	}

	// show all item's by filter
	if(groupIdOpt && subgroupIdOpt)
	{
		// check valid id by group name and valid group container
		const auto groupNameOptStr = pPlayer->m_VotesData.GetStringMapper().id_to_string(*groupIdOpt);
		if(!groupNameOptStr || !groupedTradesContainer.has_group(*groupNameOptStr))
			return;

		// check valid id by subgroup name and valid subgroup container
		const auto subGroupNameOptStr = pPlayer->m_VotesData.GetStringMapper().id_to_string(*subgroupIdOpt);
		if(!subGroupNameOptStr || !groupedTradesContainer.has_subgroup(*groupNameOptStr, *subGroupNameOptStr))
			return;

		const auto* pItemList = groupedTradesContainer.get_items(*groupNameOptStr, *subGroupNameOptStr);
		if(!pItemList)
			return;

		// initialize variables
		const auto* pCurrency = pWarehouse->GetCurrency();

		// show all item's by selector filter
		VoteWrapper::AddEmptyline(ClientID);
		const auto groupName = (*subGroupNameOptStr) != groupedTradesContainer.get_default_subgroup_key()
			? (*subGroupNameOptStr) : (*groupNameOptStr);
		VoteWrapper VGroup(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_STRICT_BOLD, "\u25BC {}", Instance::Localize(ClientID, groupName.c_str()));

		for(CTrade* pTrade : *pItemList)
		{
			const auto* pItem = pTrade->GetItem();
			const auto* pItemInfo = pItem->Info();
			const auto Price = pTrade->GetPrice();
			const auto* pPlayerItem = pPlayer->GetItem(*pItem);

			// sell
			if(!IsBuyingAction)
			{
				const auto sellValue = pItem->GetValue();
				const auto playerValue = pPlayerItem->GetValue();
				VGroup.AddOption("WAREHOUSE_SELL_ITEM", pWarehouse->GetID(), pTrade->GetID(), "[{}] Sell {} x{} - {$} {} per unit",
					playerValue, pItemInfo->GetName(), sellValue, Price, pCurrency->GetName());
				continue;
			}

			// buy
			if(!pItemInfo->IsStackable())
			{
				const bool HasItem = pPlayerItem->HasItem();
				VGroup.AddMenu(MENU_WAREHOUSE_ITEM_SELECT, pTrade->GetID(), "[{}] {} {} - {$} {}", (HasItem ? "✔" : "×"),
					pItemInfo->GetName(), pItem->GetStringEnchantLevel().c_str(), Price, pCurrency->GetName());
			}
			else
			{
				VGroup.AddMenu(MENU_WAREHOUSE_ITEM_SELECT, pTrade->GetID(), "[{}] {} x{} - {$} {}",
					pPlayerItem->GetValue(), pItemInfo->GetName(), pItem->GetValue(), Price, pCurrency->GetName());
			}
		}
	}
}


void CWarehouseManager::ShowTrade(CPlayer* pPlayer, CWarehouse* pWarehouse, int TradeID) const
{
	if(!pWarehouse || !pWarehouse->GetTrade(TradeID))
		return;

	// initialize variables
	const auto* pTrade = pWarehouse->GetTrade(TradeID);
	const auto* pItem = pTrade->GetItem();
	const auto* pPlayerItem = pPlayer->GetItem(*pItem);
	const auto* pCurrency = pWarehouse->GetCurrency();
	const auto* pPlayerItemCurrency = pPlayer->GetItem(pCurrency->GetID());
	const int ClientID = pPlayer->GetCID();
	const bool HasAlreadyItem = pPlayerItem->HasItem();
	const auto PlayerCurrencyValue = pCurrency->GetID() == itGold ? pPlayer->Account()->GetTotalGold() : pPlayerItemCurrency->GetValue();

	// do you want
	VoteWrapper VItem(ClientID, VWF_SEPARATE | VWF_STYLE_STRICT_BOLD, "Do you want to buy?");
	if(!pItem->Info()->IsStackable())
	{
		VItem.Add("{} {}", (HasAlreadyItem ? "✔" : "×"), pItem->Info()->GetName());
	}
	else
	{
		VItem.Add("{} (has {})", pItem->Info()->GetName(), pPlayerItem->GetValue());
	}
	VItem.Add(pItem->Info()->GetDescription());
	if(pItem->Info()->HasAttributes())
	{
		VItem.Add("* {}", pItem->Info()->GetStringAttributesInfo(pPlayer, pItem->GetEnchant()));
	}
	VoteWrapper::AddEmptyline(ClientID);

	// show required information
	VoteWrapper VRequired(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_STRICT, "Required");
	VRequired.ReinitNumeralDepthStyles({ { DEPTH_LVL1, DEPTH_LIST_STYLE_BOLD } });

	// show required products
	if(pWarehouse->IsHasFlag(WF_STORAGE))
	{
		const auto* pProductInfo = GS()->GetItemInfo(itProduct);
		const auto ProductCost = pTrade->GetProductsCost();
		const auto MarkHas = pWarehouse->Storage().GetValue() >= ProductCost;
		const char* pLabelMark = MarkHas ? "\u2714" : "\u2718";

		VRequired.MarkList().Add("{} {}x{} ({})", pLabelMark, pProductInfo->GetName(), ProductCost, pWarehouse->Storage().GetValue());
	}

	// show required currency items
	const auto MarkHas = PlayerCurrencyValue >= pTrade->GetPrice();
	const char* pLabelMark = MarkHas ? "\u2714" : "\u2718";
	VRequired.MarkList().Add("{} {}x{$} ({$})", pLabelMark, pCurrency->GetName(), pTrade->GetPrice(), PlayerCurrencyValue);
	VoteWrapper::AddEmptyline(ClientID);

	// show status and button buy
	const char* pError = nullptr;
	if(!pItem->Info()->IsStackable() && HasAlreadyItem)
		pError = "- You can't buy more than one item";
	else if(pWarehouse->IsHasFlag(WF_STORAGE) && pWarehouse->Storage().GetValue() < pTrade->GetProductsCost())
		pError = "- Not enough products to buy";
	else if(pPlayer->Account()->GetTotalGold() < pTrade->GetPrice())
		pError = "- Not enough gold to buy";

	// add element
	if(pError != nullptr)
		VoteWrapper(ClientID).Add(pError);
	else
		VoteWrapper(ClientID).AddOption("WAREHOUSE_BUY_ITEM", pWarehouse->GetID(), TradeID, "Buy");

	VoteWrapper::AddEmptyline(ClientID);
}


bool CWarehouseManager::BuyItem(CPlayer* pPlayer, CWarehouse* pWarehouse, int TradeID) const
{
	if(!pWarehouse || !pWarehouse->GetTrade(TradeID))
		return false;

	const int ClientID = pPlayer->GetCID();
	const auto* pTrade = pWarehouse->GetTrade(TradeID);
	const auto* pItemToBuy = pTrade->GetItem();
	const auto* pCurrency = pWarehouse->GetCurrency();
	auto* pPlayerItemBuy = pPlayer->GetItem(*pItemToBuy);


	// check if the player has the enchanted item in the backpack
	if(pPlayerItemBuy->HasItem() && !pPlayerItemBuy->Info()->IsStackable())
	{
		GS()->Chat(ClientID, "Enchant item maximal count x1 in a backpack!");
		return false;
	}


	// check products
	if(pWarehouse->IsHasFlag(WF_STORAGE))
	{
		const auto ProductsCost = pTrade->GetProductsCost();
		if(pWarehouse->Storage().GetValue() < ProductsCost)
		{
			GS()->Chat(ClientID, "Not enought products in storage! Required '{} products'.", ProductsCost);
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
		pPlayerItemBuy->Add(pItemToBuy->GetValue(), 0, pItemToBuy->GetEnchant());
		GS()->Chat(ClientID, "You exchanged '{} x{$}' for '{} x{}'.",
			pCurrency->GetName(), pTrade->GetPrice(), pItemToBuy->Info()->GetName(), pItemToBuy->GetValue());
		return true;
	}

	return false;
}


bool CWarehouseManager::SellItem(CPlayer* pPlayer, CWarehouse* pWarehouse, int TradeID, int ValueToSell) const
{
	if(!pWarehouse || !pWarehouse->GetTrade(TradeID))
		return false;

	const auto ClientID = pPlayer->GetCID();
	const auto* pTrade = pWarehouse->GetTrade(TradeID);
	const auto* pItem = pTrade->GetItem();
	const auto* pCurrency = pWarehouse->GetCurrency();
	auto* pPlayerCurrencyItem = pPlayer->GetItem(pCurrency->GetID());
	const auto Price = pTrade->GetPrice();

	const auto TotalValue = ValueToSell * pItem->GetValue();
	const auto TotalProducts = ValueToSell * pTrade->GetProductsCost();
	const auto TotalPrice = ValueToSell * Price;

	// try spend by item
	if(ValueToSell > 0 && pPlayer->Account()->SpendCurrency(TotalValue, pItem->GetID()))
	{
		// storage add products
		if(pWarehouse->IsHasFlag(WF_STORAGE))
			pWarehouse->Storage().Add(TotalProducts);

		// add currency for player
		pPlayerCurrencyItem->Add(TotalPrice);
		GS()->Chat(ClientID, "You sold '{} x{}' for '{$} {}'.", pItem->Info()->GetName(), TotalValue, TotalPrice, pCurrency->GetName());
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