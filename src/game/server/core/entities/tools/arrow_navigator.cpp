/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "arrow_navigator.h"

#include <game/server/core/components/worlds/world_manager.h>
#include <game/server/gamecontext.h>

#include <game/server/core/entities/tools/path_navigator.h>

CEntityArrowNavigator::CEntityArrowNavigator(CGameWorld* pGameWorld, int ClientID, vec2 Position, int WorldID)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_PATH_FINDER, Position, 0, ClientID)
{
	GS()->Core()->WorldManager()->FindPosition(WorldID, m_Pos, &m_PosTo);
	m_pPlayer = GS()->GetPlayer(m_ClientID, true, true);
	GameWorld()->InsertEntity(this);

	// quest navigator finder
	if(m_pPlayer && m_pPlayer->GetItem(itShowQuestNavigator)->IsEquipped())
	{
		new CEntityPathNavigator(&GS()->m_World, this, true, m_pPlayer->m_ViewPos, m_Pos, WorldID, false, CmaskOne(ClientID));
	}
}

void CEntityArrowNavigator::Tick()
{
	if(is_negative_vec(m_PosTo))
		GameWorld()->DestroyEntity(this);
}

void CEntityArrowNavigator::Snap(int SnappingClient)
{
	if(m_ClientID != SnappingClient || !m_pPlayer || !m_pPlayer->GetCharacter())
		return;

	CNetObj_Pickup* pPickup = static_cast<CNetObj_Pickup*>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, GetID(), sizeof(CNetObj_Pickup)));
	if(pPickup)
	{
		m_Pos = m_pPlayer->GetCharacter()->m_Core.m_Pos;
		vec2 Pos = m_Pos - normalize(m_Pos - m_PosTo) * clamp(distance(m_Pos, m_PosTo), 32.0f, 90.0f);

		pPickup->m_X = (int)Pos.x;
		pPickup->m_Y = (int)Pos.y;
		pPickup->m_Type = (int)POWERUP_HEALTH;
		pPickup->m_Subtype = 0;
	}
}