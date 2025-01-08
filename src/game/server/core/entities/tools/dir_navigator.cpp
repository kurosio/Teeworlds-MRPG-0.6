/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "dir_navigator.h"

#include <game/server/core/components/worlds/world_manager.h>
#include <game/server/gamecontext.h>

#include <game/server/core/entities/tools/path_navigator.h>

CPlayer* CEntityDirectionNavigator::GetPlayer() const
{
	return GS()->GetPlayer(m_ClientID);
}

CEntityDirectionNavigator::CEntityDirectionNavigator(CGameWorld* pGameWorld, int ClientID, vec2 Position, int WorldID)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_PATH_FINDER, Position, 0, ClientID)
{
	const auto PosTo = GS()->Core()->WorldManager()->FindPosition(WorldID, Position);
	if(!PosTo.has_value())
	{
		MarkForDestroy();
		return;
	}

	m_Pos = Position;
	m_PosTo = PosTo.value();
	m_pEntNavigator = nullptr;
	GameWorld()->InsertEntity(this);

	// quest navigator finder
	auto* pPlayer = GetPlayer();
	if(pPlayer && pPlayer->GetItem(itShowQuestStarNavigator)->IsEquipped())
		m_pEntNavigator = new CEntityPathNavigator(&GS()->m_World, m_ClientID, true, Position, WorldID, false, CmaskOne(ClientID));
}

CEntityDirectionNavigator::~CEntityDirectionNavigator()
{
	if(m_pEntNavigator)
	{
		delete m_pEntNavigator;
		m_pEntNavigator = nullptr;
	}
}

void CEntityDirectionNavigator::Tick()
{
	auto* pPlayer = GetPlayer();
	if(!pPlayer)
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	if(!pPlayer->GetCharacter())
		return;

	m_Pos = pPlayer->GetCharacter()->m_Core.m_Pos;
}

void CEntityDirectionNavigator::Snap(int SnappingClient)
{
	if(m_ClientID != SnappingClient)
		return;

	const auto FinalPos = m_Pos - normalize(m_Pos - m_PosTo) * clamp(distance(m_Pos, m_PosTo), 32.0f, 90.0f);
	GS()->SnapPickup(SnappingClient, GetID(), FinalPos);
}