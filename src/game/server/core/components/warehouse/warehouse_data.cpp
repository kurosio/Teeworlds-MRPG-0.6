/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "warehouse_data.h"

#include <game/server/entity_manager.h>
#include <game/server/gamecontext.h>
#include <game/server/core/components/Inventory/InventoryManager.h>

/*
 * Json structure information:
 * * type - warehouse type (buying, selling, buying_storage, selling_storage)
 * * items_collections - collection of items by type or function (collect_by, value)
 * * items - collection of items (id, value, enchant, price)
 */

void CWarehouse::Init(const std::string& Name, const std::string& Properties, vec2 Pos, int Currency, int WorldID)
{
	str_copy(m_aName, Name.c_str(), sizeof(m_aName));
	m_Pos = Pos;
	m_Currency = Currency;
	m_WorldID = WorldID;

	InitProperties(Properties);
}

void CWarehouse::InitProperties(const std::string& Properties)
{
	// check properties exist
	dbg_assert(!Properties.empty(), "The properties string is empty");

	// parse properties
	mystd::json::parse(Properties, [this](nlohmann::json& pJson)
	{
		// check importal properties values
		std::string Type = pJson.value("type", "");
		dbg_assert(!Type.empty(), "The 'type' property is missing or empty");

		// intialize warehouse flags
		if(Type == "buying")
		{
			m_Flags = WF_BUY;
		}
		else if(Type == "selling")
		{
			m_Flags = WF_SELL;
		}
		else if(Type == "buying_storage")
		{
			m_Flags = WF_BUY_STORAGE;
		}
		else if(Type == "selling_storage")
		{
			m_Flags = WF_SELL_STORAGE;
		}
		else
		{
			dbg_assert(false, "Invalid warehouse type");
		}

		// initialize trade list
		if(m_Flags & (WF_BUY | WF_SELL))
		{
			// by collections
			if(pJson.contains("items_collections"))
			{
				for(const auto& pItem : pJson["items_collections"])
				{
					std::string CollectType = pItem.value("collect_by", "");
					std::vector<int> vItems;

					// by type or function collection
					if(CollectType == "type")
					{
						vItems = CInventoryManager::GetItemIDsCollection(static_cast<ItemType>(pItem.value("value", -1)));
					}
					else
					{
						vItems = CInventoryManager::GetItemIDsCollectionByFunction(static_cast<ItemFunctional>(pItem.value("value", -1)));
					}

					// adding all item's from collection to trade list
					for(const int ItemID : vItems)
					{
						auto Item = CItem(ItemID, 1, 0);

						if(Item.IsValid() && Item.Info()->GetInitialPrice() > 0)
						{
							m_vTradingList.emplace_back(m_vTradingList.size(), std::move(Item), Item.Info()->GetInitialPrice());
						}
					}
				}
			}

			// by item's
			if(pJson.contains("items"))
			{
				for(const auto& pItem : pJson["items"])
				{
					const int ItemID = pItem.value("id", -1);
					const int Value = pItem.value("value", 1);
					const int Enchant = pItem.value("enchant", 0);
					const int Price = pItem.value("price", 0);
					auto Item = CItem(ItemID, Value, Enchant);

					if(Item.IsValid() && Price > 0)
					{
						m_vTradingList.emplace_back(m_vTradingList.size(), std::move(Item), Price);
					}
				}
			}
		}

		// initialize storage
		if(m_Flags & WF_STORAGE)
		{
			// check storage properties value
			dbg_assert(pJson.contains("storage"), "The warehouse is flagged as storage, but no storage data provided");

			// initialize storage
			const auto JsonStorage = pJson["storage"];
			m_Storage.m_Value = JsonStorage.value("value", BigInt(0));
			m_Storage.m_TextPos = vec2(JsonStorage.value("x", 0), JsonStorage.value("y", 0));
			m_Storage.m_pWarehouse = this;
		}

		// information about initialize
		dbg_msg("warehouse", "'%s' initialized. (storage: '%s', type: '%s', size: '%lu')",
			m_aName,
			IsHasFlag(WF_STORAGE) ? "Yes" : "No",
			IsHasFlag(WF_BUY) ? "Buy" : "Sell",
			m_vTradingList.size());

		// save properties data for changes
		m_Properties = std::move(pJson);
	});
}

void CWarehouse::SaveProperties()
{
	// update storage value
	if(IsHasFlag(WF_STORAGE))
	{
		auto& JsonStorage = m_Properties["storage"];
		JsonStorage["value"] = m_Storage.GetValue();
	}

	// save to db
	Database->Execute<DB::UPDATE>(TW_WAREHOUSE_TABLE, "Properties = '{}' WHERE ID = '{}'", m_Properties.dump().c_str(), m_ID);
}

CTrade* CWarehouse::GetTrade(int TradeID)
{
	const auto iter = std::ranges::find_if(m_vTradingList, [TradeID](const CTrade& Trade)
	{
		return Trade.GetID() == TradeID;
	});

	return iter != m_vTradingList.end() ? &(*iter) : nullptr;
}

void CWarehouse::CStorage::UpdateText(int LifeTime) const
{
	const auto* pGS = (CGS*)Instance::GameServer(m_pWarehouse->m_WorldID);

	pGS->EntityManager()->Text(m_TextPos, LifeTime, m_Value.to_string().c_str());
}