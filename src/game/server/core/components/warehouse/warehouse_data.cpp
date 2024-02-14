/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "warehouse_data.h"

CTradeSlot* CWarehouse::GetTradeSlot(TradeIdentifier ID)
{
	auto iter = std::find_if(m_aTradingList.begin(), m_aTradingList.end(), [ID](const CTradeSlot& Trade) { return Trade.GetID() == ID; });
	return iter != m_aTradingList.end() ? &(*iter) : nullptr;
}
