/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "house_door.h"

#include <game/server/entity_manager.h>
#include <game/server/gamecontext.h>

#include "../guild_house_data.h"
#include "../house_data.h"

void CDoorDurability::Init(IHouse* pHouse)
{
	m_pHouse = pHouse;
	m_Health = GetMaxHealth();
}

bool CDoorDurability::IncreaseHealth(int Health)
{
	const int MaxHealth = GetMaxHealth();
	if(m_Health >= MaxHealth)
		return false;

	m_Health = std::clamp(m_Health + Health, m_Health, MaxHealth);
	return true;
}

bool CDoorDurability::TakeDamage(int Damage)
{
	m_LastDamageTick = Instance::Server()->Tick();
	m_Health = std::clamp(m_Health - Damage, 0, GetMaxHealth());
	return m_Health > 0;
}

void CDoorDurability::Tick()
{
	auto* pServer = Instance::Server();
	const int RegenDelayTicks = pServer->TickSpeed() * 60 * 10;
	if(pServer->Tick() > (m_LastDamageTick + RegenDelayTicks))
	{
		IncreaseHealth(DEFAULT_HOUSE_DOOR_HEALTH);
		m_LastDamageTick = pServer->Tick();
	}
}

int CDoorDurability::GetMaxHealth() const
{
	if(m_pHouse->GetHouseType() == IHouse::Type::Guild)
	{
		auto* pHouse = static_cast<const CGuildHouse*>(m_pHouse);
		if(auto* pGuild = pHouse->GetGuild())
		{
			const int currentHealth = pGuild->GetUpgrades().getRef<int>((int)GuildUpgrade::DoorHealth);
			return currentHealth * DEFAULT_HOUSE_DOOR_HEALTH;
		}
	}

	return DEFAULT_HOUSE_DOOR_HEALTH;
}

CEntityHouseDoor::CEntityHouseDoor(CGameWorld* pGameWorld, IHouse* pHouse, const std::string& Name, vec2 Pos)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_DEFAULT_DOOR, Pos)
{
	// prepare positions
	GS()->Collision()->FillLengthWall(vec2(0, -1), &m_Pos, &m_PosTo, 32, true, false);
	GS()->Collision()->SetDoorFromToCollisionAt(m_Pos, m_PosTo, TILE_STOPA, 0, GetID());
	GS()->EntityManager()->LaserOrbite(this, 4, LaserOrbiteType::Default, 0.f, 16.f, LASERTYPE_DOOR);

	// initialize variables
	m_pHouse = pHouse;
	m_Name = Name;
	m_PosControll = Pos;
	m_State = State::Closed;
	m_DurabilityManager.Init(pHouse);

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

	// check if the house is purchased
	const bool HouseIsPurchased = pHouse->IsPurchased();
	if(!HouseIsPurchased && m_State == State::Closed)
	{
		Open();
		return true;
	}

	// durability manager
	m_DurabilityManager.Tick();
	if(m_DurabilityManager.IsDestroyed())
		return true;

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
			const auto ClientID = pPlayer->GetCID();

			// check valid
			if(pGuild && HouseIsPurchased && pGuild->GetID() == pHouse->GetGuild()->GetID() &&
				pAccount->GetGuildMember()->CheckAccess(GUILD_RANK_RIGHT_UPGRADES_HOUSE))
			{
				if(Server()->Input()->IsKeyClicked(ClientID, KEY_EVENT_FIRE_HAMMER))
					Reverse();

				GS()->Broadcast(ClientID, BroadcastPriority::GameInformation, 10, "Use hammer 'fire.' To operate the door '{}'!", m_Name);
			}
			else
			{
				// TODO: Add logic durability house.
				GS()->Broadcast(ClientID, BroadcastPriority::GameInformation, 10, "You do not have access to '{}' door!", m_Name);
			}
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
	if(NetworkClipped(SnappingClient) || m_State == State::Opened || m_DurabilityManager.IsDestroyed())
		return;

	GS()->SnapLaser(SnappingClient, GetID(), m_Pos, m_PosTo, Server()->Tick() - 2, LASERTYPE_DOOR);
}