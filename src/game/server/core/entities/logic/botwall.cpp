/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "botwall.h"

#include <game/server/gamecontext.h>

CBotWall::CBotWall(CGameWorld* pGameWorld, vec2 Pos, vec2 Direction, int Flag)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_BOT_WALL, Pos)
{
	GS()->Collision()->FillLengthWall(32, Direction, &m_Pos, &m_PosTo);
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

		const auto pPlayerBot = static_cast<CPlayerBot*>(pChar->GetPlayer());
		int BotType = pPlayerBot->GetBotType();
		if((m_Flag & WALLLINEFLAG_MOB_BOT) && (BotType == TYPE_BOT_MOB))
		{
			HitCharacter(pChar);
			continue;
		}

		int MobID = pPlayerBot->GetBotMobID();
		if((m_Flag & WALLLINEFLAG_NPC_BOT) && (BotType == TYPE_BOT_NPC) && (NpcBotInfo::ms_aNpcBot[MobID].m_Function != FUNCTION_NPC_GUARDIAN))
		{
			HitCharacter(pChar);
			continue;
		}

		if((m_Flag & WALLLINEFLAG_QUEST_BOT) && (BotType == TYPE_BOT_QUEST))
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

	GS()->SnapLaser(SnappingClient, GetID(), m_Pos, m_PosTo, Server()->Tick() - 4, LASERTYPE_FREEZE, LASERTYPE_DOOR);
}
