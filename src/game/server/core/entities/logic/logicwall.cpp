/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "logicwall.h"

#include <game/server/entity_manager.h>
#include <game/server/gamecontext.h>

CLogicWall::CLogicWall(CGameWorld *pGameWorld, vec2 Pos)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_TOOLS, Pos)
{
	m_RespawnTick = Server()->TickSpeed()*10;
	pLogicWallLine = new CLogicWallLine(&GS()->m_World, m_Pos);
	GameWorld()->InsertEntity(this);
}

void CLogicWall::SetDestroy(int Sec)
{
	m_RespawnTick = Server()->TickSpeed()*Sec;
	if(pLogicWallLine)
	{
		pLogicWallLine->Respawn(false);
	}
}

CPlayer *CLogicWall::FindPlayerAI(float Distance)
{
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		CPlayer *pPlayer = GS()->GetPlayer(i, true, true);
		if(!pPlayer || distance(pPlayer->GetCharacter()->m_Core.m_Pos, m_Pos) > Distance)
			continue;
		return pPlayer;
	}
	return NULL;
}

void CLogicWall::Tick()
{
	if(m_RespawnTick)
	{
		m_RespawnTick--;
		if(!m_RespawnTick)
		{
			if(pLogicWallLine)
				pLogicWallLine->Respawn(true);
		}
		return;
	}

	CPlayer *pPlayer = FindPlayerAI(250.0f);
	if(Server()->Tick() % (Server()->TickSpeed()*5) == 0 && pPlayer)
	{
		pLogicWallLine->SetClientID(pPlayer->GetCID());
		vec2 Dir = normalize(m_Pos - pPlayer->GetCharacter()->m_Core.m_Pos);
		new CLogicWallFire(&GS()->m_World, m_Pos, Dir, this);
	}
}

void CLogicWall::Snap(int SnappingClient)
{
	if(m_RespawnTick > 0 || NetworkClipped(SnappingClient))
		return;

	CNetObj_Pickup *pP = static_cast<CNetObj_Pickup *>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, GetID(), sizeof(CNetObj_Pickup)));
	if(!pP)
		return;

	pP->m_X = (int)m_Pos.x;
	pP->m_Y = (int)m_Pos.y;
	pP->m_Type = 1;
}

/////////////////////////////////////////
CLogicWallFire::CLogicWallFire(CGameWorld *pGameWorld, vec2 Pos, vec2 Direction, CLogicWall *Eyes)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_TOOLS, Pos, 14)
{
	m_Pos = Pos;
	pLogicWall = Eyes;
	m_Dir = Direction;
	GameWorld()->InsertEntity(this);
}

void CLogicWallFire::Tick()
{
	if(!pLogicWall || GS()->Collision()->CheckPoint(m_Pos.x, m_Pos.y))
	{
		GameWorld()->DestroyEntity(this);
		return;
	}

	/*
	for(CLogicWallWall *p = (CLogicWallWall*) GameWorld()->FindFirst(CGameWorld::ENTTYPE_EYES_WALL); p; p = (CLogicWallWall *)p->TypeNext())
	{
		vec2 IntersectPos;
		if(closest_point_on_line(p->GetPos(), p->GetTo(), m_Pos, IntersectPos))
		{
			if(distance(m_Pos, IntersectPos) < 15)
			{
				p->TakeDamage();
				if(p->GetHealth() <= 0)
				{
					pLogicWall->SetDestroy(120);
					p->SetDestroy(120);
				}

				GS()->EntityManager()->Text(m_Pos, 100, std::to_string(p->GetHealth()).c_str());
				GameWorld()->DestroyEntity(this);
				return;
			}
		}
	}*/
	m_Pos += m_Dir*2.0f;
}

void CLogicWallFire::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	CNetObj_Pickup *pP = static_cast<CNetObj_Pickup *>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, GetID(), sizeof(CNetObj_Pickup)));
	if(!pP)
		return;

	pP->m_X = (int)m_Pos.x;
	pP->m_Y = (int)m_Pos.y;
	pP->m_Type = 1;
}

/////////////////////////////////////////
CLogicWallWall::CLogicWallWall(CGameWorld *pGameWorld, vec2 Pos, int Mode, int Health)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_TOOLS, Pos, 14)
{
	vec2 Direction = Mode == 0 ? vec2(0, -1) : vec2(1, 0);
	GS()->Collision()->FillLengthWall(Direction, &m_Pos, &m_PosTo);
	GS()->Collision()->SetDoorFromToCollisionAt(m_Pos, m_PosTo, TILE_STOPA, 0, GetID());

	m_RespawnTick = Server()->TickSpeed()*10;

	GameWorld()->InsertEntity(this);
}

void CLogicWallWall::TakeDamage()
{
	m_Health -= 25;
}

void CLogicWallWall::SetDestroy(int Sec) { m_RespawnTick = Server()->TickSpeed()*Sec, m_Health = m_SaveHealth; }

void CLogicWallWall::Tick()
{
	if(m_RespawnTick)
		m_RespawnTick--;

	if(!m_RespawnTick)
	{
		for (auto* pChar = (CCharacter*)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
		{
			if(is_within_distance_to_segment_sq(DOOR_ACTIVATION_RADIUS_SQUARED, m_Pos, m_PosTo, pChar->m_Core.m_Pos))
				pChar->SetDoorHit(GetID());
		}
	}
}

void CLogicWallWall::Snap(int SnappingClient)
{
	if (m_RespawnTick > 0 || NetworkClipped(SnappingClient))
		return;

	GS()->SnapLaser(SnappingClient, GetID(), m_Pos, m_PosTo, Server()->Tick() - 2, LASERTYPE_DOOR);
}

/////////////////////////////////////////
/////////////////////////////////////////
/////////////////////////////////////////
/////////////////////////////////////////
/////////////////////////////////////////
CLogicWallLine::CLogicWallLine(CGameWorld *pGameWorld, vec2 Pos)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_LASER, Pos, 14)
{
	m_ClientID = -1;
	m_Spawned = false;
	GameWorld()->InsertEntity(this);
}
void CLogicWallLine::Respawn(bool Spawn) { m_Spawned = Spawn; }

void CLogicWallLine::Tick()
{
	CPlayer* pPlayer = GS()->GetPlayer(m_ClientID);
	if(m_ClientID < 0 || m_ClientID >= MAX_PLAYERS || !pPlayer || !pPlayer->GetCharacter())
	{
		m_PosTo = m_Pos;
		return;
	}

	vec2 PositionPlayer = pPlayer->GetCharacter()->m_Core.m_Pos;
	if(m_Spawned && distance(m_Pos, PositionPlayer) < 300)
	{
		m_PosTo = PositionPlayer;
		if(Server()->Tick() % Server()->TickSpeed() == 0)
			pPlayer->GetCharacter()->TakeDamage(vec2(0,0), 1, -1, WEAPON_WORLD);
		return;
	}
	m_PosTo = m_Pos;
}

void CLogicWallLine::Snap(int SnappingClient)
{
	if (!m_Spawned || NetworkClipped(SnappingClient))
		return;

	CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, GetID(), sizeof(CNetObj_Laser)));
	if (!pObj)
		return;

	pObj->m_X = int(m_Pos.x);
	pObj->m_Y = int(m_Pos.y);
	pObj->m_FromX = int(m_PosTo.x);
	pObj->m_FromY = int(m_PosTo.y);
	pObj->m_StartTick = Server()->Tick()-5;
}


/////////////////////////////////////////
/////////////////////////////////////////
/////////////////////////////////////////
/////////////////////////////////////////
/////////////////////////////////////////
CLogicDoorKey::CLogicDoorKey(CGameWorld *pGameWorld, vec2 Pos, int ItemID, int Mode)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_LASER, Pos, 128)
{
	vec2 Direction = Mode == 0 ? vec2(0, -1) : vec2(1, 0);
	GS()->Collision()->FillLengthWall(Direction, &m_Pos, &m_PosTo);
	GS()->Collision()->SetDoorFromToCollisionAt(m_Pos, m_PosTo, TILE_STOPA, 0, GetID());

	m_ItemID = ItemID;

	GameWorld()->InsertEntity(this);
}

void CLogicDoorKey::Tick()
{
	for (CCharacter* pChar = (CCharacter*)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
	{
		CPlayer* pPlayer = pChar->GetPlayer();
		if(pPlayer->GetItem(m_ItemID)->GetValue())
			continue;

		vec2 IntersectPos;
		if(closest_point_on_line(m_Pos, m_PosTo, pChar->m_Core.m_Pos, IntersectPos))
		{
			float Distance = distance(IntersectPos, pChar->m_Core.m_Pos);
			if(Distance <= GetRadius())
				GS()->Broadcast(pChar->GetPlayer()->GetCID(), BroadcastPriority::GameWarning, 100, "You need {}", GS()->GetItemInfo(m_ItemID)->GetName());

			pChar->SetDoorHit(GetID());
		}
	}
}

void CLogicDoorKey::Snap(int SnappingClient)
{
	if (NetworkClipped(SnappingClient))
		return;

	GS()->SnapLaser(SnappingClient, GetID(), m_Pos, m_PosTo, Server()->Tick() - 3, LASERTYPE_DOOR);
}