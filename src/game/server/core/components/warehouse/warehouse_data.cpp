/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "warehouse_data.h"

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
	dbg_msg("warehouse", "Warehouse '%s' is initializing...", m_aName);
	dbg_assert(Properties.length() > 0, "The properties string is empty");
	Tools::Json::parseFromString(Properties, [this](nlohmann::json& pJson)
	{
		dbg_assert(pJson.find("type") != pJson.end(), "The importal properties value is empty");

		// Intialize warehouse type
		std::string Type = pJson.value("type", "");
		if(Type == "buying")
			m_Flags = WF_BUY;
		else if(Type == "selling")
			m_Flags = WF_SELL;
		else if(Type == "buying_storage")
			m_Flags = WF_BUY_STORAGE;
		else if(Type == "selling_storage")
			m_Flags = WF_SELL_STORAGE;
		else
			dbg_assert(false, "The warehouse type is not valid");

		if(m_Flags & (WF_BUY | WF_SELL))
		{
			// Initialize by collections
			if(pJson.contains("items_collections"))
			{
				auto pJsonItemsCollection = pJson["items_collections"];
				for(const auto& pItem : pJsonItemsCollection)
				{
					std::vector<int> vItems {};
					std::string CollectType = pItem.value("collect_by", "");

					if(CollectType == "type")
						vItems = CInventoryManager::GetItemIDsCollection(static_cast<ItemType>(pItem.value("value", -1)));
					else
						vItems = CInventoryManager::GetItemIDsCollectionByFunction(static_cast<ItemFunctional>(pItem.value("value", -1)));

					for(const auto& ItemID : vItems)
					{
						auto ID = (int)m_aTradingList.size();
						if(CItem Item(ItemID, 1, 0); Item.IsValid() && Item.Info()->GetInitialPrice() > 0)
							m_aTradingList.emplace_back(ID, std::move(Item), Item.Info()->GetInitialPrice());
					}
				}
			}

			// Default initilize item's
			if(pJson.contains("items"))
			{
				auto pJsonItems = pJson["items"];
				for(const auto& pItem : pJsonItems)
				{
					auto ID = (int)m_aTradingList.size();
					const int& ItemID = pItem.value("id", -1);
					const int& Value = pItem.value("value", 1);
					const int& Enchant = pItem.value("enchant", 0);
					const int& Price = pItem.value("price", 0);

					if(CItem Item(ItemID, Value, Enchant); Item.IsValid() && Price > 0)
						m_aTradingList.emplace_back(ID, std::move(Item), Price);
				}
			}
		}

		// Initialize storage
		if(m_Flags & WF_STORAGE)
		{
			dbg_assert(pJson.contains("storage"), "the flags is specified as having a storage, but it is impossible to initialize it");
			auto JsonStorage = pJson["storage"];
			bool Value = JsonStorage.value("value", 0);
			vec2 TextPos = vec2(JsonStorage.value("x", 0), JsonStorage.value("y", 0));

			m_Storage.m_pWarehouse = this;
			m_Storage.m_Value = Value;
			m_Storage.m_TextPos = TextPos;
		}

		dbg_msg("warehouse", "Warehouse '%s' has been initialized. (Storage: '%s' Type: '%s').", 
			m_aName, IsHasFlag(WF_STORAGE) ? "Yes" : "No", IsHasFlag(WF_BUY) ? "Buy" : "Sell");
		m_Properties = std::move(pJson);
	});
}

void CWarehouse::SaveProperties()
{
	if(IsHasFlag(WF_STORAGE) && m_Properties.contains("storage"))
	{
		auto& JsonStorage = m_Properties["storage"];
		JsonStorage["value"] = m_Storage.GetValue();
	}

	Database->Execute<DB::UPDATE>(TW_WAREHOUSE_TABLE, "Properties = '%s' WHERE ID = '%d'", m_Properties.dump().c_str(), m_ID);
}

CTrade* CWarehouse::GetTrade(int ID)
{
	auto iter = std::find_if(m_aTradingList.begin(), m_aTradingList.end(), [ID](const CTrade& Trade) { return Trade.GetID() == ID; });
	return iter != m_aTradingList.end() ? &(*iter) : nullptr;
}

/*
 * Storage
 */
void CWarehouse::CStorage::UpdateText(int LifeTime) const
{
	// Update storage text
	CGS* pGS = (CGS*)Instance::GameServer(m_pWarehouse->m_WorldID);
	pGS->CreateText(nullptr, false, m_TextPos, {}, LifeTime, std::to_string(m_Value).c_str());
}
