/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "pickup.h"

#include <game/server/gamecontext.h>
#include <generated/server_data.h>
#include "character.h"

CPickup::CPickup(CGameWorld *pGameWorld, int Type, int SubType, vec2 Pos)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_PICKUP, Pos, 14.f)
{
	Init(Type, SubType, false);
	GameWorld()->InsertEntity(this);
}

CPickup::CPickup(CGameWorld *pGameWorld, int ProjType, vec2 Pos)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_PICKUP, Pos, 14.f)
{
	Init(ProjType, 0, true);
	GameWorld()->InsertEntity(this);
}

void CPickup::Init(int Type, int Subtype, bool Projectile)
{
	m_Type = Type;
	m_SubType = Subtype;
	m_Projectile = Projectile;
	CPickup::Reset();
}

void CPickup::Reset()
{
	if(g_pData->m_aPickups[m_Type].m_Spawndelay > 0)
	{
		m_SpawnTick = Server()->Tick() + Server()->TickSpeed() * g_pData->m_aPickups[m_Type].m_Spawndelay;
	}
	else
	{
		m_SpawnTick = -1;
	}
}

void CPickup::Tick()
{
	// wait for respawn
	if(m_SpawnTick > 0)
	{
		if(Server()->Tick() < m_SpawnTick)
			return;

		m_SpawnTick = -1;
		if(m_Type == POWERUP_ARMOR_GRENADE || m_Type == POWERUP_ARMOR_SHOTGUN || m_Type == POWERUP_ARMOR_LASER)
		{
			GS()->CreateSound(m_Pos, SOUND_WEAPON_SPAWN);
		}
	}

	if(!HasPlayersInView())
		return;

	auto* pChar = (CCharacter *)GS()->m_World.ClosestEntity(m_Pos, 20.0f, CGameWorld::ENTTYPE_CHARACTER, nullptr);
	if(!pChar || !pChar->IsAlive() || pChar->GetPlayer()->IsBot())
		return;

	bool Picked = false;
	auto* pPlayer = pChar->GetPlayer();

	if(m_Type == POWERUP_HEALTH && !m_Projectile)
	{
		const auto RestoreHealth = translate_to_percent_rest(pPlayer->GetMaxHealth(), 1);
		if(pChar->IncreaseHealth(RestoreHealth))
		{
			GS()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH);
			Picked = true;
		}
	}
	else if(m_Type == POWERUP_ARMOR && !m_Projectile)
	{
		const auto RestoreMana = translate_to_percent_rest(pPlayer->GetMaxMana(), 1);
		if(pChar->IncreaseMana(RestoreMana))
		{
			GS()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH);
			Picked = true;
		}
	}
	else if(m_Type == POWERUP_WEAPON && m_SubType == WEAPON_SHOTGUN && !m_Projectile)
	{
		const auto RealAmmo = 10 + pPlayer->GetTotalAttributeValue(AttributeIdentifier::Ammo);
		const auto RestoreAmmo = translate_to_percent_rest(RealAmmo, 40);
		if(pChar->GiveWeapon(WEAPON_SHOTGUN, RestoreAmmo))
		{
			Picked = true;
			GS()->CreateSound(m_Pos, SOUND_PICKUP_SHOTGUN);
		}
	}
	else if(m_Type == POWERUP_WEAPON && m_SubType == WEAPON_GRENADE && !m_Projectile)
	{
		const auto RealAmmo = 10 + pPlayer->GetTotalAttributeValue(AttributeIdentifier::Ammo);
		const auto RestoreAmmo = translate_to_percent_rest(RealAmmo, 40);
		if(pChar->GiveWeapon(WEAPON_GRENADE, RestoreAmmo))
		{
			Picked = true;
			GS()->CreateSound(m_Pos, SOUND_PICKUP_GRENADE);

		}
	}
	else if(m_Type == POWERUP_WEAPON && m_SubType == WEAPON_LASER && !m_Projectile)
	{
		const auto RealAmmo = 10 + pPlayer->GetTotalAttributeValue(AttributeIdentifier::Ammo);
		const auto RestoreAmmo = translate_to_percent_rest(RealAmmo, 40);
		if(pChar->GiveWeapon(WEAPON_LASER, RestoreAmmo))
		{
			Picked = true;
			GS()->CreateSound(m_Pos, SOUND_PICKUP_SHOTGUN);
		}
	}

	if(Picked)
	{
		const auto RespawnTime = g_pData->m_aPickups[m_Type].m_Respawntime;
		if(RespawnTime >= 0)
		{
			m_SpawnTick = Server()->Tick() + Server()->TickSpeed() * RespawnTime;
		}
	}
}

void CPickup::Snap(int SnappingClient)
{
	if(m_SpawnTick != -1 || NetworkClipped(SnappingClient))
		return;

	if(m_Projectile)
	{
		GS()->SnapProjectile(SnappingClient, GetID(), m_Pos, {}, Server()->Tick() - 4, m_Type);
	}
	else
	{
		GS()->SnapPickup(SnappingClient, GetID(), m_Pos, m_Type, m_SubType);
	}
}
