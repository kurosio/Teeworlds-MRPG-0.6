/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "dir_navigator.h"

#include <game/server/core/components/worlds/world_manager.h>
#include <game/server/gamecontext.h>

CEntityDirectionNavigator::CEntityDirectionNavigator(CGameWorld* pGameWorld)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_PATH_FINDER, {}, 0, -1)
{
	GameWorld()->InsertEntity(this);
}

bool CEntityDirectionNavigator::TryStart(int ClientID, vec2 Position, int WorldID)
{
	m_ClientID = ClientID;
	m_pPlayer = GS()->GetPlayer(m_ClientID, true, true);

	auto PosTo = GS()->Core()->WorldManager()->FindPosition(WorldID, Position);
	if(!PosTo.has_value() || !m_pPlayer)
		return false;

	m_PosTo = PosTo.value();
	return true;
}

void CEntityDirectionNavigator::Snap(int SnappingClient)
{
	if(m_ClientID != SnappingClient || !m_pPlayer || !m_pPlayer->GetCharacter())
		return;

	m_Pos = m_pPlayer->GetCharacter()->m_Core.m_Pos;

	const auto FinalPos = m_Pos - normalize(m_Pos - m_PosTo) * clamp(distance(m_Pos, m_PosTo), 32.0f, 90.0f);
	GS()->SnapPickup(SnappingClient, GetID(), FinalPos);
}