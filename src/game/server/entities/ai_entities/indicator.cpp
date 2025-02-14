#include "indicator.h"

#include <game/server/gamecontext.h>

CEntityBotIndicator::CEntityBotIndicator(CGameWorld *pGameWorld, int ClientID, int Type, int SubType)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_VISUAL, {}, 14.f)
{
	m_Type = Type;
	m_SubType = SubType;
	m_ClientID = ClientID;
	GameWorld()->InsertEntity(this);
}

void CEntityBotIndicator::Tick()
{
	const auto* pPlayerBot = dynamic_cast<CPlayerBot*>(GS()->GetPlayer(m_ClientID));
	if(!pPlayerBot || !pPlayerBot->GetCharacter())
	{
		MarkForDestroy();
		return;
	}

	m_Pos = pPlayerBot->GetCharacter()->m_Core.m_Pos;
}

void CEntityBotIndicator::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	vec2 ShowPos = vec2(m_Pos.x, m_Pos.y - 64.f);
	GS()->SnapPickup(SnappingClient, GetID(), ShowPos, m_Type, m_SubType);
}
