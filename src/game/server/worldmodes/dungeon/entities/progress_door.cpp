#include "progress_door.h"

#include <game/server/gamecontext.h>

CEntityDungeonProgressDoor::CEntityDungeonProgressDoor(CGameWorld* pGameWorld, vec2 Pos, int BotID)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_DUNGEON_DOOR, Pos, 14)
{
	GS()->Collision()->FillLengthWall(32, vec2(0, -1), &m_Pos, &m_PosTo);
	m_OpenedDoor = false;
	m_BotID = BotID;

	GameWorld()->InsertEntity(this);
}

void CEntityDungeonProgressDoor::Tick()
{
	if(m_OpenedDoor)
		return;

	for(CCharacter* pChar = (CCharacter*)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
	{
		vec2 IntersectPos;
		if(closest_point_on_line(m_Pos, m_PosTo, pChar->m_Core.m_Pos, IntersectPos))
		{
			float Distance = distance(IntersectPos, pChar->m_Core.m_Pos);
			if(Distance <= g_Config.m_SvDoorRadiusHit)
				pChar->SetDoorHit(m_Pos, m_PosTo);
		}
	}
}

bool CEntityDungeonProgressDoor::Update()
{
	bool NewState = true;

	for(int i = MAX_PLAYERS; i < MAX_CLIENTS; i++)
	{
		auto* pPlayer = dynamic_cast<CPlayerBot*>(GS()->GetPlayer(i));
		if(!pPlayer || !pPlayer->GetCharacter())
			continue;

		if(pPlayer->GetBotType() != EBotsType::TYPE_BOT_MOB || pPlayer->GetBotID() != m_BotID)
			continue;

		NewState = false;
		break;
	}

	bool Changed = (m_OpenedDoor != NewState);
	m_OpenedDoor = NewState;
	return Changed;
}

void CEntityDungeonProgressDoor::Snap(int SnappingClient)
{
	if(m_OpenedDoor || NetworkClipped(SnappingClient))
		return;

	GS()->SnapLaser(SnappingClient, GetID(), m_Pos, m_PosTo, Server()->Tick() - 3, LASERTYPE_DOOR);
}