/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "path_navigator.h"

#include <game/server/gamecontext.h>

#include <game/server/core/components/Worlds/WorldManager.h>

#include "game/server/core/utilities/pathfinder.h"

CEntityPathNavigator::CEntityPathNavigator(CGameWorld* pGameWorld, CEntity* pParent, bool StartByCreating, vec2 FromPos, vec2 SearchPos, int WorldID, bool Projectile, int64_t Mask)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_DROPBONUS, FromPos)
{
	vec2 PosTo { 0, 0 };
	GS()->Core()->WorldManager()->FindPosition(WorldID, SearchPos, &PosTo);

	m_Mask = Mask;
	m_PosTo = PosTo;
	m_StepPos = 0;
	m_pParent = pParent;
	m_Projectile = Projectile;
	m_StartByCreating = StartByCreating;
	GameWorld()->InsertEntity(this);
}

bool CEntityPathNavigator::PreparedPathData()
{
	// Check if enough time has passed since the last idle tick and if the distance between the parent's position and the target position is greater than 240 units
	if(m_Data.IsRequiredPrepare() && m_TickLastIdle < Server()->Tick() && distance(m_pParent->GetPos(), m_PosTo) > 240.f)
	{
		// Prepare the data for path finding using the default method in the path finder handle
		GS()->PathFinder()->Handle()->Prepare<CPathFinderPrepared::DEFAULT>(&m_Data, m_pParent->GetPos(), m_PosTo);
		m_StepPos = 0;
	}

	// Check if the path finder has prepared data available
	if(!GS()->PathFinder()->Handle()->TryGetPreparedData(&m_Data))
		return false;

	return true;
}

void CEntityPathNavigator::Tick()
{
	// Check if the entity's parent is null or marked for destruction
	if(!m_pParent || m_pParent->IsMarkedForDestroy())
	{
		// Destroy the entity and return
		GameWorld()->DestroyEntity(this);
		return;
	}

	// Check if the countdown has reached its limit and if the path data is prepared
	if(m_TickCountDown < Server()->Tick() && PreparedPathData())
	{
		// Move the object
		Move();

	}
}

void CEntityPathNavigator::Move()
{
	// move by projectile
	if(m_Projectile)
	{
		if(is_negative_vec(m_Pos))
		{
			m_Pos = m_Data.Get().m_Points[m_StepPos];
			m_StepPos++;
		}

		// smooth movement
		m_Pos += normalize(m_Data.Get().m_Points[m_StepPos] - m_Pos) * 4.f;

		// update timer by steps
		if(Server()->Tick() % (Server()->TickSpeed() / 8) == 0)
		{
			m_Pos = m_Data.Get().m_Points[m_StepPos];
			m_StepPos++;
		}

		return;
	}

	// default move
	m_Pos = m_Data.Get().m_Points[m_StepPos];

	// update timer by steps
	if(Server()->Tick() % (Server()->TickSpeed() / 10) == 0)
	{
		GS()->CreateDamage(vec2(m_Pos.x - 32.f, m_Pos.y - 64.f), -1, 1, false, 0.f, m_Mask);
		m_StepPos++;
	}

	// Check if the distance between the parent's position and the object's position is greater than 800.0f
	// or if the step position is greater than or equal to the number of points in the path data
	if(distance(m_pParent->GetPos(), m_Pos) > 800.0f || m_StepPos >= (int)m_Data.Get().m_Points.size())
	{
		// Set the countdown to the current tick plus 2 seconds
		m_TickCountDown = Server()->Tick() + (Server()->TickSpeed() * 2);
		m_Data.Get().Clear();
		m_StartByCreating = false;
	}
}

void CEntityPathNavigator::Snap(int SnappingClient)
{
	if(!m_Projectile || m_Data.Get().Empty() || !CmaskIsSet(m_Mask, SnappingClient) || NetworkClipped(SnappingClient))
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

void CEntityPathNavigator::PostSnap()
{
	// update last position
	if(m_pParent)
	{
		if(m_LastPos != m_pParent->GetPos() && m_TickLastIdle != -1 && !m_StartByCreating)
		{
			m_TickLastIdle = Server()->Tick() + (Server()->TickSpeed() * 3);
		}
		m_LastPos = m_pParent->GetPos();
	}
}

