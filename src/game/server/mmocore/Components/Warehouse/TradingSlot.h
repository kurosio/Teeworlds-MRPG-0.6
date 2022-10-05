/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_TRADING_SLOT_DATA_H
#define GAME_SERVER_COMPONENT_TRADING_SLOT_DATA_H

#include <game/server/mmocore/Components/Inventory/ItemData.h>

class CTradingSlot
{
	int m_ID{};
	CItem m_Item{};

	int m_RequiredItemID{};
	int m_Price{};

public:
	CTradingSlot() = default;
	CTradingSlot(int ID) :  m_ID(ID) {}

	void Init(CItem Item, int RequiredItemID, int Price)
	{
		m_Item = std::move(Item);
		m_RequiredItemID = RequiredItemID;
		m_Price = Price;
	}

	int GetID() const { return m_ID; }
	CItem* GetItem() { return &m_Item; }
	const CItem* GetItem() const { return &m_Item; }
	CItemDescription* GetCurrency() { return &CItemDescription::Data()[m_RequiredItemID]; }
	const CItemDescription* GetCurrency() const { return &CItemDescription::Data()[m_RequiredItemID]; }
	int GetPrice() const { return m_Price; }
};

#endif