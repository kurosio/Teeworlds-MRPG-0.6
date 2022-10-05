/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_AUCTION_DATA_H
#define GAME_SERVER_COMPONENT_AUCTION_DATA_H

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

#endif

