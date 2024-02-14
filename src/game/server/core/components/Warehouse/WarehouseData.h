/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_WAREHOUSE_DATA_H
#define GAME_SERVER_COMPONENT_WAREHOUSE_DATA_H

using WarehouseIdentifier = int;
using TradeIdentifier = int;

#include <game/server/core/components/Inventory/ItemData.h>

/*
 * Trading slot
 */
class CTradeSlot
{
	TradeIdentifier m_ID {};
	CItem* m_pTradeItem {};
	int m_Price {};

public:
	CTradeSlot() = default;
	CTradeSlot(TradeIdentifier ID) : m_ID(ID) {}

	// Initialize the trading slot
	void Init(CItem* pItem, int Price)
	{
		m_pTradeItem = pItem;
		m_Price = Price;
	}

	TradeIdentifier GetID() const { return m_ID; }
	CItem* GetTradeItem() const { return m_pTradeItem; }
	int GetPrice() const { return m_Price; }
};

/*
 * Warehouse
 */
using ContainerTradingList = std::deque<CTradeSlot>;
class CWarehouse : public MultiworldIdentifiableStaticData< std::deque< CWarehouse* > >
{
	WarehouseIdentifier m_ID{};
	char m_aName[32]{};
	vec2 m_Pos{};
	int m_Currency{};
	int m_WorldID{};
	ContainerTradingList m_aTradingList {};

public:
	CWarehouse() = default;

	static CWarehouse* CreateElement(const WarehouseIdentifier& ID) noexcept
	{
		auto pData = new CWarehouse;
		pData->m_ID = ID;
		return m_pData.emplace_back(pData);
	}

	// Initialize the warehouse
	void Init(const std::string& Name, vec2 Pos, int Currency, int WorldID)
	{
		str_copy(m_aName, Name.c_str(), sizeof(m_aName));
		m_Pos = Pos;
		m_Currency = Currency;
		m_WorldID = WorldID;
	}

	void InitTradingList(const ContainerTradingList& DataContainer) noexcept
	{
		m_aTradingList = DataContainer;
	}

	WarehouseIdentifier GetID() const { return m_ID; }
	const char* GetName() const { return m_aName; }
	vec2 GetPos() const { return m_Pos; }
	CItemDescription* GetCurrency() const { return &CItemDescription::Data()[m_Currency]; }
	int GetWorldID() const { return m_WorldID; }
	CTradeSlot* GetTradeSlot(TradeIdentifier ID);

	const ContainerTradingList& GetTradingList() const { return m_aTradingList; }
};

#endif