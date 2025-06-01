/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "drop_pickup.h"

#include <game/server/gamecontext.h>

CEntityDropPickup::CEntityDropPickup(CGameWorld *pGameWorld, vec2 Pos, vec2 Vel, int Type, int Subtype, int Value)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_PICKUP, Pos, 16.f)
{
	m_Vel = Vel;
	m_Type = Type;
	m_Subtype = Subtype;
	m_Value = Value;
	m_LifeSpan = Server()->TickSpeed() * 15;

	GameWorld()->InsertEntity(this);
}

void CEntityDropPickup::Tick()
{
	m_LifeSpan--;
	if (m_LifeSpan < 0)
	{
		GS()->CreatePlayerSpawn(m_Pos);
		GameWorld()->DestroyEntity(this);
		return;
	}

	if(!HasPlayersInView())
		return;

	m_Flash.Tick(m_LifeSpan);
	GS()->Collision()->MovePhysicalBox(&m_Pos, &m_Vel, vec2(m_Radius, m_Radius), 0.5f);

	// interactive
	auto *pChar = (CCharacter*)GameWorld()->ClosestEntity(m_Pos, m_Radius, CGameWorld::ENTTYPE_CHARACTER, nullptr);
	if(pChar && !pChar->GetPlayer()->IsBot())
	{
		// health
		if (m_Type == POWERUP_HEALTH)
		{
			GS()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH);
		}

		// experience
		else if (m_Type == POWERUP_ARMOR)
		{
			pChar->GetPlayer()->Account()->AddExperience(m_Value);
			GS()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR);
		}

		// weapons
		else if (m_Type == POWERUP_WEAPON)
		{
			// shotgun
			if (m_Subtype == WEAPON_SHOTGUN && pChar->GetPlayer()->IsEquippedSlot(ItemType::EquipShotgun))
			{
				pChar->GiveWeapon(m_Subtype, m_Value);
				GS()->CreateSound(m_Pos, SOUND_PICKUP_SHOTGUN);
			}
			// grenade
			else if (m_Subtype == WEAPON_GRENADE && pChar->GetPlayer()->IsEquippedSlot(ItemType::EquipGrenade))
			{
				pChar->GiveWeapon(m_Subtype, m_Value);
				GS()->CreateSound(m_Pos, SOUND_PICKUP_GRENADE);
			}
			// laser
			else if (m_Subtype == WEAPON_LASER && pChar->GetPlayer()->IsEquippedSlot(ItemType::EquipLaser))
			{
				pChar->GiveWeapon(m_Subtype, m_Value);
				GS()->CreateSound(m_Pos, SOUND_PICKUP_SHOTGUN);
			}
		}

		GameWorld()->DestroyEntity(this);
	}
}

void CEntityDropPickup::Snap(int SnappingClient)
{
	if(m_Flash.IsFlashing() || NetworkClipped(SnappingClient))
		return;

	GS()->SnapPickup(SnappingClient, GetID(), m_Pos, m_Type, m_Subtype);
}