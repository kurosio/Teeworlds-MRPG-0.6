/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_CRAFT_DATA_H
#define GAME_SERVER_COMPONENT_CRAFT_DATA_H

#include "game/server/mmocore/Components/Inventory/ItemData.h"

using CraftIdentifier = int;

using CraftPtr = std::shared_ptr< class CCraftItem >;

class CCraftItem : public MultiworldIdentifiableStaticData< std::deque< CraftPtr > >
{
private:
	CraftIdentifier m_ID{};
	CItem m_Item{};
	int m_Price{};
	int m_WorldID{};

public:
	CItemsContainer m_RequiredItem;

	CCraftItem() = default;

	static CraftPtr CreateDataItem(CraftIdentifier ID)
	{
		CCraftItem p;
		p.m_ID = ID;
		return m_pData.emplace_back(std::make_shared<CCraftItem>(p));
	}

	void Init(CItemsContainer RequiredContainer, CItem Item, int Price, int WorldID)
	{
		m_RequiredItem = std::move(RequiredContainer);
		m_Item = std::move(Item);
		m_Price = Price;
		m_WorldID = WorldID;
	}

	CraftIdentifier GetID() const { return m_ID; }
	CItem* GetItem() { return &m_Item; }
	const CItem* GetItem() const { return &m_Item; }
	int GetPrice(class CPlayer * pPlayer) const;
	int GetWorldID() const { return m_WorldID; }
};


#endif