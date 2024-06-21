/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_COMPONENTS_AUCTION_AUCTION_DATA_H
#define GAME_SERVER_CORE_COMPONENTS_AUCTION_AUCTION_DATA_H

#include <game/server/core/components/Inventory/ItemData.h>

constexpr auto TW_AUCTION_SLOTS_TABLE = "tw_auction_slots";
class CAuctionSlot : public MultiworldIdentifiableData<std::deque<CAuctionSlot*>>
{
	int m_ID {};
	CItem m_Item {};
	int m_Price {};
	int m_OwnerID {};

public:
	CAuctionSlot() = default;

	// create new element
	static CAuctionSlot* CreateElement(int ID)
	{
		auto* pAuctionSlot = new CAuctionSlot;
		pAuctionSlot->m_ID = ID;
		return m_pData.emplace_back(pAuctionSlot);
	}

	// functions
	void Init(const CItem& Item, int Price, int OwnerID)
	{
		m_Item = Item;
		m_Price = Price;
		m_OwnerID = OwnerID;
	}

	int GetID() const { return m_ID; }

	// Setter methods for item and price
	void SetItem(CItem Item) { m_Item = std::move(Item); }
	void SetPrice(int Price) { m_Price = Price; }

	// Getter methods for item and price
	CItem* GetItem() { return &m_Item; }                           // Return a pointer to the item
	const CItem* GetItem() const { return &m_Item; }               // Return a const pointer to the item (for const objects)
	int GetPrice() const { return m_Price; }                       // Return the price
	int GetTaxPrice() const;                                       // Declaration for a function to calculate the tax price
	int GetOwnerID() const { return m_OwnerID; }
};
#endif

