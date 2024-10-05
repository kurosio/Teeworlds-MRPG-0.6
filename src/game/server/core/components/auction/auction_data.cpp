/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "auction_data.h"

int CAuctionSlot::GetTaxPrice() const
{
	return maximum(1, translate_to_percent_rest(m_Price, g_Config.m_SvAuctionSlotTaxRate));
}