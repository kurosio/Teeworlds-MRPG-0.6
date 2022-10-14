/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "CraftData.h"

#include <game/server/player.h>

int CCraftItem::GetPrice(CPlayer* pPlayer) const
{
	if(!pPlayer)
		return m_Price;

	int Discount = translate_to_percent_rest(m_Price, pPlayer->GetSkill(SkillCraftDiscount)->GetLevel());
	if(pPlayer->GetItem(itTicketDiscountCraft)->IsEquipped())
		Discount += translate_to_percent_rest(m_Price, 20);

	return max(m_Price - Discount, 0);
}
