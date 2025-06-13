/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "botwall.h"
#include <game/server/gamecontext.h>

CBotWall::CBotWall(CGameWorld* pGameWorld, vec2 Pos, vec2 Direction, int Flags)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_BOT_DOOR, Pos, 64)
{
	// prepare positions
	GS()->Collision()->FillLengthWall(Direction, &m_Pos, &m_PosTo);
	GS()->Collision()->SetDoorFromToCollisionAt(m_Pos, m_PosTo, TILE_STOPA, 0, GetID());

	// initialize variables
	m_Flags = Flags;
	m_Active = false;
	GameWorld()->InsertEntity(this);
}

void CBotWall::HitCharacter(CCharacter* pChar)
{
	if(is_within_distance_to_segment_sq(DOOR_ACTIVATION_RADIUS_SQUARED, m_Pos, m_PosTo, pChar->GetPos()))
	{
		pChar->SetDoorHit(GetID());
		m_Active = true;
	}
}

void CBotWall::Tick()
{
	m_Active = false;

	for(auto* pChar = (CCharacter*)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
	{
		auto* pPlayer = pChar->GetPlayer();
		if(!pPlayer->IsBot())
			continue;

		auto* pPlayerBot = static_cast<CPlayerBot*>(pPlayer);
		const int BotType = pPlayerBot->GetBotType();
		switch(BotType)
		{
			case TYPE_BOT_MOB:
				if(m_Flags & WALLLINEFLAG_MOB_BOT)
					HitCharacter(pChar);
				break;

			case TYPE_BOT_NPC:
				if(m_Flags & WALLLINEFLAG_NPC_BOT)
				{
					const int MobID = pPlayerBot->GetBotMobID();
					if(NpcBotInfo::ms_aNpcBot[MobID].m_Function != FUNCTION_NPC_GUARDIAN)
						HitCharacter(pChar);
				}
				break;

			case TYPE_BOT_QUEST:
				if(m_Flags & WALLLINEFLAG_QUEST_BOT)
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