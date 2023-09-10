/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "path_navigator.h"

#include <game/server/gamecontext.h>

#include <game/server/mmocore/Components/Worlds/WorldManager.h>

#include "game/server/mmocore/PathFinder.h"

CEntityPathNavigator::CEntityPathNavigator(CGameWorld* pGameWorld, vec2 StartPos, vec2 EndPos, int ClientID, int WorldID)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_DROPBONUS, StartPos)
{
	m_WorldID = WorldID;
	m_ClientID = ClientID;
	GameWorld()->InsertEntity(this);

	SetNavigatorPositions(StartPos, EndPos, WorldID);
}

void CEntityPathNavigator::SetNavigatorPositions(vec2 StartPos, vec2 EndPos, int WorldID)
{
	vec2 GetterPos { 0,0 };
	GS()->Mmo()->WorldSwap()->FindPosition(WorldID, EndPos, &GetterPos);

	m_Pos = StartPos;
	m_PosTo = GetterPos;
	GS()->PathFinder()->SyncHandler()->Prepare<CPathFinderPrepared::TYPE::DEFAULT>(&m_Data, m_Pos, m_PosTo);
}

void CEntityPathNavigator::Tick()
{
	if(!GS()->m_apPlayers[m_ClientID])
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	if(GS()->PathFinder()->SyncHandler()->TryGetPreparedData(&m_Data))
		SetNavigatorPositions(m_Pos, m_PosTo, m_WorldID);

	m_Pos = GS()->m_apPlayers[m_ClientID]->m_ViewPos;
	dbg_msg("test", "%d", m_Data.Get().m_Size);
}

void CEntityPathNavigator::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	CNetObj_Projectile *pObj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, GetID(), sizeof(CNetObj_Projectile)));
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
