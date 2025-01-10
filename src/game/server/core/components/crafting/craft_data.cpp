#include "craft_data.h"

#include <game/server/player.h>

int CCraftItem::GetPrice(CPlayer* pPlayer) const
{
	int Discount = 0;

	if(pPlayer)
	{
		// passive skill discount
		int skillLevel = pPlayer->GetSkill(SKILL_CRAFT_DISCOUNT)->GetLevel();
		Discount += translate_to_percent_rest(m_Price, skillLevel);
	}

	return maximum(m_Price - Discount, 0);
}