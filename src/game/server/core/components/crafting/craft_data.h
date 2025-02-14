#ifndef GAME_SERVER_COMPONENT_CRAFT_DATA_H
#define GAME_SERVER_COMPONENT_CRAFT_DATA_H

#include "game/server/core/components/Inventory/ItemData.h"

using CraftIdentifier = int;

class CCraftItem : public MultiworldIdentifiableData< std::map<std::string, std::deque<CCraftItem*> >>
{
	CraftIdentifier m_ID {};
	CItem m_Item {};
	CItemsContainer m_RequiredItem;
	int m_Price {};
	int m_WorldID {};

public:
	// constructor
	explicit CCraftItem(CraftIdentifier ID) : m_ID(ID) {}

	// create new element to container
	static void CreateGroup(const std::string& GroupName, const std::vector<CCraftItem>& vElem)
	{
		for(auto& elem : vElem)
		{
			auto pData = new CCraftItem(elem);
			m_pData[GroupName].emplace_back(pData);
		}
	}

	// initialize
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
	int GetPrice(class CPlayer* pPlayer = nullptr) const;                      // Get the price of the craft item for a given player
	int GetWorldID() const { return m_WorldID; }                               // Get the world ID of the craft item
};

#endif