/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_CRAFT_DATA_H
#define GAME_SERVER_COMPONENT_CRAFT_DATA_H

#include "game/server/mmocore/Components/Inventory/ItemData.h"

using CraftIdentifier = int;

class CCraftData : public MultiworldIdentifiableStaticData< std::map< int, CCraftData > >
{
public:
	using ContainerRequiredCraftItems = std::deque < CItem >;

private:
	CraftIdentifier m_ID{};
	CItem m_Item{};
	int m_Price{};
	int m_WorldID{};

public:
	ContainerRequiredCraftItems m_RequiredItem;

	CCraftData() = default;
	CCraftData(CraftIdentifier ID) :  m_ID(ID) {}

	void Init(ContainerRequiredCraftItems RequiredContainer, CItem Item, int Price, int WorldID)
	{
		m_RequiredItem = std::move(RequiredContainer);
		m_Item = std::move(Item);
		m_Price = Price;
		m_WorldID = WorldID;
		CCraftData::Data()[m_ID] = *this;
	}

	CraftIdentifier GetID() const { return m_ID; }
	CItem* GetItem() { return &m_Item; }
	const CItem* GetItem() const { return &m_Item; }
	int GetPrice() const { return m_Price; }
	int GetWorldID() const { return m_WorldID; }
};


#endif