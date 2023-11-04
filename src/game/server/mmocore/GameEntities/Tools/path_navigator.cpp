/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "path_navigator.h"

#include <game/server/gamecontext.h>

#include <game/server/mmocore/Components/Worlds/WorldManager.h>

#include "game/server/mmocore/PathFinder.h"

CEntityPathNavigator::CEntityPathNavigator(CGameWorld* pGameWorld, CEntity* pParent, bool StartByCreating, vec2 FromPos, vec2 SearchPos, int WorldID, bool Projectile, int64 Mask)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_DROPBONUS, FromPos)
{
	vec2 PosTo { 0, 0 };
	GS()->Mmo()->WorldSwap()->FindPosition(WorldID, SearchPos, &PosTo);

	m_Mask = Mask;
	m_PosTo = PosTo;
	m_StepPos = 0;
	m_pParent = pParent;
	m_Projectile = Projectile;
	m_StartByCreating = StartByCreating;
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
	if(m_Projectile)
	{
		// smooth movement
		const vec2 Dir = m_Data.Get().m_Points.find(m_StepPos + 1) != m_Data.Get().m_Points.end()
			? normalize(m_Data.Get().m_Points[m_StepPos + 1] - m_Pos) : vec2 {};
		m_Pos += Dir * 5.f;

		// update timer by steps
		if(Server()->Tick() % (Server()->TickSpeed() / 10) == 0)
		{
			m_StepPos++;

			if(m_StepPos < (int)m_Data.Get().m_Points.size())
			{
				m_Pos = m_Data.Get().m_Points[m_StepPos];
			}
		}
	}
	else
	{
		// movement
		m_Pos = m_Data.Get().m_Points[m_StepPos];

		// update timer by steps
		if(Server()->Tick() % (Server()->TickSpeed() / 15) == 0)
		{
			m_StepPos++;
			GS()->CreateDamage(vec2(m_Pos.x - 32.f, m_Pos.y - 64.f), -1, 1, false, m_Mask);
		}
	}

	// checking
	if(distance(m_pParent->GetPos(), m_Pos) > 800.0f || m_StepPos >= (int)m_Data.Get().m_Points.size())
	{
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
