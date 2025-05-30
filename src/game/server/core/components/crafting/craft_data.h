#ifndef GAME_SERVER_COMPONENT_CRAFT_DATA_H
#define GAME_SERVER_COMPONENT_CRAFT_DATA_H

#include "game/server/core/components/inventory/item_data.h"

using CraftIdentifier = int;
class CCraftItem : public MultiworldIdentifiableData< std::deque<CCraftItem*> >
{
	CraftIdentifier m_ID {};
	CItem m_Item {};
	CItemsContainer m_RequiredItem;
	int m_Price {};
	int m_WorldID {};

public:
	explicit CCraftItem() = default;
	explicit CCraftItem(CraftIdentifier ID) : m_ID(ID) {}

	static CCraftItem* CreateElement(int ID)
	{
		auto pData = new CCraftItem(ID);
		pData->m_ID = ID;
		return m_pData.emplace_back(std::move(pData));
	}

	void Init(CItemsContainer RequiredContainer, CItem Item, int Price, int WorldID)
	{
		m_RequiredItem = std::move(RequiredContainer);
		m_Item = std::move(Item);
		m_Price = Price;
		m_WorldID = WorldID;
	}

	// Getter methods for craft item
	CraftIdentifier GetID() const { return m_ID; }
	CItem* GetItem() { return &m_Item; }
	const CItem* GetItem() const { return &m_Item; }
	CItemsContainer& GetRequiredItems() { return m_RequiredItem; }
	const CItemsContainer& GetRequiredItems() const { return m_RequiredItem; }
	int GetPrice(class CPlayer* pPlayer = nullptr) const;
	int GetWorldID() const { return m_WorldID; }
};

#endif