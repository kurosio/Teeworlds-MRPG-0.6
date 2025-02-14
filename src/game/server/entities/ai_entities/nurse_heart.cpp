#include "nurse_heart.h"
#include <game/server/gamecontext.h>

CEntityNurseHeart::CEntityNurseHeart(CGameWorld* pGameWorld, int ClientID)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_TOOLS, {}, 0.0f, ClientID)
{
	dbg_assert(ClientID >= MAX_PLAYERS, "CNurseHeart only for bot's");

	m_ClientID = ClientID;
	GameWorld()->InsertEntity(this);
}

void CEntityNurseHeart::Tick()
{
	auto* pPlayerBot = dynamic_cast<CPlayerBot*>(GS()->GetPlayer(m_ClientID));
	if(!pPlayerBot || !pPlayerBot->GetCharacter())
	{
		MarkForDestroy();
		return;
	}

	if(!pPlayerBot->IsActive())
		return;

	float Radius = 24.f;
	float Angle = angle(normalize(vec2(pPlayerBot->GetCharacter()->m_Core.m_Input.m_TargetX, pPlayerBot->GetCharacter()->m_Core.m_Input.m_TargetY)));
	m_Pos = pPlayerBot->GetCharacter()->GetPos() - vec2(Radius * cos(Angle + pi), Radius * sin(Angle + pi));
}

void CEntityNurseHeart::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	GS()->SnapPickup(SnappingClient, GetID(), m_Pos, 0, 0);
}
