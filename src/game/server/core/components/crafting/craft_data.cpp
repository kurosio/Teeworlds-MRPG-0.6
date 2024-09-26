#include "craft_data.h"

#include <game/server/player.h>

intbig CCraftItem::GetPrice(CPlayer* pPlayer) const
{
	intbig Discount = 0;

	if(pPlayer)
	{
		// passive skill discount
		int skillLevel = pPlayer->GetSkill(SkillCraftDiscount)->GetLevel();
		Discount += translate_to_percent_rest(m_Price, skillLevel);

		// discount from a special item
		auto* pDiscountTicket = pPlayer->GetItem(itTicketDiscountCraft);
		if(pDiscountTicket && pDiscountTicket->IsEquipped())
		{
			Discount += translate_to_percent_rest(m_Price, 20);
		}
	}

	return maximum(m_Price - Discount, (intbig)0);
}