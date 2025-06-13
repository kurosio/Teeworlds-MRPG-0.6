#include "progress_door.h"

#include <game/server/gamecontext.h>

CEntityDungeonProgressDoor::CEntityDungeonProgressDoor(CGameWorld* pGameWorld, vec2 Pos, int BotID)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_DUNGEON_DOOR, Pos, 14)
{
	// prepare wall line
	GS()->Collision()->FillLengthWall(vec2(0, -1), &m_Pos, &m_PosTo);
	GS()->Collision()->SetDoorFromToCollisionAt(m_Pos, m_PosTo, TILE_STOPA, 0, GetID());

	// initialize variables
	m_OpenedDoor = false;
	m_BotID = BotID;

	GameWorld()->InsertEntity(this);
}

void CEntityDungeonProgressDoor::Tick()
{
	if(m_OpenedDoor)
		return;

	for(auto* pChar = (CCharacter*)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
	{
		if(is_within_distance_to_segment_sq(DOOR_ACTIVATION_RADIUS_SQUARED, m_Pos, m_PosTo, pChar->m_Core.m_Pos))
			pChar->SetDoorHit(GetID());
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