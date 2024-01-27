/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildHousePlantzoneData.h"

#include "GuildHousePlantzonesManager.h"
#include <game/server/core/entities/items/jobitems.h>

void CGuildHousePlantzoneData::ChangeItem(int ItemID) const
{
	for(auto& pPlant : m_vPlants)
	{
		pPlant->m_ItemID = ItemID;
	}
	m_pManager->Save();
}
