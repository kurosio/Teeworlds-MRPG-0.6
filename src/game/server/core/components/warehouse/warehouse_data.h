#ifndef GAME_SERVER_COMPONENT_WAREHOUSE_DATA_H
#define GAME_SERVER_COMPONENT_WAREHOUSE_DATA_H

#include <teeother/tools/grouped_container.h>
#include <game/server/core/components/inventory/item_data.h>

class CTrade;
using ContainerTradingList = std::deque<CTrade>;

constexpr auto TW_WAREHOUSE_TABLE = "tw_warehouses";

enum WarehouseFlags
{
	WF_NONE = 0,
	WF_BUY = 1 << 0,
	WF_SELL = 1 << 1,
	WF_STORAGE = 1 << 2,
	WF_BUY_STORAGE = WF_BUY | WF_STORAGE,
	WF_SELL_STORAGE = WF_SELL | WF_STORAGE
};

class CTrade
{
	int m_ID {};
	CItem m_Item {};
	int m_Price {};
	int m_ExplicitProducts {};

public:
	CTrade(int ID, CItem&& pItem, int Price, int ExplicitProducts = NOPE)
		: m_ID(ID), m_Item(std::move(pItem)), m_Price(Price), m_ExplicitProducts(ExplicitProducts)
	{
	}

	int GetID() const { return m_ID; }
	CItem* GetItem() { return &m_Item; }
	const CItem* GetItem() const { return &m_Item; }
	int GetPrice() const { return m_Price; }
	int GetExplicitProducts() const { return m_ExplicitProducts; }

	int GetProductsCost() const
	{
		if(m_ExplicitProducts != NOPE)
			return m_ExplicitProducts;

		auto Price = maximum(1, GetPrice());
		return translate_to_percent_rest(Price, g_Config.m_SvPercentProductsByPrice);
	}
};

// Warehouse
class CWarehouse : public MultiworldIdentifiableData<std::deque<CWarehouse*>>
{
	class CStorage
	{
		friend class CWarehouse;
		CWarehouse* m_pWarehouse {};
		BigInt m_Value {};
		vec2 m_TextPos {};

	public:
		CStorage() : m_pWarehouse(nullptr), m_Value(0), m_TextPos(0, 0) { }

		void Init(CWarehouse* pWarehouse)
		{
			m_pWarehouse = pWarehouse;
		}

		bool Remove(const BigInt& Value)
		{
			if(m_pWarehouse && m_pWarehouse->IsHasFlag(WF_STORAGE) && m_Value >= Value)
			{
				m_Value -= Value;
				m_pWarehouse->SaveData();
				return true;
			}
			return false;
		}

		void Add(const BigInt& Value)
		{
			if(m_pWarehouse && m_pWarehouse->IsHasFlag(WF_STORAGE))
			{
				m_Value += Value;
				m_pWarehouse->SaveData();
			}
		}

		BigInt GetValue() const { return m_Value; }
		vec2 GetTextPos() const { return m_TextPos; }
		void SetTextPos(vec2 Pos) { m_TextPos = Pos; }
		void SetValue(const BigInt& Value) { m_Value = Value; }
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
	mystd::grouped_container<CTrade> m_GroupedTrades;

public:
	static const std::string s_DefaultSubgroupKey;
	CWarehouse() : m_GroupedTrades(s_DefaultSubgroupKey) { }

	static CWarehouse* CreateElement(const int WarehouseID) noexcept
	{
		auto pData = new CWarehouse;
		pData->m_ID = WarehouseID;
		return m_pData.emplace_back(pData);
	}

	void Init(const std::string& Name, const DBSet& Type, const std::string& TradesStr,
		const nlohmann::json& StorageData, vec2 Pos, int Currency, int WorldID);
	void InitData(const DBSet& Type, const std::string& TradesStr, const nlohmann::json& StorageData);
	void SaveData();

	int GetID() const { return m_ID; }
	bool IsHasFlag(int Flag) const { return m_Flags & Flag; }
	const char* GetName() const { return m_Name.c_str(); }
	vec2 GetPos() const { return m_Pos; }
	int GetWorldID() const { return m_WorldID; }

	CItemDescription* GetCurrency() const;
	CStorage& Storage() { return m_Storage; }
	const CStorage& Storage() const { return m_Storage; }
	CTrade* GetTrade(int TradeID);
	const ContainerTradingList& GetTradingList() const { return m_vTradingList; }

	const mystd::grouped_container<CTrade>& GetGroupedTrades() const { return m_GroupedTrades; }
	mystd::grouped_container<CTrade>& GetGroupedTrades() { return m_GroupedTrades; }

private:
	void ParseCollectionBlock(const std::string& block_content, const std::string& currentGroup, const std::string& currentSubgroup);
	void ParseItemBlock(const std::string& block_content, const std::string& currentGroup, const std::string& currentSubgroup);
	void InitializeStorage(const nlohmann::json& StorageData);
	void InitializeFlags(const DBSet& Type);
};

#endif
