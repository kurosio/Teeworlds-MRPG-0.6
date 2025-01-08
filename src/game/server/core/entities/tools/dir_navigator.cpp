/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "dir_navigator.h"

#include <game/server/core/components/worlds/world_manager.h>
#include <game/server/gamecontext.h>

#include <game/server/core/entities/tools/path_navigator.h>

CEntityDirectionNavigator::CEntityDirectionNavigator(CGameWorld* pGameWorld, int ClientID, vec2 Position, int WorldID)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_PATH_FINDER, Position, 0, ClientID)
{
	const auto PosTo = GS()->Core()->WorldManager()->FindPosition(WorldID, Position);
	if(!PosTo.has_value())
	{
		MarkForDestroy();
		return;
	}

	m_PosTo = PosTo.value();
	m_pPlayer = GS()->GetPlayer(m_ClientID, true, true);
	GameWorld()->InsertEntity(this);

	// quest navigator finder
	if(m_pPlayer && m_pPlayer->GetItem(itShowQuestStarNavigator)->IsEquipped())
		new CEntityPathNavigator(&GS()->m_World, this, true, m_pPlayer->m_ViewPos, m_Pos, WorldID, false, CmaskOne(ClientID));
}

void CEntityDirectionNavigator::Snap(int SnappingClient)
{
	if(m_ClientID != SnappingClient || !m_pPlayer || !m_pPlayer->GetCharacter())
		return;

	m_Pos = m_pPlayer->GetCharacter()->m_Core.m_Pos;

	const auto FinalPos = m_Pos - normalize(m_Pos - m_PosTo) * clamp(distance(m_Pos, m_PosTo), 32.0f, 90.0f);
	GS()->SnapPickup(SnappingClient, GetID(), FinalPos);
}