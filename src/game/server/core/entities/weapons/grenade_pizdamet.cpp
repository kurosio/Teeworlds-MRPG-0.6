#include "grenade_pizdamet.h"

#include <game/server/gamecontext.h>

CEntityGrenadePizdamet::CEntityGrenadePizdamet(CGameWorld *pGameWorld, int OwnerCID, vec2 Pos, vec2 Direction)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_PROJECTILE, Pos, 24)
{
	m_ClientID = OwnerCID;
	m_Direction = vec2(centrelized_frandom(Direction.x, 0.3f), centrelized_frandom(Direction.y, 0.3f));

	GameWorld()->InsertEntity(this);
}

void CEntityGrenadePizdamet::Tick()
{
	// check valid owner
	const auto* pOwner = GetOwner();
	if(!pOwner || !pOwner->GetCharacter())
	{
		MarkForDestroy();
		return;
	}

	// update position
	m_Pos += normalize(m_Direction) * 18.0f;

	// search target chat
	CCharacter* pTargetChar = nullptr;
	for(auto* pChar = (CCharacter*)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
	{
		if(m_ClientID == pChar->GetPlayer()->GetCID() || !pChar->IsAllowedPVP(m_ClientID))
			continue;

		// check distance
		const float Distance = distance(m_Pos, pChar->m_Core.m_Pos);
		if(Distance < 64.f)
		{
			pTargetChar = pChar;
			break;
		}

		// smooth navigate to target
		if(Distance < 300.0f)
		{
			vec2 ToEnemy = normalize(pChar->m_Core.m_Pos - m_Pos);
			m_Direction = normalize(m_Direction + ToEnemy * 0.05f);
		}
	}

	// destroy and explosion
	if(pTargetChar != nullptr || GS()->Collision()->CheckPoint(m_Pos))
	{
		const auto totalDmg = pOwner->GetCharacter()->GetTotalDamageByWeapon(WEAPON_GRENADE);
		GS()->CreateRandomRadiusExplosion(2, 96.f, m_Pos, m_ClientID, WEAPON_GRENADE, totalDmg);
		MarkForDestroy();
	}
}

void CEntityGrenadePizdamet::Snap(int SnappingClient)
{
	if(!NetworkClipped(SnappingClient))
	{
		GS()->SnapPickup(SnappingClient, GetID(), m_Pos, POWERUP_HEALTH, 0);
	}
}