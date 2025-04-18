/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "random_box_data.h"

#include <game/server/gamecontext.h>
#include "random_box_handler.h"

bool CRandomBox::Start(CPlayer* pPlayer, int Ticks, CPlayerItem* pPlayerUsesItem, int Value)
{
	if(!pPlayer || !pPlayer->IsAuthed())
		return false;

	if(!pPlayerUsesItem || !pPlayerUsesItem->HasItem())
		return false;

	if(pPlayer->m_aPlayerTick[LastRandomBox] > pPlayer->GS()->Server()->Tick())
	{
		pPlayer->GS()->Broadcast(pPlayer->GetCID(), BroadcastPriority::MainInformation, 100, "Wait until the last random box opens!");
		return false;
	}

	// clamping value (maximal is 100)
	Value = minimum(100, Value);

	// start random box event
	if(pPlayerUsesItem->Remove(Value))
	{
		pPlayer->GS()->Chat(pPlayer->GetCID(), "You used '{} x{}'.", pPlayerUsesItem->Info()->GetName(), Value);
		pPlayer->m_aPlayerTick[LastRandomBox] = pPlayer->GS()->Server()->Tick() + Ticks;
		new CEntityRandomBoxRandomizer(&pPlayer->GS()->m_World, pPlayer->Account()->GetID(), Ticks, m_vItems, pPlayerUsesItem, Value);
		return true;
	}

	return true;
}