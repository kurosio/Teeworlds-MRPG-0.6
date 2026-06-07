/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "random_box_data.h"

#include <game/server/gamecontext.h>
#include "random_box_handler.h"

bool CBox::Give(CPlayer* pPlayer, CPlayerItem* pPlayerUsesItem, int UseValue) const
{
	if(!pPlayer || !pPlayer->IsAuthed())
		return false;

	if(!pPlayerUsesItem || !pPlayerUsesItem->HasItem())
		return false;

	if(IsEmpty())
		return false;

	UseValue = clamp(UseValue, 1, 100);
	if(!pPlayerUsesItem->Remove(UseValue))
		return false;

	pPlayer->GS()->Chat(pPlayer->GetCID(), "You opened '{} x{}'.", pPlayerUsesItem->Info()->GetName(), UseValue);
	for(const auto& Item : m_vItems)
	{
		const int TotalValue = Item.Value * UseValue;
		auto* pRewardItem = pPlayer->GetItem(Item.ItemID);
		pRewardItem->Add(TotalValue, 0, 0, 0, false);
	}

	return true;
}

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