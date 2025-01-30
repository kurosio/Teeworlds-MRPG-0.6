#include "rifle_trackedplazma.h"

#include <game/server/gamecontext.h>

CEntityRifleTrackedPlazma::CEntityRifleTrackedPlazma(CGameWorld* pGameWorld, int OwnerCID, vec2 Pos, vec2 Direction)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_LASER, Pos, 24)
{
	m_ClientID = OwnerCID;
	m_Direction = Direction;
	m_TrackedCID = -1;
	m_Speed.Init(5.f, 20.f, Server()->TickSpeed(), [](float t)
	{
		return 5.f + t * (20.f - 5.0f);
	});
	m_Speed.Start(Server()->Tick());

	GameWorld()->InsertEntity(this);
}

void CEntityRifleTrackedPlazma::Tick()
{
	const auto* pOwner = GetOwner();
	if(!pOwner || !pOwner->GetCharacter())
	{
		Explode();
		return;
	}

	// collision detection
	if(GS()->Collision()->CheckPoint(m_Pos.x, m_Pos.y))
	{
		Explode();
		return;
	}

	// moving & search potential target
	m_Pos += m_Direction * m_Speed.GetCurrentValue(Server()->Tick());

	// search potential target
	const auto* pTarget = GS()->GetPlayerChar(m_TrackedCID);
	if(!pTarget)
	{
		SearchPotentialTarget();
		return;
	}

	// tracking
	float Dist = distance(m_Pos, pTarget->m_Core.m_Pos);
	if(Dist < 24.0f)
	{
		Explode();
		return;
	}

	m_Direction = normalize(pTarget->m_Core.m_Pos - m_Pos);
}

void CEntityRifleTrackedPlazma::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	GS()->SnapLaser(SnappingClient, GetID(), m_Pos, m_Pos, Server()->Tick());
}

void CEntityRifleTrackedPlazma::Explode()
{
	GS()->CreateCyrcleExplosion(2, 32.f, m_Pos, m_ClientID, WEAPON_LASER, 1);
	GameWorld()->DestroyEntity(this);
}

void CEntityRifleTrackedPlazma::SearchPotentialTarget()
{
	// search min distance player
	int MinDistanceCID = -1;
	float MinDistance = 2400.0f;
	for(auto* pChar = (CCharacter*)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
	{
		if(m_ClientID == pChar->GetPlayer()->GetCID() || !pChar->IsAllowedPVP(m_ClientID))
			continue;

		if(GS()->Collision()->IntersectLineWithInvisible(m_Pos, pChar->m_Core.m_Pos, 0, 0))
			continue;

		float Dist = distance(pChar->m_Core.m_Pos, m_Pos);
		if(Dist < MinDistance)
		{
			MinDistance = Dist;
			MinDistanceCID = pChar->GetPlayer()->GetCID();
		}
	}

	// update tracked player
	if(MinDistanceCID > -1)
		m_TrackedCID = MinDistanceCID;
}
