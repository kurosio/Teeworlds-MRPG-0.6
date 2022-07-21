/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildDoor.h"

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>

GuildDoor::GuildDoor(CGameWorld* pGameWorld, vec2 Pos, int GuildID)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_GUILD_HOUSE_DOOR, Pos)
{
	m_Pos.y += 30;
	m_PosTo = GS()->Collision()->FindDirCollision(100, m_PosTo, 'y', '-');
	m_GuildID = GuildID;
	GameWorld()->InsertEntity(this);
}
GuildDoor::~GuildDoor() {}

void GuildDoor::Tick()
{
	for(CCharacter* pChar = (CCharacter*)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
	{
		CPlayer* pPlayer = pChar->GetPlayer();
		if(m_GuildID == pPlayer->Acc().m_GuildID)
			continue;

		vec2 IntersectPos = closest_point_on_line(m_Pos, m_PosTo, pChar->m_Core.m_Pos);
		float Distance = distance(IntersectPos, pChar->m_Core.m_Pos);
		if(Distance <= g_Config.m_SvDoorRadiusHit)
			pChar->m_DoorHit = true;
	}
}

void GuildDoor::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	CNetObj_Laser* pObj = static_cast<CNetObj_Laser*>(Server()->SnapNewItem(NETOBJTYPE_LASER, GetID(), sizeof(CNetObj_Laser)));
	if(!pObj)
		return;

	pObj->m_X = int(m_Pos.x);
	pObj->m_Y = int(m_Pos.y);
	pObj->m_FromX = int(m_PosTo.x);
	pObj->m_FromY = int(m_PosTo.y);
	pObj->m_StartTick = Server()->Tick() - 2;
}