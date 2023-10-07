/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_AUCTION_DATA_H
#define GAME_SERVER_COMPONENT_AUCTION_DATA_H

#include <game/server/mmocore/Components/Inventory/ItemData.h>

class CAuctionSlot
{
	CItem m_Item {};
	int m_Price {};

public:
	// Default constructor
	CAuctionSlot() = default;

	// Parameterized constructor
	CAuctionSlot(CItem Item, int Price) : m_Item(std::move(Item)), m_Price(Price) {}

	// Setter methods for item and price
	void SetItem(CItem Item) { m_Item = std::move(Item); }
	void SetPrice(int Price) { m_Price = Price; }

	// Getter methods for item and price
	CItem* GetItem() { return &m_Item; }                           // Return a pointer to the item
	const CItem* GetItem() const { return &m_Item; }               // Return a const pointer to the item (for const objects)
	int GetPrice() const { return m_Price; }                       // Return the price
	int GetTaxPrice() const;                                       // Declaration for a function to calculate the tax price
};
#endif

