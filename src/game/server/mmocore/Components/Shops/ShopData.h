/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_SHOP_DATA_H
#define GAME_SERVER_COMPONENT_SHOP_DATA_H

#include <game/server/mmocore/Components/Inventory/ItemData.h>

class CAuctionSlot
{
	CItem m_Item{};
	int m_Price{};

public:
	CAuctionSlot() = default;
	CAuctionSlot(CItem Item, int Price) : m_Item(std::move(Item)), m_Price(Price) {}

	void SetItem(CItem Item) { m_Item = std::move(Item); }
	void SetPrice(int Price) { m_Price = Price; }

	CItem* GetItem() { return &m_Item; }
	const CItem* GetItem() const { return &m_Item; }
	int GetPrice() const { return m_Price; }
};


class CTradingSpot : public MultiworldIdentifiableStaticData< std::map< int, CTradingSpot > >
{
	int m_ID{};
	int m_WarehouseID{};

public:
	CTradingSpot() = default;
	CTradingSpot(int ID) : m_ID(ID) {}

	void Init(int WarehouseID)
	{
		m_WarehouseID = WarehouseID;
		CTradingSpot::m_pData[m_ID] = *this;
	}

	int GetWarehouseID() const { return m_WarehouseID; }
	int GetID() const { return m_ID; }
};

#endif

