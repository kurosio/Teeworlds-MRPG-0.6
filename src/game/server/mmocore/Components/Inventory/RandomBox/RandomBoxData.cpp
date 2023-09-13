/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "RandomBoxData.h"

#include <game/server/gamecontext.h>
#include "RandomBoxHandler.h"

bool CRandomBox::Start(CPlayer *pPlayer, int Seconds, CPlayerItem* pPlayerUsesItem, int UseValue)
{
	if(!pPlayer || !pPlayer->IsAuthed() || !pPlayerUsesItem || !pPlayerUsesItem->HasItem())
		return false;

	// check last random box
	if(pPlayer->m_aPlayerTick[LastRandomBox] > pPlayer->GS()->Server()->Tick())
	{
		pPlayer->GS()->Broadcast(pPlayer->GetCID(), BroadcastPriority::MAIN_INFORMATION, 100, "Wait until the last random box opens!");
		return false;
	}

	// clamp use value for maximum
	UseValue = min(100, UseValue);

	// remove item and init box
	if(pPlayerUsesItem->Remove(UseValue))
	{
		Seconds *= pPlayer->GS()->Server()->TickSpeed();
		pPlayer->m_aPlayerTick[LastRandomBox] = pPlayer->GS()->Server()->Tick() + Seconds;
		std::sort(m_VectorItems.begin(), m_VectorItems.end(), [](const StRandomItem& pLeft, const StRandomItem& pRight) { return pLeft.m_Chance < pRight.m_Chance; });

		new CEntityRandomBoxRandomizer(&pPlayer->GS()->m_World, pPlayer, pPlayer->Acc().m_UserID, Seconds, m_VectorItems, pPlayerUsesItem, UseValue);
		pPlayer->GS()->Chat(pPlayer->GetCID(), "You used '{STR}x{VAL}'.", pPlayerUsesItem->Info()->GetName(), UseValue);
	}

	return true;
};