/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "guild_door.h"

#include <game/server/entity_manager.h>
#include <game/server/gamecontext.h>

#include "game/server/core/components/guilds/guild_data.h"
#include "game/server/core/components/guilds/guild_house_data.h"

CEntityGuildDoor::CEntityGuildDoor(CGameWorld* pGameWorld, CGuildHouse* pHouse, std::string&& Name, vec2 Pos)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_PLAYER_HOUSE_DOOR, Pos), m_Name(std::move(Name)), m_pHouse(pHouse)
{
	// check if the guild house is null
	dbg_assert(m_pHouse != nullptr, "guild house is null for the door");

	// initialize the door
	GS()->Collision()->FillLengthWall(32, vec2(0, -1), &m_Pos, &m_PosTo, false);
	m_PosControll = Pos;
	m_State = CLOSED;
	GS()->EntityManager()->LaserOrbite(this, 4, LaserOrbiteType::Default, 0.f, 16.f, LASERTYPE_DOOR);

	// insert the entity into the game world
	GameWorld()->InsertEntity(this);
}

void CEntityGuildDoor::Reverse()
{
	if (m_State == OPENED)
		Close();
	else
		Open();
}

void CEntityGuildDoor::Tick()
{
    // check if the house is purchased
    if(!m_pHouse->IsPurchased())
    {
    	if(m_State == CLOSED)
           Open();
    	return;
    }

    // interact with the door
    for (CCharacter* pChar = (CCharacter*)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
    {
        const int ClientID = pChar->GetPlayer()->GetCID();
        CGuild* pCharGuild = pChar->GetPlayer()->Account()->GetGuild();

        // setting the door state
        if(distance(m_PosControll, pChar->GetMousePos()) < 24.0f)
        {
            if(pCharGuild && m_pHouse->IsPurchased() && pCharGuild->GetID() == m_pHouse->GetGuild()->GetID() &&
                pChar->GetPlayer()->Account()->GetGuildMember()->CheckAccess(GUILD_RANK_RIGHT_UPGRADES_HOUSE))
            {
                // is clicked key
                if(Server()->Input()->IsKeyClicked(ClientID, KEY_EVENT_FIRE_HAMMER))
                    Reverse();

                // send information
                GS()->Broadcast(ClientID, BroadcastPriority::GameInformation, 10, "Use hammer 'fire.' To operate the door '{}'!", m_Name);
            }
            else
            {
                // send information
                GS()->Broadcast(ClientID, BroadcastPriority::GameInformation, 10, "You do not have access to '{}' door!", m_Name);
            }
        }

        // Check if the door is closed
        if (m_State == CLOSED)
        {
            vec2 IntersectPos;
            if(closest_point_on_line(m_Pos, m_PosTo, pChar->m_Core.m_Pos, IntersectPos))
            {
                // check distance
                const float Distance = distance(IntersectPos, pChar->m_Core.m_Pos);
                if (Distance <= g_Config.m_SvDoorRadiusHit)
                {
                    // check for guild member
                    if(pCharGuild && m_pHouse->IsPurchased() && pCharGuild->GetID() == m_pHouse->GetGuild()->GetID())
                        continue;

                    pChar->SetDoorHit(m_Pos, m_PosTo);
                }
            }
        }
    }
}

void CEntityGuildDoor::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient, true) || m_State == OPENED)
		return;

	if(GS()->GetClientVersion(SnappingClient) >= VERSION_DDNET_MULTI_LASER)
	{
		CNetObj_DDNetLaser* pObj = static_cast<CNetObj_DDNetLaser*>(Server()->SnapNewItem(NETOBJTYPE_DDNETLASER, GetID(), sizeof(CNetObj_DDNetLaser)));
		if(!pObj)
			return;

		pObj->m_ToX = round_to_int(m_Pos.x);
		pObj->m_ToY = round_to_int(m_Pos.y);
		pObj->m_FromX = round_to_int(m_PosTo.x);
		pObj->m_FromY = round_to_int(m_PosTo.y);
		pObj->m_StartTick = Server()->Tick() - 2;
		pObj->m_Owner = -1;
		pObj->m_Type = LASERTYPE_DOOR;
	}
	else
	{
		CNetObj_Laser* pObj = static_cast<CNetObj_Laser*>(Server()->SnapNewItem(NETOBJTYPE_LASER, GetID(), sizeof(CNetObj_Laser)));
		if(!pObj)
			return;

		pObj->m_X = round_to_int(m_Pos.x);
		pObj->m_Y = round_to_int(m_Pos.y);
		pObj->m_FromX = round_to_int(m_PosTo.x);
		pObj->m_FromY = round_to_int(m_PosTo.y);
		pObj->m_StartTick = Server()->Tick() - 2;
	}
}