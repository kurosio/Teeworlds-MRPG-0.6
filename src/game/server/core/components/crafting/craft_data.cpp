#include "craft_data.h"

#include <game/server/player.h>

int CCraftItem::GetPrice(CPlayer* pPlayer) const
{
	int Discount = 0;

	if(pPlayer)
	{
		// passive skill discount
		int DiscountPct = pPlayer->GetSkill(SKILL_CRAFT_DISCOUNT)->GetMod(SkillMod::BonusIncreasePct);
		if(DiscountPct > 0)
			Discount += translate_to_percent_rest(m_Price, (float)DiscountPct);
	}

	return maximum(m_Price - Discount, 0);
}