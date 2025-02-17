/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "warehouse_data.h"

#include <game/server/entity_manager.h>
#include <game/server/gamecontext.h>
#include <game/server/core/components/Inventory/InventoryManager.h>

void CWarehouse::Init(const std::string& Name, const DBSet& Type, const std::string& Data, vec2 Pos, int Currency, int WorldID)
{
	m_Flags = WF_NONE;
	m_Name = Name;
	m_Pos = Pos;
	m_Currency = Currency;
	m_WorldID = WorldID;

	InitJsonData(Type, Data);
}

void CWarehouse::InitJsonData(const DBSet& Type, const std::string& Data)
{
	// check properties exist
	dbg_assert(!Data.empty(), "The data json string is empty");


	// initialize type flags
	if(Type.hasSet("buying"))
		m_Flags |= WF_BUY;
	else if(Type.hasSet("selling"))
		m_Flags |= WF_SELL;


	// initialize other flags
	if(Type.hasSet("storage"))
		m_Flags |= WF_STORAGE;


	// parse properties
	mystd::json::parse(Data, [this](nlohmann::json& pJson)
	{
		// initialize trade list
		if(m_Flags & (WF_BUY | WF_SELL))
		{
			// by collections
			if(pJson.contains("items_collections"))
			{
				for(const auto& pItem : pJson["items_collections"])
				{
					const auto dbSetGroup = DBSet(pItem.value("group", ""));
					const auto dbSetType = DBSet(pItem.value("type", ""));
					const auto groupOpt = GetItemGroupFromDBSet(dbSetGroup);
					const auto typeOpt = GetItemTypeFromDBSet(dbSetType);
					auto vItems = CInventoryManager::GetItemsCollection(groupOpt, typeOpt);

					// adding all item's from collection to trade list
					for(const int ItemID : vItems)
					{
						auto Item = CItem(ItemID, 1, 0);
						const auto HasInitialPrice = Item.Info()->GetInitialPrice() > 0;

						if(Item.IsValid() && HasInitialPrice)
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
					const auto ItemID = pItem.value("id", -1);
					const auto Value = pItem.value("value", 1);
					const auto Enchant = pItem.value("enchant", 0);
					const auto Price = pItem.value("price", 0);
					auto Item = CItem(ItemID, Value, Enchant);

					// adding item to trading list
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
			m_Storage.m_TextPos = JsonStorage.value("position", vec2());
			m_Storage.m_pWarehouse = this;
		}

		// sorting by group and price
		std::ranges::sort(m_vTradingList, [](const CTrade& a, const CTrade& b)
		{
			const auto aPrice = a.GetItem()->Info()->GetInitialPrice();
			const auto bPrice = b.GetItem()->Info()->GetInitialPrice();

			if(aPrice != bPrice)
			{
				return aPrice < bPrice;
			}

			const auto aGroup = a.GetItem()->Info()->GetGroup();
			const auto bGroup = b.GetItem()->Info()->GetGroup();
			if(aGroup != bGroup)
			{
				return aGroup < bGroup;
			}

			const auto aType = a.GetItem()->Info()->GetType();
			const auto bType = b.GetItem()->Info()->GetType();
			return aType < bType;
		});


		// information about initialize
		dbg_msg("warehouse", "'%s' initialized. (storage: '%s', type: '%s', size: '%lu')",
			m_Name.c_str(),
			IsHasFlag(WF_STORAGE) ? "Yes" : "No",
			IsHasFlag(WF_BUY) ? "Buy" : "Sell",
			m_vTradingList.size());

		// save properties data for changes
		m_Properties = std::move(pJson);
	});
}

void CWarehouse::SaveData()
{
	// update storage value
	if(IsHasFlag(WF_STORAGE))
	{
		auto& JsonStorage = m_Properties["storage"];
		JsonStorage["value"] = m_Storage.GetValue();
	}

	// save to db
	Database->Execute<DB::UPDATE>(TW_WAREHOUSE_TABLE, "Data = '{}' WHERE ID = '{}'", m_Properties.dump().c_str(), m_ID);
}

CTrade* CWarehouse::GetTrade(int TradeID)
{
	const auto iter = std::ranges::find_if(m_vTradingList, [TradeID](const CTrade& Trade)
	{
		return Trade.GetID() == TradeID;
	});

	return iter != m_vTradingList.end() ? std::to_address(iter) : nullptr;
}

void CWarehouse::CStorage::UpdateText(int LifeTime) const
{
	const auto* pGS = (CGS*)Instance::GameServer(m_pWarehouse->m_WorldID);

	pGS->EntityManager()->Text(m_TextPos, LifeTime, m_Value.to_string().c_str());
}