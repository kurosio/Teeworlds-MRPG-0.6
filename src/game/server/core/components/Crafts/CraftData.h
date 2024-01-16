/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_CRAFT_DATA_H
#define GAME_SERVER_COMPONENT_CRAFT_DATA_H

#include "game/server/core/components/Inventory/ItemData.h"

using CraftIdentifier = int;

using CraftPtr = std::shared_ptr< class CCraftItem >;

class CCraftItem : public MultiworldIdentifiableStaticData< std::deque< CraftPtr > >
{
	CraftIdentifier m_ID {};
	CItem m_Item {};
	CItemsContainer m_RequiredItem;
	int m_Price {};
	int m_WorldID {};

public:
	CCraftItem() = default; // Default constructor for CCraftItem

	// Create a static function called CreateElement which creates a new CCraftItem object and adds it to the data container
	static CraftPtr CreateElement(CraftIdentifier ID)
	{
		CCraftItem p;
		p.m_ID = ID;
		return m_pData.emplace_back(std::make_shared<CCraftItem>(p));
	}

	// Initialize the CCraftItem object with the required container for required items, crafted item, price, and world ID where can craft item
	void Init(CItemsContainer RequiredContainer, CItem Item, int Price, int WorldID)
	{
		m_RequiredItem = std::move(RequiredContainer);
		m_Item = std::move(Item);
		m_Price = Price;
		m_WorldID = WorldID;
	}

	// Getter methods for craft item
	CraftIdentifier GetID() const { return m_ID; }                             // Get the ID of the craft item
	CItem* GetItem() { return &m_Item; }                                       // Get a pointer to the crafted item
	const CItem* GetItem() const { return &m_Item; }                           // Get a constant pointer to the crafted item
	CItemsContainer& GetRequiredItems() { return m_RequiredItem; }             // Get a reference to the required items container
	const CItemsContainer& GetRequiredItems() const { return m_RequiredItem; } // Get a constant reference to the required items container
	int GetPrice(class CPlayer* pPlayer) const;                                // Get the price of the craft item for a given player
	int GetWorldID() const { return m_WorldID; }                               // Get the world ID of the craft item
};

#endif