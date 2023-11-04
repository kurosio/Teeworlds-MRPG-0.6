/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "RandomBoxData.h"

#include <game/server/gamecontext.h>
#include "RandomBoxHandler.h"

bool CRandomBox::Start(CPlayer* pPlayer, int Seconds, CPlayerItem* pPlayerUsesItem, int UseValue)
{
	// Check if player and item parameters are valid
	if(!pPlayer || !pPlayer->IsAuthed() || !pPlayerUsesItem || !pPlayerUsesItem->HasItem())
		return false;

	// Check if the last random box is still opening
	if(pPlayer->m_aPlayerTick[LastRandomBox] > pPlayer->GS()->Server()->Tick())
	{
		pPlayer->GS()->Broadcast(pPlayer->GetCID(), BroadcastPriority::MAIN_INFORMATION, 100, "Wait until the last random box opens!");
		return false;
	}

	// Clamp use value to a maximum of 100
	UseValue = min(100, UseValue);

	// Remove the specified amount of items and initialize the box
	if(pPlayerUsesItem->Remove(UseValue))
	{
		// Send a chat message to the player confirming the usage of the items
		pPlayer->GS()->Chat(pPlayer->GetCID(), "You used '{STR}x{VAL}'.", pPlayerUsesItem->Info()->GetName(), UseValue);

		// Convert the duration from seconds to game ticks
		Seconds *= pPlayer->GS()->Server()->TickSpeed();

		// Set the tick when the last random box will finish opening
		pPlayer->m_aPlayerTick[LastRandomBox] = pPlayer->GS()->Server()->Tick() + Seconds;

		// Sort the vector of random items by chance
		// Create a new instance of the random box randomizer entity
		std::sort(m_VectorItems.begin(), m_VectorItems.end(), [](const CRandomItem& pLeft, const CRandomItem& pRight) { return pLeft.m_Chance < pRight.m_Chance; });
		new CEntityRandomBoxRandomizer(&pPlayer->GS()->m_World, pPlayer, pPlayer->Acc().m_ID, Seconds, std::move(m_VectorItems), pPlayerUsesItem, UseValue);
	}

	return true;
}