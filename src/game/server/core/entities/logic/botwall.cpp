/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "botwall.h"

#include <game/server/gamecontext.h>

CBotWall::CBotWall(CGameWorld* pGameWorld, vec2 Pos, vec2 Direction, int Flag)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_BOT_DOOR, Pos, 64)
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
		if(Distance <= (GetRadius() * 3))
		{
			pChar->SetDoorHit(GetID());
			m_Active = true;
		}
	}
}

void CBotWall::Tick()
{
	m_Active = false;

	const bool flagCheckMobBots = (m_Flag & WALLLINEFLAG_MOB_BOT);
	const bool flagCheckNpcBots = (m_Flag & WALLLINEFLAG_NPC_BOT);
	const bool flagCheckQuestBots = (m_Flag & WALLLINEFLAG_QUEST_BOT);
	for(auto* pChar = (CCharacter*)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
	{
		auto* pPlayer = pChar->GetPlayer();
		if(!pPlayer->IsBot())
			continue;

		auto* pPlayerBot = dynamic_cast<CPlayerBot*>(pPlayer);
		const int BotType = pPlayerBot->GetBotType();
		switch(BotType)
		{
			case TYPE_BOT_MOB:
				if(flagCheckMobBots)
					HitCharacter(pChar);
				break;

			case TYPE_BOT_NPC:
				if(flagCheckNpcBots)
				{
					const int MobID = pPlayerBot->GetBotMobID();
					if(NpcBotInfo::ms_aNpcBot[MobID].m_Function != FUNCTION_NPC_GUARDIAN)
						HitCharacter(pChar);
				}
				break;

			case TYPE_BOT_QUEST:
				if(flagCheckQuestBots)
					HitCharacter(pChar);
				break;
			default: break;
		}
	}
}

void CBotWall::Snap(int SnappingClient)
{
	if(!m_Active || (NetworkClipped(SnappingClient) && NetworkClipped(SnappingClient, m_PosTo)))
		return;

	GS()->SnapLaser(SnappingClient, GetID(), m_Pos, m_PosTo, Server()->Tick() - 2, LASERTYPE_DOOR);
}
