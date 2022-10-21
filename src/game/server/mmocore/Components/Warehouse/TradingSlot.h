/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_TRADING_SLOT_DATA_H
#define GAME_SERVER_COMPONENT_TRADING_SLOT_DATA_H

#include <game/server/mmocore/Components/Inventory/ItemData.h>

using TradeIdentifier = int;

class CTradingSlot
{
	TradeIdentifier m_ID{};
	std::shared_ptr<CItem> m_pItem{};
	CItemDescription* m_pRequiredItem{};
	int m_Price{};

public:
	CTradingSlot() = default;
	CTradingSlot(TradeIdentifier ID) :  m_ID(ID) {}

	void Init(std::shared_ptr <CItem> pItem, CItemDescription* pRequiredItem, int Price)
	{
		m_pItem = std::move(pItem);
		m_pRequiredItem = pRequiredItem;
		m_Price = Price;
	}

	TradeIdentifier GetID() const { return m_ID; }

	CItem* GetItem() { return m_pItem.get(); }
	const CItem* GetItem() const { return m_pItem.get(); }
	CItemDescription* GetCurrency() { return m_pRequiredItem; }
	const CItemDescription* GetCurrency() const { return m_pRequiredItem; }
	int GetPrice() const { return m_Price; }
};

#endif