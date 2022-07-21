/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "pickup.h"

#include <game/server/gamecontext.h>
#include <generated/server_data.h>
#include "character.h"

CPickup::CPickup(CGameWorld *pGameWorld, int Type, vec2 Pos)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_PICKUP, Pos, PickupPhysSize)
{
	m_Type = Type;

	CPickup::Reset();
	GameWorld()->InsertEntity(this);
}

void CPickup::Reset()
{
	if (g_pData->m_aPickups[m_Type].m_Spawndelay > 0)
		m_SpawnTick = Server()->Tick() + Server()->TickSpeed() * g_pData->m_aPickups[m_Type].m_Spawndelay;
	else
		m_SpawnTick = -1;
}

void CPickup::Tick()
{
	// wait for respawn
	if(m_SpawnTick > 0)
	{
		if(Server()->Tick() > m_SpawnTick)
		{
			m_SpawnTick = -1;
			if(m_Type == PICKUP_GRENADE || m_Type == PICKUP_SHOTGUN || m_Type == PICKUP_LASER)
				GS()->CreateSound(m_Pos, SOUND_WEAPON_SPAWN);
		}
		else
			return;
	}

	CCharacter *pChr = (CCharacter *)GS()->m_World.ClosestEntity(m_Pos, 20.0f, CGameWorld::ENTTYPE_CHARACTER, 0);
	if(!pChr || !pChr->IsAlive() || pChr->GetPlayer()->IsBot())
		return;

	bool Picked = false;
	if(m_Type == PICKUP_HEALTH)
	{
		const int RestoreHealth = translate_to_percent_rest(pChr->GetPlayer()->GetStartHealth(), 1);
		if(pChr->IncreaseHealth(RestoreHealth))
		{
			GS()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH);
			Picked = true;
		}
	}
	else if(m_Type == PICKUP_ARMOR)
	{
		const int RestoreMana = translate_to_percent_rest(pChr->GetPlayer()->GetStartMana(), 1);
		if(pChr->IncreaseMana(RestoreMana))
		{
			GS()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH);
			Picked = true;
		}
	}
	else if(m_Type == PICKUP_SHOTGUN)
	{
		const int RealAmmo = 10 + pChr->GetPlayer()->GetAttributeCount(Stats::StAmmo);
		const int RestoreAmmo = translate_to_percent_rest(RealAmmo, 40);
		if(pChr->GiveWeapon(WEAPON_SHOTGUN, RestoreAmmo))
		{
			Picked = true;
			GS()->CreateSound(m_Pos, SOUND_PICKUP_SHOTGUN);
		}
	}
	else if(m_Type == PICKUP_GRENADE)
	{
		const int RealAmmo = 10 + pChr->GetPlayer()->GetAttributeCount(Stats::StAmmo);
		const int RestoreAmmo = translate_to_percent_rest(RealAmmo, 40);
		if(pChr->GiveWeapon(WEAPON_GRENADE, RestoreAmmo))
		{
			Picked = true;
			GS()->CreateSound(m_Pos, SOUND_PICKUP_GRENADE);

		}
	}
	else if(m_Type == PICKUP_LASER)
	{
		const int RealAmmo = 10 + pChr->GetPlayer()->GetAttributeCount(Stats::StAmmo);
		const int RestoreAmmo = translate_to_percent_rest(RealAmmo, 40);
		if(pChr->GiveWeapon(WEAPON_LASER, RestoreAmmo))
		{
			Picked = true;
			GS()->CreateSound(m_Pos, SOUND_PICKUP_SHOTGUN);
		}
	}

	if(Picked)
	{
		int RespawnTime = g_pData->m_aPickups[m_Type].m_Respawntime;
		if(RespawnTime >= 0)
			m_SpawnTick = Server()->Tick() + Server()->TickSpeed() * RespawnTime;
	}
}

void CPickup::TickPaused()
{
	if(m_SpawnTick != -1)
		++m_SpawnTick;
}

void CPickup::Snap(int SnappingClient)
{
	if(m_SpawnTick != -1 || NetworkClipped(SnappingClient))
		return;

	CNetObj_Pickup *pP = static_cast<CNetObj_Pickup *>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, GetID(), sizeof(CNetObj_Pickup)));
	if(!pP)
		return;

	pP->m_X = (int)m_Pos.x;
	pP->m_Y = (int)m_Pos.y;
	pP->m_Type = m_Type;
}
