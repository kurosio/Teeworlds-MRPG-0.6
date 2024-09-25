/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_WAREHOUSE_DATA_H
#define GAME_SERVER_COMPONENT_WAREHOUSE_DATA_H

#include <game/server/core/components/Inventory/ItemData.h>

constexpr auto TW_WAREHOUSE_TABLE = "tw_warehouses";
using WarehouseIdentifier = int;
using TradeIdentifier = int;

/*
 * Warehouse flags
 */
enum WarehouseFlags
{
	WF_NONE = 0,
	WF_BUY = 1 << 0,
	WF_SELL = 1 << 1,
	WF_STORAGE = 1 << 2,
	WF_BUY_STORAGE = WF_BUY | WF_STORAGE,
	WF_SELL_STORAGE = WF_SELL | WF_STORAGE
};

/*
 * Trading slot
 *
 */
class CTrade
{
	TradeIdentifier m_ID {};
	CItem m_Item {};
	int m_Price {};

public:
	CTrade(TradeIdentifier ID, CItem&& pItem, int Price) : m_ID(ID), m_Item(std::move(pItem)), m_Price(Price) {}

	// default getters and setters
	TradeIdentifier GetID() const { return m_ID; }
	CItem* GetItem() { return &m_Item; }
	const CItem* GetItem() const { return &m_Item; }
	int GetPrice() const { return m_Price; }
	int GetProductsCost() const
	{
		auto Cost = m_Item.Info()->GetDysenthis(m_Item.GetEnchant());
		Cost = (Cost <= 0) ? m_Item.GetValue() : Cost;
		return Cost;
	}
};

/*
 * Warehouse
 *
 */
class CWarehouseStorage;
using ContainerTradingList = std::deque<CTrade>;
class CWarehouse : public MultiworldIdentifiableData<std::deque<CWarehouse*>>
{
	// storage inner structure
	class CStorage
	{
		friend class CWarehouse;
		BigInt m_Value {};
		vec2 m_TextPos {};
		CWarehouse* m_pWarehouse {};

	public:
		// remove storage value
		bool Remove(const BigInt& Value)
		{
			if(m_pWarehouse->IsHasFlag(WF_STORAGE) && m_Value >= Value)
			{
				m_Value -= Value;
				m_pWarehouse->SaveProperties();
				return true;
			}
			return false;
		}

		// add storage value
		void Add(const BigInt& Value)
		{
			if(m_pWarehouse->IsHasFlag(WF_STORAGE))
			{
				m_Value += Value;
				m_pWarehouse->SaveProperties();
			}
		}

		void UpdateText(int LifeTime) const;
		BigInt GetValue() const { return m_Value; }
	};

	nlohmann::json m_Properties {};
	WarehouseIdentifier m_ID {};
	int64_t m_Flags {};
	char m_aName[32] {};
	vec2 m_Pos {};
	int m_Currency {};
	int m_WorldID {};
	CStorage m_Storage {};
	ContainerTradingList m_vTradingList {};

public:
	CWarehouse() = default;

	static CWarehouse* CreateElement(const WarehouseIdentifier& ID) noexcept
	{
		auto pData = new CWarehouse;
		pData->m_ID = ID;
		return m_pData.emplace_back(pData);
	}

	// functions
	void Init(const std::string& Name, const std::string& Properties, vec2 Pos, int Currency, int WorldID);
	void InitProperties(const std::string& Properties);
	void SaveProperties();

	// getters and setters
	WarehouseIdentifier GetID() const { return m_ID; }
	bool IsHasFlag(int Flag) const { return m_Flags & Flag; }
	const char* GetName() const { return m_aName; }
	vec2 GetPos() const { return m_Pos; }
	CItemDescription* GetCurrency() const { return &CItemDescription::Data()[m_Currency]; }
	int GetWorldID() const { return m_WorldID; }
	CStorage& Storage() { return m_Storage; }
	CTrade* GetTrade(TradeIdentifier ID);
	const ContainerTradingList& GetTradingList() const { return m_vTradingList; }
};

#endif