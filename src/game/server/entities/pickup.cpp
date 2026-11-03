/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "pickup.h"

#include <game/server/gamecontext.h>
#include <generated/server_data.h>
#include "character.h"
#include <game/server/core/entities/items/drop_pickup.h>

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

bool CPickup::CanMakeDrop() const
{
	const bool Supported = m_Type == POWERUP_HEALTH || m_Type == POWERUP_ARMOR ||
		(m_Type == POWERUP_WEAPON && (m_SubType == WEAPON_SHOTGUN || m_SubType == WEAPON_GRENADE || m_SubType == WEAPON_LASER));
	return !IsMarkedForDestroy() && !m_Projectile && m_SpawnTick == -1 && 
		Supported && g_pData->m_aPickups[m_Type].m_Respawntime >= 0;
}

CEntityDropPickup* CPickup::MakeToDrop()
{
	if(!CanMakeDrop())
		return nullptr;

	auto* pDrop = new CEntityDropPickup(GameWorld(), m_Pos, m_Type, m_SubType);
	ScheduleRespawn();
	return pDrop;
}

void CPickup::ScheduleRespawn()
{
	const int Seconds = g_pData->m_aPickups[m_Type].m_Respawntime;
	if(Seconds >= 0)
		m_SpawnTick = Server()->Tick() + maximum(1, Server()->TickSpeed() * Seconds);
}

bool CPickup::ApplyTo(CCharacter* pChar, int Type, int Subtype)
{
	if(!pChar || !pChar->IsAlive() || pChar->GetPlayer()->IsBot())
		return false;

	auto* pPlayer = pChar->GetPlayer();
	int Sound = SOUND_PICKUP_HEALTH;
	bool Picked = false;

	if (Type == POWERUP_HEALTH)
	{
		Picked = pChar->IncreaseHealth(translate_to_percent_rest(pPlayer->GetMaxHealth(), 1));
	}
	else if (Type == POWERUP_ARMOR)
	{
		Picked = pChar->IncreaseMana(translate_to_percent_rest(pPlayer->GetMaxMana(), 1));
		Sound = SOUND_PICKUP_ARMOR;
	}

	else if (Type == POWERUP_WEAPON && (Subtype == WEAPON_SHOTGUN || Subtype == WEAPON_GRENADE || Subtype == WEAPON_LASER))
	{
		const int Ammo = 10 + pPlayer->GetTotalAttributeValue(AttributeIdentifier::Ammo);
		Picked = pChar->GiveWeapon(Subtype, translate_to_percent_rest(Ammo, 40));
		Sound = Subtype == WEAPON_GRENADE ? SOUND_PICKUP_GRENADE : SOUND_PICKUP_SHOTGUN;
	}
	
	if(Picked)
		pChar->GS()->CreateSound(pChar->GetPos(), Sound);
	
	return Picked;
}

void CPickup::Tick()
{
	if(m_SpawnTick != -1)
	{
		if(Server()->Tick() < m_SpawnTick)
			return;

		m_SpawnTick = -1;
		if(m_Type == POWERUP_ARMOR_GRENADE || m_Type == POWERUP_ARMOR_SHOTGUN || m_Type == POWERUP_ARMOR_LASER)
			GS()->CreateSound(m_Pos, SOUND_WEAPON_SPAWN);
	}

	if(!HasPlayersInView() || m_Projectile)
		return;
	
	auto* pChar = static_cast<CCharacter*>(GameWorld()->ClosestEntity(m_Pos, 20.f, CGameWorld::ENTTYPE_CHARACTER, nullptr));
	if(ApplyTo(pChar, m_Type, m_SubType))
		ScheduleRespawn();
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
