/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "base.h"

#include <game/server/gamecontext.h>

CEidolon::CEidolon(CGameWorld *pGameWorld, vec2 Pos, int Type, int OwnerID)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_DROPBONUS, Pos)
{
	m_Pos = Pos;
	m_Type = Type;
	m_OwnerID = OwnerID;
	GameWorld()->InsertEntity(this);

	for(int i = 0; i < NUM_PARTICLES_AROUND_EIDOLON; i++)
	{
		m_IDs[i] = Server()->SnapNewID();
	}
}

CEidolon::~CEidolon()
{
	for(int i = 0; i < NUM_PARTICLES_AROUND_EIDOLON; i++)
	{
		Server()->SnapFreeID(m_IDs[i]);
	}
}

void CEidolon::Tick()
{
	CPlayer* pPlayer = GS()->m_apPlayers[m_OwnerID];
	if(!pPlayer || !pPlayer->GetCharacter() || !pPlayer->GetEquippedItemID(ItemFunctional::EQUIP_EIDOLON))
	{
		Destroy();
		return;
	}

	vec2 MoveTo = pPlayer->GetCharacter()->GetPos() + m_MoveTo;

	constexpr int Tired = 5;
	for(int i = 0; i < Tired; i++)
	{
		if(distance(MoveTo, m_Pos) < 10.0f || GS()->Collision()->CheckPoint(MoveTo) || GS()->Collision()->IntersectLine(MoveTo, pPlayer->GetCharacter()->GetPos(), nullptr, nullptr))
		{
			m_MoveTo.x = frandom_num(-96, 96);
			m_MoveTo.y = frandom_num(-96, 96);
			m_TickMove = Server()->Tick() + random_int() % Server()->TickSpeed();
			MoveTo = pPlayer->GetCharacter()->GetPos() + m_MoveTo;
		}
	}

	if(m_TickMove < Server()->Tick())
	{
		if(MoveTo.x > m_Pos.x)
			m_MovePos.x += 1;
		else if(MoveTo.x < m_Pos.x)
			m_MovePos.x -= 1;
		if(MoveTo.y > m_Pos.y)
			m_MovePos.y += 1;
		else if(MoveTo.y < m_Pos.y)
			m_MovePos.y -= 1;
	}

	m_Pos = pPlayer->GetCharacter()->GetPos() + m_MovePos;
}

void CEidolon::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	{
		CNetObj_Projectile* pObj = static_cast<CNetObj_Projectile*>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, GetID(), sizeof(CNetObj_Projectile)));
		if(!pObj)
			return;

		pObj->m_Type = WEAPON_SHOTGUN;
		pObj->m_X = (int)m_Pos.x;
		pObj->m_Y = (int)m_Pos.y;
		pObj->m_VelX = 0;
		pObj->m_VelY = 0;
		pObj->m_StartTick = Server()->Tick();
	}

	for(int i = 0; i < NUM_PARTICLES_AROUND_EIDOLON; i++)
	{
		CNetObj_Projectile* pObj = static_cast<CNetObj_Projectile*>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_IDs[i], sizeof(CNetObj_Projectile)));
		if(!pObj)
			return;

		pObj->m_Type = WEAPON_HAMMER;
		pObj->m_X = m_Pos.x + cos(Server()->Tick() - pi / NUM_PARTICLES_AROUND_EIDOLON * i) * 18.f;
		pObj->m_Y = m_Pos.y + sin(Server()->Tick() - pi / NUM_PARTICLES_AROUND_EIDOLON * i) * 18.f;
		pObj->m_StartTick = Server()->Tick();
	}
}
