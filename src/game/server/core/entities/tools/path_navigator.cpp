/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "path_navigator.h"
#include <game/server/gamecontext.h>

#include <game/server/core/components/worlds/world_manager.h>
#include <game/server/core/tools/path_finder.h>

CEntityPathNavigator::CEntityPathNavigator(CGameWorld* pGameWorld, CEntity* pParent, bool StartByCreating, vec2 FromPos, vec2 SearchPos, int WorldID, bool Projectile, int64_t Mask)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_PATH_NAVIGATOR, FromPos)
{
	GS()->Core()->WorldManager()->FindPosition(WorldID, SearchPos, &m_PosTo);

	m_Mask = Mask;
	m_StepPos = 0;
	m_pParent = pParent;
	m_Projectile = Projectile;
	m_StartByCreating = StartByCreating;
	GameWorld()->InsertEntity(this);
}

void CEntityPathNavigator::Tick()
{
	// destroy the entity if its parent no longer exists
	if(!GameWorld()->ExistEntity(m_pParent))
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	// if no path is available, request a new path
	if(m_PathHandle.vPath.empty())
	{
		if(m_TickLastIdle < Server()->Tick() && distance(m_pParent->GetPos(), m_PosTo) > 240.f)
		{
			GS()->PathFinder()->RequestPath(m_PathHandle, m_pParent->GetPos(), m_PosTo);
			m_StepPos = 0;
		}
		m_PathHandle.TryGetPath();
		return;
	}

	// perform movement if the countdown has expired
	if(m_TickCountDown < Server()->Tick())
		Move();
}

void CEntityPathNavigator::TickDeferred()
{
	// update last position
	if(m_LastPos != m_pParent->GetPos() && m_TickLastIdle != -1 && !m_StartByCreating)
	{
		m_TickLastIdle = Server()->Tick() + (Server()->TickSpeed() * 3);
	}
	m_LastPos = m_pParent->GetPos();
}


void CEntityPathNavigator::Move()
{
	// move by projectile
	if(m_Projectile)
	{
		if(is_negative_vec(m_Pos))
		{
			m_Pos = m_PathHandle.vPath[m_StepPos];
			m_StepPos++;
		}

		// smooth movement
		if(m_StepPos < (int)m_PathHandle.vPath.size())
		{
			m_Pos += normalize(m_PathHandle.vPath[m_StepPos] - m_Pos) * 4.f;
		}

		// update timer by steps
		if(Server()->Tick() % (Server()->TickSpeed() / 8) == 0 && m_StepPos < (int)m_PathHandle.vPath.size())
		{
			m_Pos = m_PathHandle.vPath[m_StepPos];
			m_StepPos++;
		}

		return;
	}

	// update timer by steps
	if(Server()->Tick() % (Server()->TickSpeed() / 10) == 0)
	{
		m_Pos = m_PathHandle.vPath[m_StepPos];

		// create damage effect
		constexpr vec2 Corrector(32.f, 64.f);
		GS()->CreateDamage(m_Pos - Corrector, -1, 1, false, 0.f, m_Mask);
		m_StepPos++;

		// check if the entity should stop moving
		if(distance(m_pParent->GetPos(), m_Pos) > 800.0f || m_StepPos >= (int)m_PathHandle.vPath.size())
		{
			m_TickCountDown = Server()->Tick() + (Server()->TickSpeed() * 2);
			m_StartByCreating = false;
			m_PathHandle.vPath.clear();
		}
	}
}

void CEntityPathNavigator::Snap(int SnappingClient)
{
	if(!m_Projectile || m_PathHandle.vPath.empty() || !CmaskIsSet(m_Mask, SnappingClient) || NetworkClipped(SnappingClient))
		return;

	CNetObj_Projectile* pObj = static_cast<CNetObj_Projectile*>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, GetID(), sizeof(CNetObj_Projectile)));
	if(pObj)
	{
		pObj->m_X = (int)m_Pos.x;
		pObj->m_Y = (int)m_Pos.y;
		pObj->m_VelX = 0;
		pObj->m_VelY = 0;
		pObj->m_StartTick = Server()->Tick();
		pObj->m_Type = WEAPON_HAMMER;
	}
}