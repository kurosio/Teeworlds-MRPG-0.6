/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_WAREHOUSE_DATA_H
#define GAME_SERVER_COMPONENT_WAREHOUSE_DATA_H

#include <game/server/core/components/Inventory/ItemData.h>

class CTrade;
class CWarehouseStorage;
using ContainerTradingList = std::deque<CTrade>;
constexpr auto TW_WAREHOUSE_TABLE = "tw_warehouses";

// warehouse flags
enum WarehouseFlags
{
	WF_NONE = 0,
	WF_BUY = 1 << 0,
	WF_SELL = 1 << 1,
	WF_STORAGE = 1 << 2,
	WF_BUY_STORAGE = WF_BUY | WF_STORAGE,
	WF_SELL_STORAGE = WF_SELL | WF_STORAGE
};

// trade slot
class CTrade
{
	int m_ID {};
	CItem m_Item {};
	int m_Price {};

public:
	CTrade(int ID, CItem&& pItem, int Price)
		: m_ID(ID), m_Item(std::move(pItem)), m_Price(Price) {}

	// get trade identifier
	int GetID() const
	{
		return m_ID;
	}

	// get trade item
	CItem* GetItem()
	{
		return &m_Item;
	}

	// get trade item
	const CItem* GetItem() const
	{
		return &m_Item;
	}

	// get price
	int GetPrice() const
	{
		return m_Price;
	}

	// get product cost
	int GetProductsCost() const
	{
		auto Cost = m_Item.Info()->GetDysenthis(m_Item.GetEnchant());
		Cost = (Cost <= 0) ? m_Item.GetValue() : Cost;
		return Cost;
	}
};

// warehouse
class CWarehouse : public MultiworldIdentifiableData<std::deque<CWarehouse*>>
{
	// storage inner structure
	class CStorage
	{
		friend class CWarehouse;
		CWarehouse* m_pWarehouse {};
		BigInt m_Value {};
		vec2 m_TextPos {};

	public:
		// remove storage value
		bool Remove(const BigInt& Value)
		{
			if(m_pWarehouse->IsHasFlag(WF_STORAGE) && m_Value >= Value)
			{
				m_Value -= Value;
				m_pWarehouse->SaveData();
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
				m_pWarehouse->SaveData();
			}
		}

		// get value
		BigInt GetValue() const
		{
			return m_Value;
		}

		// update text storage product value
		void UpdateText(int LifeTime) const;

	};

	int m_ID {};
	int64_t m_Flags {};
	std::string m_Name {};
	vec2 m_Pos {};
	int m_Currency {};
	int m_WorldID {};
	CStorage m_Storage {};
	ContainerTradingList m_vTradingList {};
	nlohmann::json m_Properties {};

public:
	CWarehouse() = default;

	static CWarehouse* CreateElement(const int WarehouseID) noexcept
	{
		auto pData = new CWarehouse;
		pData->m_ID = WarehouseID;
		return m_pData.emplace_back(pData);
	}

	// initialize
	void Init(const std::string& Name, const DBSet& Type, const std::string& Data, vec2 Pos, int Currency, int WorldID);
	void InitJsonData(const DBSet& Type, const std::string& Data);
	void SaveData();

	int GetID() const { return m_ID; }
	bool IsHasFlag(int Flag) const { return m_Flags & Flag; }
	const char* GetName() const { return m_Name.c_str(); }
	vec2 GetPos() const { return m_Pos; }
	int GetWorldID() const { return m_WorldID; }

	CItemDescription* GetCurrency() const { return &CItemDescription::Data()[m_Currency]; }
	CStorage& Storage() { return m_Storage; }
	CTrade* GetTrade(int TradeID);
	const ContainerTradingList& GetTradingList() const { return m_vTradingList; }
};

#endif