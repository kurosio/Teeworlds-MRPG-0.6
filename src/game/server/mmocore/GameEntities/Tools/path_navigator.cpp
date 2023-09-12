/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "path_navigator.h"

#include <game/server/gamecontext.h>

#include <game/server/mmocore/Components/Worlds/WorldManager.h>

#include "game/server/mmocore/PathFinder.h"

CEntityPathNavigator::CEntityPathNavigator(CGameWorld* pGameWorld, CEntity* pParent, vec2 StartPos, vec2 SearchPos, int WorldID, int64 Mask)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_DROPBONUS, StartPos)
{
	vec2 PosTo { 0, 0 };
	GS()->Mmo()->WorldSwap()->FindPosition(WorldID, SearchPos, &PosTo);

	m_First = true;
	m_Mask = Mask;
	m_PosTo = PosTo;
	m_StepPos = 0;
	m_pParent = pParent;
	GS()->PathFinder()->SyncHandler()->Prepare<CPathFinderPrepared::TYPE::DEFAULT>(&m_Data, m_Pos, m_PosTo);
	GameWorld()->InsertEntity(this);
}

void CEntityPathNavigator::Tick()
{
	// check
	if(!m_pParent || m_pParent->IsMarkedForDestroy())
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	// countdown
	if(m_TickCountDown > Server()->Tick())
		return;

	// update prepared data by required
	if(m_Data.IsRequiredUpdatePreparedData())
	{
		if(m_TickLastIdle < Server()->Tick() && distance(m_pParent->GetPos(), m_PosTo) > 240.f)
		{
			m_StepPos = 0;
			GS()->PathFinder()->SyncHandler()->Prepare<CPathFinderPrepared::TYPE::DEFAULT>(&m_Data, m_pParent->GetPos(), m_PosTo);
			GS()->PathFinder()->SyncHandler()->TryMarkAndUpdatePreparedData(&m_Data);
		}

		return;
	}

	// valid data
	m_Pos = m_Data.Get().m_Points[m_StepPos];

	// update timer by steps
	if(Server()->Tick() % (Server()->TickSpeed() / 15) == 0)
	{
		m_StepPos++;
		GS()->CreateDamage(vec2(m_Pos.x - 32.f, m_Pos.y - 64.f), -1, 1, 0, m_Mask);
	}

	// checking
	if(distance(m_pParent->GetPos(), m_Pos) > 800.0f || m_StepPos >= m_Data.Get().m_Size)
	{
		m_TickCountDown = Server()->Tick() + Server()->TickSpeed();
		m_Data.Get().Clear();
		m_First = false;
	}
}

void CEntityPathNavigator::PostSnap()
{
	// update last position
	if(m_pParent)
	{
		if(m_LastPos != m_pParent->GetPos() && m_TickLastIdle != -1 && !m_First)
		{
			m_TickLastIdle = Server()->Tick() + (Server()->TickSpeed() * 3);
		}
		m_LastPos = m_pParent->GetPos();
	}
}
