/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_WAREHOUSE_DATA_H
#define GAME_SERVER_COMPONENT_WAREHOUSE_DATA_H

#include <game/server/mmocore/Components/Warehouse/TradingSlot.h>

using WarehouseIdentifier = int;

class CWarehouse : public MultiworldIdentifiableStaticData< std::map< int, CWarehouse > >
{
public:
	using ContainerTradingSlots = std::deque<CTradingSlot>;

private:
	WarehouseIdentifier m_ID{};
	char m_aName[32]{};
	vec2 m_Pos{};
	int m_Currency{};
	int m_WorldID{};

public:
	ContainerTradingSlots m_aTradingSlots{};

	CWarehouse() = default;
	CWarehouse(WarehouseIdentifier ID) : m_ID(ID) {}

	void Init(const std::string& Name, vec2 Pos, int Currency, int WorldID)
	{
		str_copy(m_aName, Name.c_str(), sizeof(m_aName));
		m_Pos = Pos;
		m_Currency = Currency;
		m_WorldID = WorldID;
		CWarehouse::m_pData[m_ID] = *this;
	}

	WarehouseIdentifier GetID() const { return m_ID; }

	const char* GetName() const { return m_aName; }
	vec2 GetPos() const { return m_Pos; }
	CItemDescription* GetCurrency() const { return &CItemDescription::Data()[m_Currency]; }
	int GetWorldID() const { return m_WorldID; }
};

#endif