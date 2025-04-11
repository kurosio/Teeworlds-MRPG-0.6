#include "dir_navigator.h"
#include "path_navigator.h"

#include <game/server/core/components/worlds/world_manager.h>
#include <game/server/gamecontext.h>

CEntityDirNavigator::CEntityDirNavigator(CGameWorld* pGameWorld, int Type, int Subtype, bool StarNavigator, int ClientID, float Clipped, vec2 Pos, int WorldID)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_DIR_NAVIGATOR, Pos, 0, ClientID)
{
	m_Type = Type;
	m_Subtype = Subtype;
	m_Clipped = Clipped;
	m_pEntNavigator = nullptr;
	if(const auto PosTo = GS()->Core()->WorldManager()->FindPosition(WorldID, Pos))
	{
		m_PosTo = *PosTo;

		// path navigator
		auto* pPlayer = GetOwner();
		if(pPlayer && pPlayer->GetItem(itShowQuestStarNavigator)->IsEquipped())
		{
			auto* pNavigator = new CEntityPathNavigator(&GS()->m_World, m_ClientID, true, Pos, WorldID, !StarNavigator, CmaskOne(ClientID));
			if(!pNavigator->IsMarkedForDestroy())
				m_pEntNavigator = pNavigator;
		}
	}
	else
	{
		MarkForDestroy();
	}

	GameWorld()->InsertEntity(this);
}


CEntityDirNavigator::~CEntityDirNavigator()
{
	if(m_pEntNavigator)
		m_pEntNavigator->MarkForDestroy();
}


void CEntityDirNavigator::Tick()
{
	auto* pPlayer = GetOwner();
	if(!pPlayer || !pPlayer->GetCharacter())
		return;

	m_Pos = pPlayer->GetCharacter()->m_Core.m_Pos;
}


void CEntityDirNavigator::Snap(int SnappingClient)
{
	if(m_ClientID != SnappingClient || !GetOwnerChar())
		return;

	if(m_Clipped > 1.f && distance(m_PosTo, m_Pos) < m_Clipped)
		return;

	const auto FinalPos = m_Pos - normalize(m_Pos - m_PosTo) * clamp(distance(m_Pos, m_PosTo), 32.0f, 90.0f);
	GS()->SnapPickup(SnappingClient, GetID(), FinalPos, m_Type, m_Subtype);
}