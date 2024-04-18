/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "craft_data.h"

#include <game/server/player.h>

int CCraftItem::GetPrice(CPlayer* pPlayer) const
{
	int Discount = 0;

	if(pPlayer)
	{
		Discount = translate_to_percent_rest(m_Price, pPlayer->GetSkill(SkillCraftDiscount)->GetLevel());
		if(pPlayer->GetItem(itTicketDiscountCraft)->IsEquipped())
			Discount += translate_to_percent_rest(m_Price, 20);
	}

	return maximum(m_Price - Discount, 0);
}