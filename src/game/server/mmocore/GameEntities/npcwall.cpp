/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "npcwall.h"

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>

CNPCWall::CNPCWall(CGameWorld *pGameWorld, vec2 Pos, bool Left, int Flag)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_NPC_DOOR, Pos)
{
	if(!Left)
		m_Pos.y += 30;
	else
		m_Pos.x -= 30;

	m_PosTo = GS()->Collision()->FindDirCollision(100, m_PosTo, (Left ? 'x' : 'y'), (Left ? '+' : '-'));
	m_Active = false;
	m_Flag = Flag;

	GameWorld()->InsertEntity(this);
}

void CNPCWall::Tick()
{
	// We're looking for bots
	m_Active = false;
	for (CCharacter* pChar = (CCharacter*)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
	{
		if(!pChar->GetPlayer()->IsBot())
			continue;

		vec2 IntersectPos = closest_point_on_line(m_Pos, m_PosTo, pChar->m_Core.m_Pos);
		const float Distance = distance(IntersectPos, pChar->m_Core.m_Pos);
		if(Distance <= g_Config.m_SvDoorRadiusHit * 3)
		{
			int BotType = pChar->GetPlayer()->GetBotType();
			if(((m_Flag & Flags::MOB_BOT && BotType == BotsTypes::TYPE_BOT_MOB) || (m_Flag & Flags::NPC_BOT && BotType == BotsTypes::TYPE_BOT_NPC) || (m_Flag & Flags::QUEST_BOT && BotType == BotsTypes::TYPE_BOT_QUEST)))
			{
				if(Distance <= g_Config.m_SvDoorRadiusHit)
					pChar->m_DoorHit = true;
				m_Active = true;
			}
		}
	}
}

void CNPCWall::Snap(int SnappingClient)
{
	if((m_Flag ^ Flags::MOB_BOT && !m_Active) || NetworkClipped(SnappingClient))
		return;

	if(GS()->GetClientVersion(SnappingClient) >= VERSION_DDNET_MULTI_LASER)
	{
		CNetObj_DDNetLaser* pObj = static_cast<CNetObj_DDNetLaser*>(Server()->SnapNewItem(NETOBJTYPE_DDNETLASER, GetID(), sizeof(CNetObj_DDNetLaser)));
		if(!pObj)
			return;

		pObj->m_ToX = int(m_Pos.x);
		pObj->m_ToY = int(m_Pos.y);
		pObj->m_FromX = int(m_PosTo.x);
		pObj->m_FromY = int(m_PosTo.y);
		pObj->m_StartTick = Server()->Tick() - 4;
		pObj->m_Owner = -1;
		pObj->m_Type = LASERTYPE_FREEZE;
	}
	else
	{
		CNetObj_Laser* pObj = static_cast<CNetObj_Laser*>(Server()->SnapNewItem(NETOBJTYPE_LASER, GetID(), sizeof(CNetObj_Laser)));
		if(!pObj)
			return;

		pObj->m_X = int(m_Pos.x);
		pObj->m_Y = int(m_Pos.y);
		pObj->m_FromX = int(m_PosTo.x);
		pObj->m_FromY = int(m_PosTo.y);
		pObj->m_StartTick = Server()->Tick() - 4;
	}
}
