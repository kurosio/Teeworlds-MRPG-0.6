/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "path_navigator.h"

#include <game/server/gamecontext.h>

#include <game/server/mmocore/Components/Worlds/WorldManager.h>

#include "game/server/mmocore/PathFinder.h"

CEntityPathNavigator::CEntityPathNavigator(CGameWorld* pGameWorld, CEntity* pParent, vec2 EndPos, int WorldID, int64 Mask)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_DROPBONUS, {})
{
	vec2 PosTo { 0, 0 };
	GS()->Mmo()->WorldSwap()->FindPosition(WorldID, EndPos, &PosTo);

	m_Mask = Mask;
	m_PosTo = PosTo;
	m_pParent = pParent;
	GameWorld()->InsertEntity(this);

	SetNavigatorPositions(m_PosTo);
}

void CEntityPathNavigator::SetNavigatorPositions(vec2 StartPos)
{
	m_StepPos = 0;
	m_StartSnake = true;
	m_StartPos = StartPos;
	GS()->PathFinder()->SyncHandler()->Prepare<CPathFinderPrepared::TYPE::DEFAULT>(&m_Data, m_StartPos, m_PosTo);
}

void CEntityPathNavigator::Tick()
{
	if(!m_pParent)
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	// start prepare navigator data
	if(!m_StartSnake && m_TickLastIdle < Server()->Tick())
	{
		m_StartPos = m_pParent->GetPos();
		SetNavigatorPositions(m_StartPos);
		return;
	}

	// get prepared data and snapping 
	m_Snapping = distance(m_StartPos, m_PosTo) > 240.0f;
	if(m_Snapping && GS()->PathFinder()->SyncHandler()->TryGetPreparedData(&m_Data))
		m_Pos = m_Data.Get().m_Points[m_StepPos];

	// update timer by steps
	if(Server()->Tick() % (Server()->TickSpeed() / 15) == 0)
	{
		m_StepPos++;
		
		if(m_StepPos >= m_Data.Get().m_Size)
		{
			m_StartSnake = false;
			return;
		}

		m_Pos = m_Data.Get().m_Points[m_StepPos];
		GS()->CreateDamage(vec2(m_Pos.x -32.f, m_Pos.y - 64.f), -1, 1, 0);
	}

	// clipped
	if(distance(m_pParent->GetPos(), m_Pos) > 800.0f || distance(m_pParent->GetPos(), m_StartPos) > 400.f)
	{
		m_StartSnake = false;
	}
}

void CEntityPathNavigator::Snap(int SnappingClient)
{

}

void CEntityPathNavigator::PostSnap()
{
	// update last position
	if(m_pParent)
	{
		if(m_LastPos != m_pParent->GetPos())
		{
			m_TickLastIdle = Server()->Tick() + Server()->TickSpeed();
		}
		m_LastPos = m_pParent->GetPos();
	}
}
