/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "house_door.h"

#include <game/server/entity_manager.h>
#include <game/server/gamecontext.h>
#include <generated/server_data.h>

#include <components/houses/guild_house_data.h>
#include <components/houses/house_data.h>
#include <game/server/core/components/guilds/guild_data.h>

CEntityHouseDoor::CEntityHouseDoor(CGameWorld* pGameWorld, IHouse* pHouse, const std::string& Name, vec2 Pos)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_DEFAULT_DOOR, Pos)
{
	// prepare positions
	GS()->Collision()->FillLengthWall(vec2(0, -1), &m_Pos, &m_PosTo, 32, true, false);
	GS()->Collision()->SetDoorFromToCollisionAt(m_Pos, m_PosTo, TILE_STOPA, 0, GetID());
	GS()->EntityManager()->LaserOrbit(this, 4, LaserOrbitType::Default, 0.f, 16.f, LASERTYPE_DOOR);

	// initialize variables
	m_pHouse = pHouse;
	m_Name = Name;
	m_PosControll = Pos;
	m_State = State::Closed;

	// insert entity to gameworld
	GameWorld()->InsertEntity(this);
}

void CEntityHouseDoor::Tick()
{
	bool IsActive = true;
	if(m_pHouse->GetHouseType() == IHouse::Type::Player)
	{
		IsActive = PlayerHouseTick(static_cast<CHouse*>(m_pHouse));
	}
	else if(m_pHouse->GetHouseType() == IHouse::Type::Guild)
	{
		IsActive = GuildHouseTick(static_cast<CGuildHouse*>(m_pHouse));
	}

	if(!IsActive)
	{
		MarkForDestroy();
		return;
	}
}


void CEntityHouseDoor::Open()
{
	if(m_State != State::Opened)
	{
		m_State = State::Opened;
		GS()->CreateSound(m_Pos, SOUND_SFX_DOOR);
	}
}

void CEntityHouseDoor::Close()
{
	if(m_State != State::Closed)
	{
		m_State = State::Closed;
		GS()->CreateSound(m_Pos, SOUND_SFX_DOOR);
	}
}

void CEntityHouseDoor::Reverse()
{
	if(m_State == State::Opened)
		Close();
	else
		Open();
}

bool CEntityHouseDoor::PlayerHouseTick(CHouse* pHouse)
{
	if(!pHouse)
		return false;

	// open doors in houses without an owner
	if(!pHouse->HasOwner() && m_State == State::Closed)
	{
		Open();
		return true;
	}

	// player control
	if(auto* pPlayer = GS()->GetPlayerByUserID(pHouse->GetAccountID()))
	{
		if(auto* pChar = pPlayer->GetCharacter())
		{
			const auto ClientID = pPlayer->GetCID();
			if(distance(m_PosControll, pChar->GetMousePos()) < 24.0f)
			{
				if(Server()->Input()->IsKeyClicked(ClientID, KEY_EVENT_FIRE_HAMMER))
					Reverse();

				GS()->Broadcast(ClientID, BroadcastPriority::GameInformation, 10, "Use hammer 'fire.' To operate the door '{}'!", m_Name);
			}
		}
	}

	// is closed
	if(m_State == State::Closed)
	{
		for(auto* pChar = (CCharacter*)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
		{
			if(is_within_distance_to_segment_sq(DOOR_ACTIVATION_RADIUS_SQUARED, m_Pos, m_PosTo, pChar->GetPos()))
			{
				// only for has access
				if(pHouse->GetAccountID() == pChar->GetPlayer()->Account()->GetID())
					continue;

				// skip eidolon
				if(pChar->GetPlayer()->IsBot())
				{
					auto* pPlayerBot = static_cast<CPlayerBot*>(pChar->GetPlayer());
					if(pPlayerBot->GetEidolonOwner() && pHouse->GetAccountID() == pPlayerBot->GetEidolonOwner()->Account()->GetID())
						continue;
				}

				// hit by door
				pChar->SetDoorHit(GetID());
			}
		}
	}

	return true;
}

bool CEntityHouseDoor::GuildHouseTick(CGuildHouse* pHouse)
{
	if(!pHouse)
		return false;

	// open doors in houses without an owner
	const bool HouseIsPurchased = pHouse->IsPurchased();
	if(!HouseIsPurchased && m_State == State::Closed)
	{
		Open();
		return true;
	}

	// interact with the door
	for(auto* pChar = (CCharacter*)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
	{

		// interaction by mouse
		if(distance(m_PosControll, pChar->GetMousePos()) < 24.0f)
		{
			// initialize variables
			const auto* pPlayer = pChar->GetPlayer();
			const auto* pAccount = pPlayer->Account();
			const auto* pGuild = pAccount->GetGuild();
			const auto* pMember = pAccount->GetGuildMember();
			const auto ClientID = pPlayer->GetCID();

			// check valid
			if(pGuild && pMember && HouseIsPurchased && pGuild->GetID() == pHouse->GetGuild()->GetID() &&
				pMember->CheckAccess(RANKFLAG_RIGHT_HOUSE_DOORS))
			{
				if(Server()->Input()->IsKeyClicked(ClientID, KEY_EVENT_FIRE_HAMMER))
					Reverse();

				GS()->Broadcast(ClientID, BroadcastPriority::GameInformation, 10, "Use hammer 'fire.' To operate the door '{}'!", m_Name);
			}
			else
				GS()->Broadcast(ClientID, BroadcastPriority::GameInformation, 10, "You do not have access to '{}' door!", m_Name);
		}

		// is closed
		if(m_State == State::Closed)
		{
			if(is_within_distance_to_segment_sq(DOOR_ACTIVATION_RADIUS_SQUARED, m_Pos, m_PosTo, pChar->GetPos()))
			{
				// only for has access
				auto* pGuild = pChar->GetPlayer()->Account()->GetGuild();
				if(pGuild && HouseIsPurchased && pGuild->GetID() == pHouse->GetGuild()->GetID())
					continue;

				pChar->SetDoorHit(GetID());
			}
		}
	}

	return true;
}

void CEntityHouseDoor::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient) || m_State == State::Opened)
		return;

	GS()->SnapLaser(SnappingClient, GetID(), m_Pos, m_PosTo, Server()->Tick() - 2, LASERTYPE_DOOR);
}
