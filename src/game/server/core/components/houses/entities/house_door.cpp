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
	if(m_Health >= GetMaxHealth())
		return false;

	m_Health = minimum(m_Health + Health, GetMaxHealth());
	return true;
}


bool CDoorDurability::TakeDamage(int Damage)
{
	m_LastDamageTick = Instance::Server()->Tick();
	m_Health = maximum(0, m_Health - Damage);
	return m_Health > 0;
}


void CDoorDurability::Tick()
{
	auto* pServer = Instance::Server();
	if(pServer->Tick() > (m_LastDamageTick + (pServer->TickSpeed() * 60 * 10)))
	{
		IncreaseHealth(DEFAULT_HOUSE_DOOR_HEALTH);
		m_LastDamageTick = pServer->Tick();
	}
}


int CDoorDurability::GetMaxHealth() const
{
	if(m_pHouse->GetHouseType() == IHouse::Type::Guild)
	{
		auto* pHouse = dynamic_cast<CGuildHouse*>(m_pHouse);
		auto* pGuild = pHouse->GetGuild();
		if(pGuild)
		{
			const int currentHealth = pGuild->GetUpgrades().getRef<int>((int)GuildUpgrade::DoorHealth);
			return currentHealth * DEFAULT_HOUSE_DOOR_HEALTH;
		}
	}

	return DEFAULT_HOUSE_DOOR_HEALTH;
}


CEntityHouseDoor::CEntityHouseDoor(CGameWorld* pGameWorld, IHouse* pHouse, const std::string& Name, vec2 Pos)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_HOUSE_DOOR, Pos)
{
	// prepare positions
	GS()->Collision()->FillLengthWall(32, vec2(0, -1), &m_Pos, &m_PosTo, false);
	GS()->EntityManager()->LaserOrbite(this, 4, LaserOrbiteType::Default, 0.f, 16.f, LASERTYPE_DOOR);

	// initialize variables
	m_pHouse = pHouse;
	m_Name = Name;
	m_PosControll = Pos;
	m_State = CLOSED;
	m_DurabilityManager.Init(pHouse);

	// insert entity to gameworld
	GameWorld()->InsertEntity(this);
}


void CEntityHouseDoor::Tick()
{
	if(!HasPlayersInView())
		return;

	if((m_pHouse->GetHouseType() == IHouse::Type::Player && !PlayerHouseTick()) ||
		(m_pHouse->GetHouseType() == IHouse::Type::Guild && !GuildHouseTick()))
	{
		MarkForDestroy();
		return;
	}
}


void CEntityHouseDoor::Reverse()
{
	if(m_State == OPENED)
		Close();
	else
		Open();
}


bool CEntityHouseDoor::PlayerHouseTick()
{
	auto* pHouse = dynamic_cast<CHouse*>(m_pHouse);
	if(!pHouse)
		return false;

	auto OwnerUID = pHouse->GetAccountID();
	auto* pPlayer = GS()->GetPlayerByUserID(OwnerUID);

	// player control
	if(pPlayer && pPlayer->GetCharacter())
	{
		auto* pChar = pPlayer->GetCharacter();
		if(distance(m_PosControll, pChar->GetMousePos()) < 24.0f)
		{
			if(Server()->Input()->IsKeyClicked(pPlayer->GetCID(), KEY_EVENT_FIRE_HAMMER))
				Reverse();

			GS()->Broadcast(pPlayer->GetCID(), BroadcastPriority::GameInformation, 10, "Use hammer 'fire.' To operate the door '{}'!", m_Name);
		}
	}

	// is closed
	if(m_State == CLOSED)
	{
		for(auto* pChar = (CCharacter*)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
		{
			vec2 IntersectPos;
			if(closest_point_on_line(m_Pos, m_PosTo, pChar->m_Core.m_Pos, IntersectPos))
			{
				const auto Distance = distance(IntersectPos, pChar->m_Core.m_Pos);
				if(Distance <= g_Config.m_SvDoorRadiusHit)
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
					pChar->SetDoorHit(m_Pos, m_PosTo);
				}
			}
		}
	}

	return true;
}


bool CEntityHouseDoor::GuildHouseTick()
{
	auto* pHouse = dynamic_cast<CGuildHouse*>(m_pHouse);
	if(!pHouse)
		return false;

	// check if the house is purchased
	if(!pHouse->IsPurchased() && m_State == CLOSED)
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
		auto* pCheckGuild = pChar->GetPlayer()->Account()->GetGuild();

		// interaction by mouse
		if(distance(m_PosControll, pChar->GetMousePos()) < 24.0f)
		{
			const auto ClientID = pChar->GetPlayer()->GetCID();

			if(pCheckGuild && pHouse->IsPurchased() && pCheckGuild->GetID() == pHouse->GetGuild()->GetID() &&
				pChar->GetPlayer()->Account()->GetGuildMember()->CheckAccess(GUILD_RANK_RIGHT_UPGRADES_HOUSE))
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
		if(m_State == CLOSED)
		{
			vec2 IntersectPos;
			if(closest_point_on_line(m_Pos, m_PosTo, pChar->m_Core.m_Pos, IntersectPos))
			{
				const auto dist = distance(IntersectPos, pChar->m_Core.m_Pos);
				if(dist <= g_Config.m_SvDoorRadiusHit)
				{
					if(pCheckGuild && pHouse->IsPurchased() && pCheckGuild->GetID() == pHouse->GetGuild()->GetID())
						continue;

					// hit by door
					pChar->SetDoorHit(m_Pos, m_PosTo);
				}
			}
		}
	}

	return true;
}


void CEntityHouseDoor::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient) || m_State == OPENED || m_DurabilityManager.IsDestroyed())
		return;

	GS()->SnapLaser(SnappingClient, GetID(), m_Pos, m_PosTo, Server()->Tick() - m_DurabilityManager.GetTickShift(), LASERTYPE_DRAGGER);
}
