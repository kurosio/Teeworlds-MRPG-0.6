/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "warehouse_data.h"

#include <game/server/core/components/Inventory/InventoryManager.h>

void CWarehouse::InitProperties(const std::string& JsonProperties)
{
	dbg_assert(JsonProperties.length() > 0, "The properties string is empty");
	Tools::Json::parseFromString(JsonProperties, [this](nlohmann::json& pJson)
	{
		dbg_assert(pJson.find("type") != pJson.end(), "The importal properties value is empty");

		// Intialize warehouse type
		std::string Type = pJson.value("type", "");
		m_Type = (Type == "selling" ? WarehouseType::Selling : WarehouseType::Shop);

		// Initialize item's
		if(pJson.find("items_collections") != pJson.end())
		{
			auto pJsonItemsCollection = pJson["items_collections"];
			for(const auto& pItem : pJsonItemsCollection)
			{
				std::vector<int> vItems {};
				std::string CollectType = pItem.value("collect_by", "");

				vItems = CollectType == "type" ? CInventoryManager::GetItemIDsCollection((ItemType)pItem.value("value", -1))
					: CInventoryManager::GetItemIDsCollectionByFunction((ItemFunctional)pItem.value("value", -1));

				for(const auto& ItemID : vItems)
				{
					auto ID = (int)m_aTradingList.size();
					if(CItem TradeItem(ItemID, 1, 0); TradeItem.IsValid() && TradeItem.Info()->GetInitialPrice() > 0)
						m_aTradingList.emplace_back(ID, std::move(TradeItem), TradeItem.Info()->GetInitialPrice());
					dbg_msg("test", "ItemID: %d, ID: %d", ItemID, m_ID);
				}
			}
		}

		// Default initilize item's
		if(pJson.find("items") != pJson.end())
		{
			auto pJsonItems = pJson["items"];
			for(const auto& pItem : pJsonItems)
			{
				auto ID = (int)m_aTradingList.size();
				const int& ItemID = pItem.value("id", -1);
				const int& Value = pItem.value("value", 0);
				const int& Enchant = pItem.value("enchant", 0);
				const int& Price = pItem.value("price", 0);
				
				if(CItem TradeItem(ItemID, Value, Enchant); TradeItem.IsValid() && Price > 0)
					m_aTradingList.emplace_back(ID, std::move(TradeItem), Price);
			}
		}
	});
}

CTradeSlot* CWarehouse::GetTradeSlot(TradeIdentifier ID)
{
	auto iter = std::find_if(m_aTradingList.begin(), m_aTradingList.end(), [ID](const CTradeSlot& Trade) { return Trade.GetID() == ID; });
	return iter != m_aTradingList.end() ? &(*iter) : nullptr;
}
