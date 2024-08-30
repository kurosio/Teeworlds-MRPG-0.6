/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "botwall.h"

#include <game/server/gamecontext.h>

CBotWall::CBotWall(CGameWorld* pGameWorld, vec2 Pos, vec2 Direction, int Flag)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_NPC_BLOCKER, Pos)
{
	GS()->Collision()->Wallline(32, Direction, &m_Pos, &m_PosTo);
	m_Active = false;
	m_Flag = Flag;

	GameWorld()->InsertEntity(this);
}

void CBotWall::HitCharacter(CCharacter* pChar)
{
	vec2 IntersectPos;
	if(closest_point_on_line(m_Pos, m_PosTo, pChar->m_Core.m_Pos, IntersectPos))
	{
		const float Distance = distance(IntersectPos, pChar->m_Core.m_Pos);
		if(Distance <= g_Config.m_SvDoorRadiusHit * 3)
		{
			if(Distance <= g_Config.m_SvDoorRadiusHit)
			{
				pChar->SetDoorHit(m_Pos, m_PosTo);
			}
			m_Active = true;
		}
	}
}

void CBotWall::Tick()
{
	m_Active = false;

	for(CCharacter* pChar = (CCharacter*)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
	{
		if(!pChar->GetPlayer()->IsBot())
			continue;

		int BotType = pChar->GetPlayer()->GetBotType();
		if((m_Flag & Flags::WALLLINEFLAG_MOB_BOT) && (BotType == BotsTypes::TYPE_BOT_MOB))
		{
			HitCharacter(pChar);
			continue;
		}

		int MobID = pChar->GetPlayer()->GetBotMobID();
		if((m_Flag & Flags::WALLLINEFLAG_NPC_BOT) && (BotType == BotsTypes::TYPE_BOT_NPC) && (NpcBotInfo::ms_aNpcBot[MobID].m_Function != FUNCTION_NPC_GUARDIAN))
		{
			HitCharacter(pChar);
			continue;
		}

		if((m_Flag & Flags::WALLLINEFLAG_QUEST_BOT) && (BotType == BotsTypes::TYPE_BOT_QUEST))
		{
			HitCharacter(pChar);
			continue;
		}
	}
}

void CBotWall::Snap(int SnappingClient)
{
	if(!m_Active || NetworkClipped(SnappingClient))
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
