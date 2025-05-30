/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "botwall.h"

#include <game/server/gamecontext.h>

CBotWall::CBotWall(CGameWorld* pGameWorld, vec2 Pos, vec2 Direction, int Flag)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_BOT_WALL, Pos, 64)
{
	// prepare positions
	GS()->Collision()->FillLengthWall(Direction, &m_Pos, &m_PosTo);
	GS()->Collision()->SetDoorFromToCollisionAt(m_Pos, m_PosTo, TILE_STOPA, 0, GetID());

	// initialize variables
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
		if(Distance <= GetRadius())
			m_Active = true;

		pChar->SetDoorHit(GetID());
	}
}

void CBotWall::Tick()
{
	if(!HasPlayersInView())
		return;

	m_Active = false;
	for(auto* pChar = (CCharacter*)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
	{
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
	if(NetworkClipped(SnappingClient) && NetworkClipped(SnappingClient, m_PosTo))
		return;

	if(m_Active)
		GS()->SnapLaser(SnappingClient, GetID(), m_Pos, m_PosTo, Server()->Tick() - 2, LASERTYPE_DOOR);
}
