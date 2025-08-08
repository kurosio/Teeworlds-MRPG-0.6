﻿/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "character.h"
#include <generated/server_data.h>

#include <game/mapitems.h>
#include <game/server/entity_manager.h>
#include <game/server/gamecontext.h>

#include "laser.h"
#include "projectile.h"

#include <game/server/core/components/Bots/BotData.h>
#include <game/server/core/components/groups/group_data.h>
#include <game/server/core/components/guilds/guild_manager.h>
#include <game/server/core/components/quests/quest_manager.h>
#include <game/server/core/components/worlds/world_data.h>

#include <game/server/core/entities/group/entitiy_group.h>
#include <game/server/core/entities/items/gathering_node.h>
#include <game/server/core/entities/items/fishing_rod.h>
#include <game/server/core/entities/tools/multiple_orbit.h>
#include <game/server/core/entities/tools/flying_point.h>
#include "character_bot.h"

// weapons
#include <game/server/core/entities/weapons/grenade_pizdamet.h>
#include <game/server/core/entities/weapons/rifle_magneticpulse.h>
#include <game/server/core/entities/weapons/rifle_wallpusher.h>
#include <game/server/core/entities/weapons/rifle_tesla_serpent.h>
#include <game/server/core/entities/weapons/rifle_trackedplazma.h>

MACRO_ALLOC_POOL_ID_IMPL(CCharacter, MAX_CLIENTS* ENGINE_MAX_WORLDS + MAX_CLIENTS)

CCharacter::CCharacter(CGameWorld* pWorld)
	: CEntity(pWorld, CGameWorld::ENTTYPE_CHARACTER, vec2(0, 0), ms_PhysSize)
{
	m_pTilesHandler = new CTileHandler(pWorld->GS()->Collision(), this);
}

CCharacter::~CCharacter()
{
	delete m_pTilesHandler;
	m_pMultipleOrbit = nullptr;
	GS()->m_World.m_Core.m_apCharacters[m_pPlayer->GetCID()] = nullptr;
}

bool CCharacter::Spawn(CPlayer* pPlayer, vec2 Pos)
{
	m_pPlayer = pPlayer;
	m_ClientID = pPlayer->GetCID();
	m_pPlayer->m_aPlayerTick[LastDamage] = 0;

	m_Mana = 0;
	m_EmoteStop = -1;
	m_LastAction = -1;
	m_LastNoAmmoSound = -1;
	m_LastWeapon = WEAPON_HAMMER;
	m_QueuedWeapon = -1;
	m_WaterAir = GetMaxWaterAir();

	m_Pos = Pos;
	m_PrevPos = Pos;
	m_Core.Reset();
	m_Core.Init(&GS()->m_World.m_Core, GS()->Collision());
	m_Core.m_ActiveWeapon = WEAPON_HAMMER;
	m_Core.m_Pos = m_Pos;
	m_Core.m_DamageDisabled = false;
	m_Core.m_CollisionDisabled = false;
	m_Core.m_WorldID = m_pPlayer->GetCurrentWorldID();
	m_ReckoningTick = 0;
	m_SendCore = {};
	m_NumInputs = 0;
	GS()->m_World.m_Core.m_apCharacters[m_ClientID] = &m_Core;
	GS()->m_World.InsertEntity(this);
	m_Alive = true;

	if(!m_pPlayer->IsBot())
	{
		m_pPlayer->m_MoodState = m_pPlayer->GetMoodState();

		GS()->Core()->QuestManager()->Update(m_pPlayer);
		GS()->Core()->QuestManager()->TryAcceptNextQuestChainAll(m_pPlayer);

		m_pPlayer->m_VotesData.UpdateCurrentVotes();
		GS()->MarkUpdatedBroadcast(m_ClientID);

		const bool Spawned = GS()->m_pController->OnCharacterSpawn(this);
		return Spawned;
	}

	return true;
}

void CCharacter::SetWeapon(int Weapon)
{
	if(Weapon == m_Core.m_ActiveWeapon)
		return;

	m_LastWeapon = m_Core.m_ActiveWeapon;
	m_QueuedWeapon = -1;
	m_Core.m_ActiveWeapon = Weapon;
	GS()->CreateSound(m_Pos, SOUND_WEAPON_SWITCH);

	if(m_Core.m_ActiveWeapon < 0 || m_Core.m_ActiveWeapon >= NUM_WEAPONS)
		m_Core.m_ActiveWeapon = 0;
	m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_AmmoRegenStart = -1;
}

bool CCharacter::IsGrounded() const
{
	if(GS()->Collision()->CheckPoint(m_Pos.x + GetRadius() / 2, m_Pos.y + GetRadius() / 2 + 5))
		return true;
	if(GS()->Collision()->CheckPoint(m_Pos.x - GetRadius() / 2, m_Pos.y + GetRadius() / 2 + 5))
		return true;

	int MoveRestrictionsBelow = GS()->Collision()->GetMoveRestrictions(m_Pos + vec2(0, GetRadius() / 2 + 4), 0.f);
	return (MoveRestrictionsBelow & CANTMOVE_DOWN) != 0;
}

bool CCharacter::IsCollisionFlag(int Flag) const
{
	if(GS()->Collision()->CheckPoint(m_Pos.x + GetRadius() / 2, m_Pos.y + GetRadius() / 2 + 10, Flag))
		return true;
	if(GS()->Collision()->CheckPoint(m_Pos.x - GetRadius() / 2, m_Pos.y + GetRadius() / 2 + 10, Flag))
		return true;
	if(GS()->Collision()->CheckPoint(m_Pos.x + GetRadius() / 2, m_Pos.y - GetRadius() / 2 + 10, Flag))
		return true;
	if(GS()->Collision()->CheckPoint(m_Pos.x - GetRadius() / 2, m_Pos.y - GetRadius() / 2 + 10, Flag))
		return true;
	return false;
}

CPlayer* CCharacter::GetHookedPlayer() const
{
	if(m_Core.m_HookState == HOOK_GRABBED)
		return GS()->GetPlayer(m_Core.m_HookedPlayer);
	return nullptr;
}

void CCharacter::SetDoorHit(int ID)
{
	m_Core.m_vDoorHitSet.insert(ID);
}

void CCharacter::DoWeaponSwitch()
{
	// make sure we can switch
	if(m_ReloadTimer != 0 || m_QueuedWeapon == -1 || m_Core.m_aWeapons[WEAPON_NINJA].m_Got)
		return;

	// switch Weapon
	SetWeapon(m_QueuedWeapon);
}

void CCharacter::HandleWeaponSwitch()
{
	int WantedWeapon = m_Core.m_ActiveWeapon;
	if(m_QueuedWeapon != -1)
		WantedWeapon = m_QueuedWeapon;

	// select Weapon
	int Next = CountInput(m_LatestPrevInput.m_NextWeapon, m_LatestInput.m_NextWeapon).m_Presses;
	int Prev = CountInput(m_LatestPrevInput.m_PrevWeapon, m_LatestInput.m_PrevWeapon).m_Presses;

	if(Next < 128) // make sure we only try sane stuff
	{
		while(Next) // Next Weapon selection
		{
			WantedWeapon = (WantedWeapon + 1) % NUM_WEAPONS;
			if(m_Core.m_aWeapons[WantedWeapon].m_Got)
				Next--;
		}
	}

	if(Prev < 128) // make sure we only try sane stuff
	{
		while(Prev) // Prev Weapon selection
		{
			WantedWeapon = (WantedWeapon - 1) < 0 ? NUM_WEAPONS - 1 : WantedWeapon - 1;
			if(m_Core.m_aWeapons[WantedWeapon].m_Got)
				Prev--;
		}
	}

	// Direct Weapon selection
	if(m_LatestInput.m_WantedWeapon)
		WantedWeapon = m_Input.m_WantedWeapon - 1;

	// check for insane values
	if(WantedWeapon >= 0 && WantedWeapon < NUM_WEAPONS && WantedWeapon != m_Core.m_ActiveWeapon && m_Core.m_aWeapons[WantedWeapon].m_Got)
		m_QueuedWeapon = WantedWeapon;

	DoWeaponSwitch();
}

void CCharacter::HandleReload()
{
	m_ReloadTimer = 125 * Server()->TickSpeed() / 1000;
	if(m_LastNoAmmoSound + Server()->TickSpeed() <= Server()->Tick())
	{
		GS()->CreateSound(m_Pos, SOUND_WEAPON_NOAMMO);
		m_LastNoAmmoSound = Server()->Tick();
	}
}

void CCharacter::FireWeapon()
{
	if(m_ReloadTimer != 0)
		return;

	DoWeaponSwitch();

	// Check if the character has learned the skill for using weapons in full auto mode
	const bool IsCharBot = m_pPlayer->IsBot();
	bool FullAuto = (IsCharBot || m_pPlayer->GetSkill(SKILL_MASTER_WEAPON)->IsLearned());
	bool WillFire = CountInput(m_LatestPrevInput.m_Fire, m_LatestInput.m_Fire).m_Presses;
	if(FullAuto && (m_LatestInput.m_Fire & 1) && m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo)
	{
		WillFire = true;
	}

	// If the player will not fire, return without taking any action
	if(!WillFire)
	{
		return;
	}

	// Check if the player is not a bot
	if(!IsCharBot)
	{
		// fishing mode
		if(m_pTilesHandler->IsActive(TILE_FISHING_MODE))
		{
			if(m_pFishingRod && m_pFishingRod->IsWaitingState())
				delete m_pFishingRod;

			if(!m_pFishingRod)
			{
				const auto ForceX = m_LatestInput.m_TargetX * 0.08f;
				const auto ForceY = m_LatestInput.m_TargetY * 0.08f;
				m_pFishingRod = new CEntityFishingRod(&GS()->m_World, m_ClientID, m_Core.m_Pos, vec2(ForceX, ForceY));
				m_ReloadTimer = Server()->TickSpeed();
			}

			return;
		}

		// Check if the active weapon has no ammo
		if(!m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo)
		{
			HandleReload();
			return;
		}
	}

	// fire by weapon
	const vec2 MouseTarget = vec2(m_LatestInput.m_TargetX, m_LatestInput.m_TargetY);
	const vec2 Direction = normalize(MouseTarget);
	const vec2 ProjStartPos = m_Pos + Direction * GetRadius() * 0.75f;
	const auto TotalWeaponDamage = GetTotalDamageByWeapon(m_Core.m_ActiveWeapon);
	switch(m_Core.m_ActiveWeapon)
	{
		case WEAPON_HAMMER: FireHammer(Direction, ProjStartPos, TotalWeaponDamage); break;
		case WEAPON_GUN: FireGun(Direction, ProjStartPos, TotalWeaponDamage); break;
		case WEAPON_SHOTGUN: FireShotgun(Direction, ProjStartPos, TotalWeaponDamage); break;
		case WEAPON_GRENADE: FireGrenade(Direction, ProjStartPos, TotalWeaponDamage); break;
		case WEAPON_LASER: FireRifle(Direction, ProjStartPos, TotalWeaponDamage); break;
		case WEAPON_NINJA:
		{
			m_Ninja.m_ActivationDir = Direction;
			m_Ninja.m_CurrentMoveTime = g_pData->m_Weapons.m_Ninja.m_Movetime * Server()->TickSpeed() / 1000;
			m_Ninja.m_OldVelAmount = length(m_Core.m_Vel);
			GS()->CreateSound(m_Pos, SOUND_NINJA_FIRE);
		}
		break;
	}

	m_AttackTick = Server()->Tick();

	if(m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo > 0)
		m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo--;

	if(!m_ReloadTimer)
	{
		constexpr int DefaultSpeed = 1000;
		const auto SpeedMultiplier = m_pPlayer->GetTotalAttributeChance(AttributeIdentifier::AttackSPD).value_or(100.f) / 100.f;
		const auto Speed = (float)DefaultSpeed * SpeedMultiplier;
		m_ReloadTimer = g_pData->m_Weapons.m_aId[m_Core.m_ActiveWeapon].m_Firedelay * Server()->TickSpeed() / round_to_int(Speed);
	}
}

bool CCharacter::FireHammer(vec2 Direction, vec2 ProjStartPos, int TotalWeaponDamage)
{
	constexpr int MAX_LENGTH_CHARACTERS = 16;
	const bool IsBot = m_pPlayer->IsBot();

	// check equip state
	const auto EquippedItemIdOpt = m_pPlayer->GetEquippedSlotItemID(ItemType::EquipHammer);
	if(!EquippedItemIdOpt.has_value())
	{
		GS()->Broadcast(m_pPlayer->GetCID(), BroadcastPriority::GameWarning, 2, "You don't have a hammer equipped.");
		return false;
	}

	// handle hammer actions
	if(HandleHammerActions(Direction, ProjStartPos))
	{
		m_ReloadTimer = Server()->TickSpeed() / 3;
		return true;
	}

	// lamp hammer
	if(EquippedItemIdOpt == itHammerLamp)
	{
		const auto vEntities = GS()->m_World.FindEntities(ProjStartPos, 400.f , MAX_LENGTH_CHARACTERS, CGameWorld::ENTTYPE_CHARACTER);
		for(auto* pEnt : vEntities)
		{
			// skip self damage
			auto* pTarget = dynamic_cast<CCharacter*>(pEnt);
			if(!pTarget || m_ClientID == pTarget->GetClientID())
				continue;

			// check intersect line
			if(GS()->Collision()->IntersectLineWithInvisible(ProjStartPos, pTarget->m_Pos, nullptr, nullptr))
				continue;

			if(!pTarget->IsAllowedPVP(m_ClientID))
				continue;

			const auto Dir = length(pTarget->m_Pos - m_Pos) > 0.0f ? normalize(pTarget->m_Pos - m_Pos) : vec2(0.f, -1.f);
			const auto Force = vec2(0.f, -1.f) + normalize(Dir + vec2(0.f, -1.1f)) * 10.0f;

			// create flying point
			auto* pPoint = new CEntityFlyingPoint(&GS()->m_World, ProjStartPos, Force, pTarget->GetClientID(), m_ClientID);
			pPoint->Register([this, Force, TotalWeaponDamage](CPlayer* pFrom, CPlayer* pPlayer)
			{
				auto* pChar = pPlayer->GetCharacter();
				GS()->CreateDeath(pChar->GetPos(), pPlayer->GetCID());
				pChar->TakeDamage(Force, TotalWeaponDamage, pFrom->GetCID(), WEAPON_HAMMER);
			});

			// reload
			m_ReloadTimer = Server()->TickSpeed() / 3;
		}

		GS()->CreateSound(m_Pos, SOUND_HAMMER_FIRE);
		return true;
	}

	// blast hammer
	if(EquippedItemIdOpt == itHammerBlast)
	{
		constexpr float Radius = 128.0f;

		// apply damage and force to all nearby characters
		for(auto* pTarget = (CCharacter*)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pTarget; pTarget = (CCharacter*)pTarget->TypeNext())
		{
			// skip self damage
			if(m_ClientID == pTarget->GetClientID())
				continue;

			if(!pTarget->IsAllowedPVP(m_ClientID))
				continue;

			const auto Dist = distance(pTarget->m_Pos, m_Pos);
			if(Dist < Radius)
			{
				GS()->CreateExplosion(pTarget->GetPos(), m_ClientID, WEAPON_HAMMER, TotalWeaponDamage);
			}
		}

		// move and visual effect
		AddVelocity(Direction * 2.5f);
		GS()->CreateExplosion(m_Pos, m_ClientID, WEAPON_HAMMER, TotalWeaponDamage);
		GS()->CreateSound(m_Pos, SOUND_WEAPONS_HAMMER_BLAST_START);
		return true;
	}

	// default hammer
	const float Radius = m_pPlayer->GetItem(itBasicHammerPlus)->IsEquipped() ? 6.4f : 2.4f;
	const auto vEntities = GS()->m_World.FindEntities(ProjStartPos, GetRadius() * Radius, MAX_LENGTH_CHARACTERS, CGameWorld::ENTTYPE_CHARACTER);
	for(auto* pEnt : vEntities)
	{
		// skip self damage
		auto* pTarget = dynamic_cast<CCharacter*>(pEnt);
		if(!pTarget || m_ClientID == pTarget->GetClientID())
			continue;

		if(GS()->Collision()->IntersectLineWithInvisible(ProjStartPos, pTarget->m_Pos, nullptr, nullptr))
			continue;

		if(!pTarget->IsAllowedPVP(m_ClientID))
			continue;

		const auto Dir = length(pTarget->m_Pos - m_Pos) > 0.0f
			? normalize(pTarget->m_Pos - m_Pos) : vec2(0.f, -1.f);
		const auto Force = vec2(0.f, -1.f) + normalize(Dir + vec2(0.f, -1.1f)) * (IsBot ? 5.0f : 10.0f);

		GS()->CreateHammerHit(pTarget->m_Pos);
		pTarget->TakeDamage(Force, TotalWeaponDamage, m_ClientID, WEAPON_HAMMER);
		m_ReloadTimer = Server()->TickSpeed() / 3;
	}

	GS()->CreateSound(m_Pos, SOUND_HAMMER_FIRE);
	return true;
}

bool CCharacter::FireGun(vec2 Direction, vec2 ProjStartPos, int TotalWeaponDamage)
{
	// check equip state
	const auto EquippedItemIdOpt = m_pPlayer->GetEquippedSlotItemID(ItemType::EquipGun);
	if(!EquippedItemIdOpt.has_value())
	{
		GS()->Broadcast(m_pPlayer->GetCID(), BroadcastPriority::GameWarning, 2, "You don't have a gun equipped.");
		return false;
	}

	// gun pulse
	if(EquippedItemIdOpt == itGunPulse)
	{
		new CLaser(GameWorld(), m_ClientID, TotalWeaponDamage, m_Pos, Direction, 400.f, true);
		GS()->CreateSound(m_Pos, SOUND_WEAPONS_GUN_PULSE_START);
		return true;
	}

	// default gun
	const auto MouseTarget = vec2(m_LatestInput.m_TargetX, m_LatestInput.m_TargetY);
	const auto Lifetime = (int)(Server()->TickSpeed() * GS()->Tuning()->m_GunLifetime);
	const auto ExplodeModule = m_pPlayer->GetItem(itExplosiveGun)->IsEquipped();
	new CProjectile(GameWorld(), WEAPON_GUN, m_pPlayer->GetCID(), ProjStartPos,
		Direction, Lifetime, ExplodeModule, 0, -1, MouseTarget, WEAPON_GUN);
	GS()->CreateSound(m_Pos, SOUND_GUN_FIRE);
	return true;
}

bool CCharacter::FireShotgun(vec2 Direction, vec2 ProjStartPos, int Damage)
{
	// check equip state
	const auto EquippedItemIdOpt = m_pPlayer->GetEquippedSlotItemID(ItemType::EquipShotgun);
	if(!EquippedItemIdOpt.has_value())
	{
		GS()->Broadcast(m_pPlayer->GetCID(), BroadcastPriority::GameWarning, 2, "You don't have a shotgun equipped.");
		return false;
	}

	// default shotgun
	constexpr int ShotSpread = 5;
	const int Lifetime = (int)(Server()->TickSpeed() * GS()->Tuning()->m_ShotgunLifetime);
	const bool IsExplosive = m_pPlayer->GetItem(itExplosiveShotgun)->IsEquipped();
	for(int i = 0; i < ShotSpread; ++i)
	{
		const float Spreading = ((0.0058945f * (9.0f * ShotSpread) / 2)) - (0.0058945f * (9.0f * i));
		const float a = angle(Direction) + Spreading;
		const float Speed = (float)GS()->Tuning()->m_ShotgunSpeeddiff + random_float(0.2f);
		vec2 TargetPos = Direction + vec2(cosf(a), sinf(a)) * (Speed * 500.0f);

		new CProjectile(GameWorld(),
			WEAPON_SHOTGUN,
			m_pPlayer->GetCID(),
			ProjStartPos,
			vec2(cosf(a), sinf(a)) * Speed,
			Lifetime,
			IsExplosive,
			0,
			15,
			TargetPos,
			WEAPON_SHOTGUN);
	}

	GS()->CreateSound(m_Pos, SOUND_SHOTGUN_FIRE);
	return true;
}

bool CCharacter::FireGrenade(vec2 Direction, vec2 ProjStartPos, int Damage)
{
	// check equip state
	const auto EquippedItemIdOpt = m_pPlayer->GetEquippedSlotItemID(ItemType::EquipGrenade);
	if(!EquippedItemIdOpt.has_value())
	{
		GS()->Broadcast(m_pPlayer->GetCID(), BroadcastPriority::GameWarning, 2, "You don't have a grenade equipped.");
		return false;
	}

	// pizdamet
	if(EquippedItemIdOpt == itPizdamet)
	{
		new CEntityGrenadePizdamet(&GS()->m_World, m_ClientID, ProjStartPos, Direction);
		m_ReloadTimer = Server()->TickSpeed() / 8;
		GS()->CreateSound(m_Pos, SOUND_WEAPONS_PIZDAMET_START);
		return true;
	}

	// default grenade
	const auto MouseTarget = vec2(m_LatestInput.m_TargetX, m_LatestInput.m_TargetY);
	new CProjectile(GameWorld(), WEAPON_GRENADE, m_pPlayer->GetCID(), ProjStartPos,
		Direction, (int)(Server()->TickSpeed() * GS()->Tuning()->m_GrenadeLifetime),
		true, 0, SOUND_GRENADE_EXPLODE, MouseTarget, WEAPON_GRENADE);
	GS()->CreateSound(m_Pos, SOUND_GRENADE_FIRE);
	return true;
}

bool CCharacter::FireRifle(vec2 Direction, vec2 ProjStartPos, int TotalWeaponDamage)
{
	// check equip state
	const auto EquippedItemIdOpt = m_pPlayer->GetEquippedSlotItemID(ItemType::EquipLaser);
	if(!EquippedItemIdOpt.has_value())
	{
		GS()->Broadcast(m_pPlayer->GetCID(), BroadcastPriority::GameWarning, 2, "You don't have a laser equipped.");
		return false;
	}

	// plazma wall
	if(EquippedItemIdOpt == itRifleWallPusher)
	{
		const auto LifeTime = 5 * Server()->TickSpeed();
		new CEntityRifleWallPusher(&GS()->m_World, m_ClientID, ProjStartPos, Direction, LifeTime);
		return true;
	}

	// Magnetic pulse rifle
	if(EquippedItemIdOpt == itRifleMagneticPulse)
	{
		new CEntityRifleMagneticPulse(&GS()->m_World, m_ClientID, 128.f, ProjStartPos, Direction);
		return true;
	}

	// Plazma
	if(EquippedItemIdOpt == itRifleTrackedPlazma)
	{
		new CEntityRifleTrackedPlazma(&GS()->m_World, m_ClientID, ProjStartPos, Direction);
		GS()->CreateSound(m_Pos, SOUND_WEAPONS_TRACKED_PLAZMA_START);
		return true;
	}

	// Tesla serpen
	if(EquippedItemIdOpt == itRifleTeslaSerpent)
	{
		new CEntityTeslaSerpent(&GS()->m_World, m_ClientID, ProjStartPos, Direction, TotalWeaponDamage, 400.f, 3, 0.5f);
		GS()->CreateSound(m_Pos, SOUND_LASER_FIRE);
		return true;
	}

	// default laser
	new CLaser(&GS()->m_World, m_ClientID, TotalWeaponDamage, ProjStartPos, Direction, GS()->Tuning()->m_LaserReach, false);
	GS()->CreateSound(m_Pos, SOUND_LASER_FIRE);
	return true;
}

void CCharacter::HandleWeapons()
{
	HandleNinja();

	if(m_ReloadTimer)
	{
		m_ReloadTimer--;
		return;
	}

	FireWeapon();

	if(m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo >= 0)
	{
		{
			constexpr int DefaultSpeed = 100;
			const auto SpeedMultiplier = m_pPlayer->GetTotalAttributeChance(AttributeIdentifier::AmmoRegen).value_or(100.f) / 100.f;
			const auto Speed = (float)DefaultSpeed * SpeedMultiplier;
			const auto AmmoRegenTime = 500 / round_to_int(Speed);
			if(m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_AmmoRegenStart < 0)
				m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_AmmoRegenStart = Server()->Tick() +
				(m_Core.m_ActiveWeapon == (int)WEAPON_GUN ? (Server()->TickSpeed() / 2) : (AmmoRegenTime * Server()->TickSpeed()));
		}


		if(m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_AmmoRegenStart <= Server()->Tick())
		{
			const int RealAmmo = 10 + m_pPlayer->GetTotalAttributeValue(AttributeIdentifier::Ammo);
			m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo = minimum(m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo + 1, RealAmmo);
			m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_AmmoRegenStart = -1;
		}
	}
}

void CCharacter::HandleNinja()
{
	if(m_Core.m_ActiveWeapon != WEAPON_NINJA)
		return;

	m_Ninja.m_CurrentMoveTime--;
	if(m_Ninja.m_CurrentMoveTime == 0)
	{
		// reset velocity
		m_Core.m_Vel = m_Ninja.m_ActivationDir * m_Ninja.m_OldVelAmount;
	}
	else if(m_Ninja.m_CurrentMoveTime > 0)
	{
		// Set velocity
		m_Core.m_Vel = m_Ninja.m_ActivationDir * g_pData->m_Weapons.m_Ninja.m_Velocity;
		GS()->Collision()->MoveBox(&m_Core.m_Pos, &m_Core.m_Vel, vec2(GetRadius(), GetRadius()), 0.f);

		// reset velocity so the client doesn't predict stuff
		m_Core.m_Vel = vec2(0.f, 0.f);

		const float Radius = GetRadius() * 2.0f;
		for(CCharacter* pChar = (CCharacter*)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
		{
			if(distance(pChar->m_Core.m_Pos, m_Core.m_Pos) < Radius)
			{
				if(pChar->GetPlayer()->GetCID() != m_ClientID && pChar->IsAllowedPVP(m_ClientID))
					pChar->TakeDamage(vec2(0, -10.0f), GetTotalDamageByWeapon(WEAPON_NINJA), m_ClientID, WEAPON_NINJA);
			}
		}
	}
}

void CCharacter::HandleHookActions()
{
	if(!m_Alive)
		return;

	CPlayer* pHookedPlayer = GetHookedPlayer();
	if(pHookedPlayer && pHookedPlayer->GetCharacter())
	{
		// poison hook :: damage increase with hammer damage
		if(Server()->Tick() % (Server()->TickSpeed() / 2) == 0)
		{
			if(m_pPlayer->GetItem(itPoisonHook)->IsEquipped())
			{
				const auto DmgSize = maximum(1, translate_to_percent_rest(m_pPlayer->GetTotalAttributeValue(AttributeIdentifier::DMG), 5));
				pHookedPlayer->GetCharacter()->TakeDamage({}, DmgSize, m_ClientID, WEAPON_GAME);
			}
		}
	}
	else
	{
		// spider hook
		if(m_Core.m_HookState == HOOK_FLYING && m_pPlayer->GetItem(itSpiderHook)->IsEquipped())
		{
			float Distance = minimum((float)m_pPlayer->m_NextTuningParams.m_HookLength - m_pPlayer->m_NextTuningParams.m_HookFireSpeed, distance(GetMousePos(), m_Core.m_Pos));
			if(distance(m_Core.m_Pos, m_Core.m_HookPos) > Distance)
			{
				m_Core.m_HookState = HOOK_GRABBED;
				m_Core.m_TriggeredEvents |= COREEVENT_HOOK_ATTACH_GROUND;
			}
		}
	}

	// explode hook
	if((m_Core.m_TriggeredEvents & COREEVENT_HOOK_ATTACH_GROUND && distance(m_Core.m_HookPos, m_Core.m_Pos) > 48.0f) || m_Core.m_TriggeredEvents & COREEVENT_HOOK_ATTACH_PLAYER)
	{
		if(m_pPlayer->GetItem(itExplodeHook)->IsEquipped())
		{
			GS()->CreateExplosion(m_Core.m_HookPos, m_ClientID, WEAPON_GUN, 0, FORCE_FLAG_CANT_SELF);
		}
	}
}

void CCharacter::AddMultipleOrbit(bool Projectile, int Amount, int Type, int Subtype, int OrbitType)
{
	if(!m_pMultipleOrbit)
	{
		m_pMultipleOrbit = new CMultipleOrbit(GameWorld(), this);
		m_pMultipleOrbit->SetClientID(m_pPlayer->GetCID());
	}

	m_pMultipleOrbit->Add(Projectile, Amount, Type, Projectile ? 0 : Subtype, OrbitType);
}

void CCharacter::RemoveMultipleOrbit(bool Projectile, int Amount, int Type, int Subtype, int OrbitType) const
{
	if(!m_pMultipleOrbit)
		return;

	m_pMultipleOrbit->Remove(Projectile, Amount, Type, Projectile ? 0 : Subtype, OrbitType);
}

bool CCharacter::GiveWeapon(int WeaponID, int Ammo)
{
	if(WeaponID < WEAPON_HAMMER || WeaponID > WEAPON_NINJA)
		return false;

	const bool IsWeaponHammer = WeaponID == WEAPON_HAMMER;
	const auto EquipID = GetEquipByWeapon(WeaponID);

	// remove is unequipped weapon
	if(!m_pPlayer->IsEquippedSlot(EquipID) && !IsWeaponHammer)
	{
		RemoveWeapon(WeaponID);
		return false;
	}

	// check max ammo
	const int MaxAmmo = 10 + m_pPlayer->GetTotalAttributeValue(AttributeIdentifier::Ammo);
	if(m_Core.m_aWeapons[WeaponID].m_Ammo >= MaxAmmo)
		return false;

	// calculate got ammo
	int GotAmmo;
	if(m_Core.m_aWeapons[WeaponID].m_Got)
	{
		GotAmmo = minimum(m_Core.m_aWeapons[WeaponID].m_Ammo + Ammo, MaxAmmo);
	}
	else
	{
		GotAmmo = minimum(Ammo, MaxAmmo);
	}

	// change weapon state
	m_Core.m_aWeapons[WeaponID].m_Got = true;
	m_Core.m_aWeapons[WeaponID].m_Ammo = IsWeaponHammer ? -1 : GotAmmo;
	return true;
}

bool CCharacter::RemoveWeapon(int WeaponID)
{
	if(!m_Core.m_aWeapons[WeaponID].m_Got || WeaponID < WEAPON_HAMMER || WeaponID > WEAPON_NINJA)
		return false;

	if(WeaponID == m_Core.m_ActiveWeapon)
		SetWeapon(m_Core.m_aWeapons[m_LastWeapon].m_Got ? m_LastWeapon : WEAPON_HAMMER);

	m_Core.m_aWeapons[WeaponID].m_Got = false;
	m_Core.m_aWeapons[WeaponID].m_Ammo = -1;
	return true;
}

// This function sets the character's emote and its duration
void CCharacter::SetEmoticonDDNet(int Emoticon, int Sec, bool SendEmoticon) {
    int EmoteType = EMOTE_NORMAL;
    switch(Emoticon)
    {
        case EMOTICON_EXCLAMATION:
        case EMOTICON_GHOST:
        case EMOTICON_QUESTION:
        case EMOTICON_WTF:
            EmoteType = EMOTE_SURPRISE;
            break;
        case EMOTICON_DOTDOT:
        case EMOTICON_DROP:
        case EMOTICON_ZZZ:
            EmoteType = EMOTE_BLINK;
            break;
        case EMOTICON_EYES:
        case EMOTICON_HEARTS:
        case EMOTICON_MUSIC:
            EmoteType = EMOTE_HAPPY;
            break;
        case EMOTICON_OOP:
        case EMOTICON_SORRY:
        case EMOTICON_SUSHI:
            EmoteType = EMOTE_PAIN;
            break;
        case EMOTICON_DEVILTEE:
        case EMOTICON_SPLATTEE:
        case EMOTICON_ZOMG:
            EmoteType = EMOTE_ANGRY;
            break;
        default:
            break;
    }

    if(g_Config.m_SvEmotionalTees)
    {
        m_EmoteType = EmoteType;
        m_EmoteStop = Server()->Tick() + Sec * Server()->TickSpeed();
    }

	if(SendEmoticon)
        GS()->SendEmoticon(m_pPlayer->GetCID(), Emoticon);
}

// This function sets the character's emote and its duration
void CCharacter::SetEmote(int Emote, int Sec, bool SendEmoticon)
{
	// Reset by default emote
	if(Emote == EMOTE_NORMAL)
	{
		m_EmoteType = EMOTE_NORMAL;
		m_EmoteStop = -1;
		return;
	}

	// check if the character is alive and the emote has stopped
	if(m_EmoteStop < Server()->Tick())
	{
		m_EmoteType = Emote;
		m_EmoteStop = Server()->Tick() + Sec * Server()->TickSpeed();
	}

	// check if the Emoticon should be started
	if(SendEmoticon)
	{
		if(Emote == EMOTE_BLINK)
			GS()->SendEmoticon(m_pPlayer->GetCID(), EMOTICON_DOTDOT);
		else if(Emote == EMOTE_HAPPY)
			GS()->SendEmoticon(m_pPlayer->GetCID(), (rand() % 2 == 0 ? (int)EMOTICON_HEARTS : (int)EMOTICON_EYES));
		else if(Emote == EMOTE_ANGRY)
			GS()->SendEmoticon(m_pPlayer->GetCID(), (EMOTICON_SPLATTEE + rand() % 3));
		else if(Emote == EMOTE_PAIN)
			GS()->SendEmoticon(m_pPlayer->GetCID(), EMOTICON_DROP);
	}
}

void CCharacter::OnPredictedInput(CNetObj_PlayerInput* pNewInput)
{
	// check for changes
	if(mem_comp(&m_Input, pNewInput, sizeof(CNetObj_PlayerInput)) != 0)
		m_LastAction = Server()->Tick();

	// copy new input
	mem_copy(&m_Input, pNewInput, sizeof(m_Input));

	// it is not allowed to aim in the center
	if(m_Input.m_TargetX == 0 && m_Input.m_TargetY == 0)
		m_Input.m_TargetY = -1;
}

void CCharacter::OnDirectInput(CNetObj_PlayerInput* pNewInput)
{
	mem_copy(&m_LatestPrevInput, &m_LatestInput, sizeof(m_LatestInput));
	mem_copy(&m_LatestInput, pNewInput, sizeof(m_LatestInput));
	m_NumInputs++;

	// it is not allowed to aim in the center
	if(m_LatestInput.m_TargetX == 0 && m_LatestInput.m_TargetY == 0)
		m_LatestInput.m_TargetY = -1;

	if(m_NumInputs > 1 && m_pPlayer->GetTeam() != TEAM_SPECTATORS)
	{
		if(Server()->Input()->IsBlockedInputGroup(m_pPlayer->GetCID(), BLOCK_INPUT_FREEZE_HAMMER))
			m_Core.m_ActiveWeapon = WEAPON_HAMMER;
		else if(Server()->Input()->IsBlockedInputGroup(m_pPlayer->GetCID(), BLOCK_INPUT_FREEZE_GUN))
			m_Core.m_ActiveWeapon = WEAPON_GUN;
		else if(Server()->Input()->IsBlockedInputGroup(m_pPlayer->GetCID(), BLOCK_INPUT_FREEZE_SHOTGUN))
			m_Core.m_ActiveWeapon = WEAPON_SHOTGUN;
		else if(Server()->Input()->IsBlockedInputGroup(m_pPlayer->GetCID(), BLOCK_INPUT_FREEZE_GRENADE))
			m_Core.m_ActiveWeapon = WEAPON_GRENADE;
		else if(Server()->Input()->IsBlockedInputGroup(m_pPlayer->GetCID(), BLOCK_INPUT_FREEZE_LASER))
			m_Core.m_ActiveWeapon = WEAPON_LASER;
		else if(!Server()->Input()->IsBlockedInputGroup(m_pPlayer->GetCID(), BLOCK_INPUT_FREEZE_WEAPON))
			HandleWeaponSwitch();

		// Check if the input group for firing weapon is not blocked for the player
		if(Server()->Input()->IsBlockedInputGroup(m_pPlayer->GetCID(), BLOCK_INPUT_FIRE))
			m_ReloadTimer = 10;

		FireWeapon();
	}

	if(Server()->Input()->IsBlockedInputGroup(m_pPlayer->GetCID(), BLOCK_INPUT_HOOK))
	{
		CTuningParams* pTuningParams = &m_pPlayer->m_NextTuningParams;
		pTuningParams->m_HookLength = 0.0f;
		pTuningParams->m_HookFireSpeed = 0.0f;
		pTuningParams->m_HookDragSpeed = 0.0f;
		pTuningParams->m_HookDragAccel = 0.0f;
		ResetHook();
	}

	mem_copy(&m_LatestPrevInput, &m_LatestInput, sizeof(m_LatestInput));
}

void CCharacter::ResetHook()
{
	m_Core.m_HookedPlayer = -1;
	m_Core.m_HookState = HOOK_RETRACTED;
	m_Core.m_TriggeredEvents |= COREEVENT_HOOK_RETRACT;
	m_Core.m_HookPos = m_Core.m_Pos;
}

void CCharacter::ResetInput()
{
	m_Input.m_Direction = 0;
	m_Input.m_Hook = 0;
	if((m_Input.m_Fire & 1) != 0)
		m_Input.m_Fire++;
	m_Input.m_Fire &= INPUT_STATE_MASK;
	m_Input.m_Jump = 0;
	m_LatestPrevInput = m_LatestInput = m_Input;
}

void CCharacter::Tick()
{
	if(!m_Alive)
		return;

	// check world access
	if(!CanAccessWorld())
		return;

	// to end the tick on the destroy caused by the change of worlds
	if(m_pTilesHandler->IsEnter(TILE_WORLD_SWAPPER))
	{
		GS()->GetWorldData()->Move(m_pPlayer);
		return;
	}

	// pre tick
	HandleTuning();

	// core
	m_Core.m_Input = m_Input;
	m_Core.Tick(true, &m_pPlayer->m_NextTuningParams);
	m_pPlayer->UpdateSharedCharacterData(m_Health, m_Mana);

	// post tick
	HandleSafeFlags();
	HandlePlayer();
	HandleWeapons();
	if(!HandleTiles())
		return;

	// game clipped
	if(GameLayerClipped(m_Pos) || m_pTilesHandler->IsEnter(TILE_DEATH))
	{
		Die(m_pPlayer->GetCID(), WEAPON_SELF);
		return;
	}

	// prev pos apply move restriction
	ApplyMoveRestrictions();
}

void CCharacter::TickDeferred()
{
	if(!m_Alive)
		return;

	// advance the dummy
	{
		CWorldCore TempWorld;
		CCharacterCore::CParams CoreTickParams(&GameWorld()->m_Core.m_Tuning);
		m_ReckoningCore.Init(&TempWorld, GS()->Collision());
		m_ReckoningCore.Tick(false, &CoreTickParams);
		m_ReckoningCore.Move(&CoreTickParams);
		m_ReckoningCore.Quantize();
	}

	CCharacterCore::CParams CoreTickParams(&m_pPlayer->m_NextTuningParams);
	m_Core.Move(&CoreTickParams);
	m_Core.Quantize();
	m_PrevPos = m_Pos;
	m_Pos = m_Core.m_Pos;
	m_TriggeredEvents |= m_Core.m_TriggeredEvents;

	if(m_TriggeredEvents & COREEVENT_HOOK_ATTACH_PLAYER)
		GS()->CreateSound(m_Pos, SOUND_HOOK_ATTACH_PLAYER);

	if(m_pPlayer->GetTeam() == TEAM_SPECTATORS)
	{
		m_Pos.x = m_Input.m_TargetX;
		m_Pos.y = m_Input.m_TargetY;
	}
	//else if(m_Core.m_Death)
	//	Die(m_pPlayer->GetCID(), WEAPON_SELF);

	// update the m_SendCore if needed
	{
		CNetObj_Character Predicted;
		CNetObj_Character Current;
		mem_zero(&Predicted, sizeof(Predicted));
		mem_zero(&Current, sizeof(Current));
		m_ReckoningCore.Write(&Predicted);
		m_Core.Write(&Current);

		// only allow dead reackoning for a top of 3 seconds
		if(m_ReckoningTick + Server()->TickSpeed() * 3 < Server()->Tick() || mem_comp(&Predicted, &Current, sizeof(CNetObj_Character)) != 0)
		{
			m_ReckoningTick = Server()->Tick();
			m_SendCore = m_Core;
			m_ReckoningCore = m_Core;
		}
	}
}

bool CCharacter::IncreaseHealth(int Amount)
{
	if(m_Health >= m_pPlayer->GetMaxHealth())
		return false;

	Amount = maximum(Amount, 1);
	m_Health = clamp(m_Health + Amount, 0, m_pPlayer->GetMaxHealth());
	GS()->MarkUpdatedBroadcast(m_pPlayer->GetCID());
	m_pPlayer->ShowHealthNickname(2);
	return true;
}

bool CCharacter::IncreaseMana(int Amount)
{
	if(m_Mana >= m_pPlayer->GetMaxMana())
		return false;

	Amount = maximum(Amount, 1);
	m_Mana = clamp(m_Mana + Amount, 0, m_pPlayer->GetMaxMana());
	GS()->MarkUpdatedBroadcast(m_pPlayer->GetCID());
	return true;
}

void CCharacter::HandleEventsDeath(int Killer, vec2 Force) const
{
	// check valid killer
	auto* pKiller = GS()->GetPlayer(Killer);
	if(!pKiller || (Killer == m_ClientID))
		return;

	// skip for prisoned client and killer
	if(m_pPlayer->Account()->GetPrisonManager().IsInPrison() ||
		(!pKiller->IsBot() && pKiller->Account()->GetPrisonManager().IsInPrison()))
		return;

	const bool KillerIsGuardian = (pKiller->IsBot() && dynamic_cast<CPlayerBot*>(pKiller)->GetBotType() == TYPE_BOT_NPC &&
		NpcBotInfo::ms_aNpcBot[dynamic_cast<CPlayerBot*>(pKiller)->GetBotMobID()].m_Function == FUNCTION_NPC_GUARDIAN);
	const bool KillerIsPlayer = !pKiller->IsBot();
	auto* pItemGold = m_pPlayer->GetItem(itGold);

	// loss gold at death
	if(GS()->HasWorldFlag(WORLD_FLAG_LOST_DEATH_GOLD) &&
		g_Config.m_SvGoldLossOnDeath)
	{
		const int LossGold = minimum(translate_to_percent_rest(pItemGold->GetValue(), (float)g_Config.m_SvGoldLossOnDeath), pItemGold->GetValue());
		if(LossGold > 0 && pItemGold->Remove(LossGold))
		{
			GS()->EntityManager()->DropItem(m_Pos, Killer >= MAX_PLAYERS ? -1 : Killer, { itGold, LossGold }, Force);
			if(KillerIsPlayer)
				GS()->Chat(m_ClientID, "You lost '{}% ({$}) gold', killer '{}'!", g_Config.m_SvGoldLossOnDeath, LossGold, Server()->ClientName(Killer));
			else
				GS()->Chat(m_ClientID, "You lost '{}% ({$})' gold due to death!", g_Config.m_SvGoldLossOnDeath, LossGold, Server()->ClientName(Killer));
		}
	}

	// Crime score system
	if(GS()->HasWorldFlag(WORLD_FLAG_CRIME_SCORE))
	{
		if(m_pPlayer->Account()->IsCrimeMaxedOut() && (KillerIsGuardian || KillerIsPlayer))
		{
			const int Arrest = minimum(translate_to_percent_rest(pItemGold->GetValue(), (float)g_Config.m_SvArrestGoldOnDeath), pItemGold->GetValue());
			if(Arrest > 0 && pItemGold->Remove(Arrest))
			{
				if(KillerIsPlayer)
				{
					pKiller->Account()->AddGold(Arrest);
					GS()->Chat(-1, "'{}' killed '{}', who was wanted. The reward is '{$} gold'!",
						Server()->ClientName(Killer), Server()->ClientName(m_pPlayer->GetCID()), Arrest);
				}
				GS()->Chat(m_ClientID, "Treasury confiscates '{}% ({$})' of your gold.", g_Config.m_SvArrestGoldOnDeath, Arrest);
			}

			m_pPlayer->Account()->GetPrisonManager().Imprison(360);
			m_pPlayer->Account()->ResetCrimeScore();
		}
		else if(KillerIsPlayer)
		{
			pKiller->Account()->IncreaseCrime(20);
		}
	}
}

int CCharacter::GetMaxWaterAir() const
{
	const auto* pBreathingReed = m_pPlayer->GetItem(itBreathingReed);
	return pBreathingReed->IsEquipped() ? 16 : 8;
}

void CCharacter::HandleWater(CTuningParams* pTuningParams)
{
	if(!m_pTilesHandler->IsActive(TILE_WATER))
	{
		if(Server()->Tick() % Server()->TickSpeed() == 0)
		{
			const int maxWaterAir = GetMaxWaterAir();
			if(m_WaterAir < maxWaterAir)
				m_WaterAir = minimum(m_WaterAir + 1, maxWaterAir);
		}

		return;
	}

	// apply physics
	bool hasDriverKit = m_pPlayer->GetItem(itDiversKit)->IsEquipped();
	bool isKeptAfloat = m_pPlayer->GetItem(itLifePreserver)->IsEquipped() || hasDriverKit;
	pTuningParams->m_Gravity = isKeptAfloat ? -0.05f : 0.15f;
	pTuningParams->m_GroundFriction = 0.95f;
	pTuningParams->m_GroundControlSpeed = 250.0f / Server()->TickSpeed();
	pTuningParams->m_GroundControlAccel = 1.5f;
	pTuningParams->m_AirFriction = 0.95f;
	pTuningParams->m_AirControlSpeed = 250.0f / Server()->TickSpeed();
	pTuningParams->m_AirControlAccel = 1.5f;
	SetEmote(EMOTE_BLINK, 1, false);

	// is has driver kit disable water air system
	if(!hasDriverKit)
	{
		// initialize variables
		const auto isHeadSubmerged = GS()->Collision()->CheckPoint(vec2(m_Core.m_Pos.x, m_Core.m_Pos.y - 16.f), CCollision::COLFLAG_WATER);
		const auto maxWaterAir = GetMaxWaterAir();
		const auto currentTick = Server()->Tick();

		// water air
		if(isHeadSubmerged)
		{
			if(m_WaterAir > 0)
			{
				if(Server()->Tick() % SERVER_TICK_SPEED == 0)
				{
					m_WaterAir--;
					GS()->Broadcast(m_pPlayer->GetCID(), BroadcastPriority::GameWarning, SERVER_TICK_SPEED, "Air: {}/{}", m_WaterAir, maxWaterAir);
				}
			}
			else
			{
				if(currentTick % SERVER_TICK_SPEED / 2 == 0)
				{
					const auto maxHP = m_pPlayer->GetMaxHealth();
					const auto damageValue = translate_to_percent_rest(maxHP, 10.f);
					TakeDamage(vec2(0, 0), damageValue, -1, WEAPON_WORLD);
					GS()->CreateSound(m_Core.m_Pos, SOUND_PLAYER_PAIN_LONG);
				}
			}
		}
		else
		{
			if(m_WaterAir < maxWaterAir)
			{
				if(currentTick % SERVER_TICK_SPEED / 4 == 0)
				{
					m_WaterAir = minimum(m_WaterAir + 1, maxWaterAir);
					GS()->Broadcast(m_pPlayer->GetCID(), BroadcastPriority::GameWarning, SERVER_TICK_SPEED, "Air: {}/{}", m_WaterAir, maxWaterAir);
				}
			}
		}
	}
}

void CCharacter::Die(int Killer, int Weapon)
{
	m_Alive = false;

	// send the kill message
	CNetMsg_Sv_KillMsg Msg;
	Msg.m_Killer = Killer;
	Msg.m_Victim = m_pPlayer->GetCID();
	Msg.m_Weapon = Weapon;
	Msg.m_ModeSpecial = 0;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1, -1, m_pPlayer->GetCurrentWorldID());

	// respawn
	m_pPlayer->m_aPlayerTick[ETickState::Die] = Server()->Tick() / 2;
	m_pPlayer->PrepareRespawnTick();
	GS()->m_pController->OnCharacterDeath(m_pPlayer, GS()->GetPlayer(Killer), Weapon);

	// remove from world
	GS()->m_World.RemoveEntity(this);
	GS()->m_World.m_Core.m_apCharacters[m_ClientID] = nullptr;
	GS()->CreateDeath(m_Pos, m_ClientID);
	GS()->CreateSound(m_Pos, SOUND_PLAYER_DIE);
}

void CCharacter::AutoUseHealingPotionIfNeeded() const
{
	// check recast time
	if(m_pPlayer->m_aPlayerTick[HealPotionRecast] >= Server()->Tick())
		return;

	// check required for heal
	if(m_Health > m_pPlayer->GetMaxHealth() / 3)
		return;

	// check for equippement potion
	const auto EquippedHealPotionOpt = m_pPlayer->GetEquippedSlotItemID(ItemType::EquipPotionHeal);
	TryUsePotion(EquippedHealPotionOpt);
}

void CCharacter::AutoUseManaPotionIfNeeded() const
{
	// check recast time
	if(m_pPlayer->m_aPlayerTick[ManaPotionRecast] >= Server()->Tick())
		return;

	// check required for mana restore
	if(m_Mana > m_pPlayer->GetMaxMana() / 3)
		return;

	// check for equippement potion
	const auto EquippedManaPotionOpt = m_pPlayer->GetEquippedSlotItemID(ItemType::EquipPotionMana);
	TryUsePotion(EquippedManaPotionOpt);
}

void CCharacter::TryUsePotion(std::optional<int> optItemID) const
{
	if(!optItemID.has_value())
		return;

	// try apply potion effect
	auto* pPlayerItem = m_pPlayer->GetItem(*optItemID);
	if(const auto optPotionContext = pPlayerItem->Info()->GetPotionContext())
	{
		const auto Effect = optPotionContext->Effect;
		if(!m_pPlayer->m_Effects.IsActive(Effect) && pPlayerItem->IsEquipped())
		{
			pPlayerItem->Use(1);
		}
	}
}

void CCharacter::SetVelocity(vec2 NewVelocity)
{
	m_Core.m_Vel = ClampVel(m_MoveRestrictions, NewVelocity);
}

void CCharacter::AddVelocity(vec2 Addition)
{
	SetVelocity(m_Core.m_Vel + Addition);
}

void CCharacter::ApplyMoveRestrictions()
{
	if(m_Core.m_Vel.y > 0 && (m_MoveRestrictions & CANTMOVE_DOWN))
	{
		m_Core.m_Jumped = 0;
		m_Core.m_JumpedTotal = 0;
	}

	m_Core.m_Vel = ClampVel(m_MoveRestrictions, m_Core.m_Vel);
	m_Core.m_vDoorHitSet.clear();
}

int CCharacter::GetTotalDamageByWeapon(int Weapon) const
{
	int Damage = 0;

	switch(Weapon)
	{
		case WEAPON_GUN: Damage += m_pPlayer->GetTotalAttributeValue(AttributeIdentifier::GunDMG); break;
		case WEAPON_SHOTGUN: Damage += m_pPlayer->GetTotalAttributeValue(AttributeIdentifier::ShotgunDMG); break;
		case WEAPON_GRENADE: Damage += m_pPlayer->GetTotalAttributeValue(AttributeIdentifier::GrenadeDMG); break;
		case WEAPON_LASER: Damage += m_pPlayer->GetTotalAttributeValue(AttributeIdentifier::RifleDMG); break;
		case WEAPON_HAMMER: Damage += m_pPlayer->GetTotalAttributeValue(AttributeIdentifier::HammerDMG); break;
		default: break;
	}

	return Damage;
}

CPlayer* CCharacter::GetLastPlayerAttacker(int Timeout) const
{
	if(m_pPlayer->m_aPlayerTick[LastDamage] > (Server()->Tick() - Server()->TickSpeed() * Timeout))
		return GS()->GetPlayer(m_LastDamageByClient);
	return nullptr;
}

bool CCharacter::TakeDamage(vec2 Force, int Damage, int FromCID, int Weapon, int ForceFlag)
{
	// apply force
	bool CanApplyForce = true;
	if(
            (ForceFlag & FORCE_FLAG_CANT_ALL) ||
            ((ForceFlag & FORCE_FLAG_CANT_SELF) && m_ClientID == FromCID) ||
            (!IsAllowedPVP(FromCID))
        )
		CanApplyForce = false;

	if(CanApplyForce)
	{
		constexpr float MaximumVel = 24.f;
		vec2 Temp = m_Core.m_Vel + Force;
		m_Core.m_Vel = ClampVel(m_MoveRestrictions,
			length(Temp) > MaximumVel ? normalize(Temp) * MaximumVel : Temp);
	}


	// check allowed can damage
	if(!IsAllowedPVP(FromCID))
		return false;

	// damage calculation
	auto* pFrom = GS()->GetPlayer(FromCID);
	if(pFrom && pFrom->GetCharacter() &&
		Weapon != WEAPON_GAME && Weapon != WEAPON_WORLD)
	{
		Damage += pFrom->GetTotalAttributeValue(AttributeIdentifier::DMG);
		Damage = (FromCID == m_pPlayer->GetCID() ? maximum(1, Damage / 2) : maximum(1, Damage));
	}

	// skip empty damage
	if(!Damage)
		return false;

	// chances of effects
	int CritDamage = 0;
	if(pFrom && pFrom->GetCharacter() && FromCID != m_pPlayer->GetCID())
	{
		// try activate ring lightning
		TryActivateChainLightning(itRingReturnLightning, Damage);
		pFrom->GetCharacter()->TryActivateChainLightning(itRingGivingLightning, Damage);

		// vampirism replenish your health
		const auto ChanceVampirism = m_pPlayer->GetTotalAttributeChance(AttributeIdentifier::Vampirism).value_or(0.f);
		if(ChanceVampirism > random_float(100.0f))
		{
			const auto Recovery = maximum(1, Damage / 2);
			pFrom->GetCharacter()->IncreaseHealth(Recovery);
			GS()->SendEmoticon(FromCID, EMOTICON_DROP);
			GS()->Chat(FromCID, ":: Vampirism stolen: {}HP.", Recovery);
		}

		// miss out on damage
		const auto ChanceLucky = m_pPlayer->GetTotalAttributeChance(AttributeIdentifier::Lucky).value_or(0.f);
		if(ChanceLucky > random_float(100.0f))
		{
			GS()->SendEmoticon(m_pPlayer->GetCID(), EMOTICON_HEARTS);
			return false;
		}

		// critical damage
		const auto ChanceCrit = m_pPlayer->GetTotalAttributeChance(AttributeIdentifier::Crit).value_or(0.f);
		if(ChanceCrit > random_float(100.0f))
		{
			const int CritAttributeDMG = maximum(pFrom->GetTotalAttributeValue(AttributeIdentifier::CritDMG), 1);
			CritDamage = (CritAttributeDMG / 2) + rand() % CritAttributeDMG;
			pFrom->GetCharacter()->SetEmote(EMOTE_ANGRY, 2, true);
		}

		// give effects from player or bot to who got damage
		pFrom->GetCharacter()->GiveRandomEffects(m_pPlayer->GetCID());
	}

	// update health & send effects
	const bool IsCriticalDamage = (CritDamage > 0);
	const auto MaxHealth = m_pPlayer->GetMaxHealth();
	const auto StarNum = maximum(1, round_to_int(translate_to_percent(MaxHealth, Damage) * 0.1f));
	const auto DamageSoundId = IsCriticalDamage ? (int)SOUND_PLAYER_PAIN_LONG : (int)SOUND_PLAYER_PAIN_SHORT;

	Damage += CritDamage;
	m_Health -= Damage;
	m_EmoteType = EMOTE_PAIN;
	m_EmoteStop = Server()->Tick() + 500 * Server()->TickSpeed() / 1000;
	m_LastDamageByClient = FromCID;
	m_pPlayer->ShowHealthNickname(2);
	m_pPlayer->m_aPlayerTick[LastDamage] = Server()->Tick();

	// last stand effect
	if(m_pPlayer->m_Effects.IsActive(ECharacterEffect::LAST_STAND))
		m_Health = maximum(1, m_Health);

	// create hit sound damage
	if(pFrom)
	{
		if(FromCID != m_pPlayer->GetCID())
		{
			pFrom->m_aPlayerTick[LastDamage] = Server()->Tick();
			GS()->CreatePlayerSound(FromCID, SOUND_HIT);
		}

		if(IsCriticalDamage && pFrom->GetItem(itShowCriticalDamage)->IsEquipped())
			GS()->Chat(FromCID, ":: Crit damage: {}p.", Damage);
	}

	GS()->CreateSound(m_Pos, DamageSoundId);
	GS()->CreateDamage(m_Pos, FromCID, StarNum, IsCriticalDamage);
	GS()->MarkUpdatedBroadcast(m_pPlayer->GetCID());
	GS()->m_pController->OnCharacterDamage(pFrom, m_pPlayer, minimum(Damage, m_Health));

	// verify death
	if(m_Health <= 0)
	{
		if(pFrom && pFrom->GetCharacter() && FromCID != m_pPlayer->GetCID())
			pFrom->GetCharacter()->SetEmote(EMOTE_HAPPY, 1, false);

		// do not kill the bot it is still running in CCharacterBotAI::TakeDamage
		if(m_pPlayer->IsBot())
			return false;

		HandleEventsDeath(FromCID, Force);
		Die(FromCID, Weapon);
		return false;
	}

	AutoUseHealingPotionIfNeeded();
	return true;
}

void CCharacter::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	CNetObj_Character* pCharacter = static_cast<CNetObj_Character*>(Server()->SnapNewItem(NETOBJTYPE_CHARACTER, m_pPlayer->GetCID(), sizeof(CNetObj_Character)));
	if(!pCharacter)
		return;

	// Check if the default input is blocked for the player
	const bool BlockingInputChangeWeapon = Server()->Input()->IsBlockedInputGroup(m_pPlayer->GetCID(), BLOCK_INPUT_FREEZE_WEAPON);
	const bool BlockingInputFireWeapon = Server()->Input()->IsBlockedInputGroup(m_pPlayer->GetCID(), BLOCK_INPUT_FIRE);

	// write down the m_Core
	if(!m_ReckoningTick)
	{
		// no dead reckoning when paused because the client doesn't know
		// how far to perform the reckoning
		pCharacter->m_Tick = 0;
		m_Core.Write(pCharacter);
	}
	else
	{
		pCharacter->m_Tick = m_ReckoningTick;
		m_SendCore.Write(pCharacter);
	}

	// set emote
	if(m_EmoteStop < Server()->Tick())
	{
		m_EmoteType = EMOTE_NORMAL;
		m_EmoteStop = -1;
	}

	pCharacter->m_Emote = m_EmoteType;
	if(250 - ((Server()->Tick() - m_LastAction) % (250)) < 5)
		pCharacter->m_Emote = EMOTE_BLINK;

	pCharacter->m_AttackTick = m_AttackTick;
	pCharacter->m_Direction = m_Input.m_Direction;
	pCharacter->m_Weapon = m_Core.m_ActiveWeapon;
	pCharacter->m_AmmoCount = 0;
	pCharacter->m_Health = 0;
	pCharacter->m_Armor = 0;
	pCharacter->m_PlayerFlags = m_pPlayer->m_PlayerFlags;
	if(pCharacter->m_HookedPlayer != -1)
	{
		if(!Server()->Translate(pCharacter->m_HookedPlayer, SnappingClient))
			pCharacter->m_HookedPlayer = -1;
	}

	if(m_pPlayer->GetCID() == SnappingClient || SnappingClient == -1)
	{
		if(m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo > 0)
		{
			const int MaximumAmmo = 10 + m_pPlayer->GetTotalAttributeValue(AttributeIdentifier::Ammo);
			const int AmmoPercent = translate_to_percent(MaximumAmmo, m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo, 10.0f);
			pCharacter->m_AmmoCount = clamp(AmmoPercent, 1, 10);
		}

		if(m_Health > 0)
		{
			const int HealthPercent = translate_to_percent(m_pPlayer->GetMaxHealth(), m_Health, 10.0f);
			pCharacter->m_Health = clamp(HealthPercent, 1, 10);
		}

		if(m_Mana > 0)
		{
			const int ManaPercent = translate_to_percent(m_pPlayer->GetMaxMana(), m_Mana, 10.0f);
			pCharacter->m_Armor = clamp(ManaPercent, 1, 10);
		}
	}

	// DDNetCharacter
	CNetObj_DDNetCharacter* pDDNetCharacter = static_cast<CNetObj_DDNetCharacter*>(Server()->SnapNewItem(NETOBJTYPE_DDNETCHARACTER, m_pPlayer->GetCID(), sizeof(CNetObj_DDNetCharacter)));
	if(!pDDNetCharacter)
		return;

	pDDNetCharacter->m_Flags = 0;

	auto DDNetFlag = [&](int flag, bool check) { if(check) pDDNetCharacter->m_Flags |= flag; };
	DDNetFlag(CHARACTERFLAG_SUPER, m_Core.m_Super);
	DDNetFlag(CHARACTERFLAG_ENDLESS_HOOK, m_Core.m_EndlessHook);
	DDNetFlag(CHARACTERFLAG_ENDLESS_JUMP, m_Core.m_EndlessJump);
	DDNetFlag(CHARACTERFLAG_JETPACK, m_Core.m_Jetpack);
	DDNetFlag(CHARACTERFLAG_COLLISION_DISABLED, (m_Core.m_CollisionDisabled || !(bool)m_pPlayer->m_NextTuningParams.m_PlayerCollision));
	DDNetFlag(CHARACTERFLAG_HOOK_HIT_DISABLED, (m_Core.m_HookHitDisabled || !(bool)m_pPlayer->m_NextTuningParams.m_PlayerHooking));
	DDNetFlag(CHARACTERFLAG_TELEGUN_GUN, m_Core.m_HasTelegunGun);
	DDNetFlag(CHARACTERFLAG_TELEGUN_GRENADE, m_Core.m_HasTelegunGrenade);
	DDNetFlag(CHARACTERFLAG_TELEGUN_LASER, m_Core.m_HasTelegunLaser);
	DDNetFlag(CHARACTERFLAG_WEAPON_HAMMER, BlockingInputChangeWeapon ? false : m_Core.m_aWeapons[WEAPON_HAMMER].m_Got);
	DDNetFlag(CHARACTERFLAG_WEAPON_GUN, BlockingInputChangeWeapon ? false : m_Core.m_aWeapons[WEAPON_GUN].m_Got);
	DDNetFlag(CHARACTERFLAG_WEAPON_SHOTGUN, BlockingInputChangeWeapon ? false : m_Core.m_aWeapons[WEAPON_SHOTGUN].m_Got);
	DDNetFlag(CHARACTERFLAG_WEAPON_GRENADE, BlockingInputChangeWeapon ? false : m_Core.m_aWeapons[WEAPON_GRENADE].m_Got);
	DDNetFlag(CHARACTERFLAG_WEAPON_LASER, BlockingInputChangeWeapon ? false : m_Core.m_aWeapons[WEAPON_LASER].m_Got);
	DDNetFlag(CHARACTERFLAG_WEAPON_NINJA, BlockingInputChangeWeapon ? false : m_Core.m_ActiveWeapon == WEAPON_NINJA);
	DDNetFlag(CHARACTERFLAG_HAMMER_HIT_DISABLED, BlockingInputFireWeapon ? true : m_Core.m_HammerHitDisabled);
	DDNetFlag(CHARACTERFLAG_SHOTGUN_HIT_DISABLED, BlockingInputFireWeapon ? true : m_Core.m_ShotgunHitDisabled);
	DDNetFlag(CHARACTERFLAG_GRENADE_HIT_DISABLED, BlockingInputFireWeapon ? true : m_Core.m_GrenadeHitDisabled);
	DDNetFlag(CHARACTERFLAG_LASER_HIT_DISABLED, BlockingInputFireWeapon ? true : m_Core.m_LaserHitDisabled);
	DDNetFlag(CHARACTERFLAG_SOLO, m_Core.m_Solo);
	DDNetFlag(CHARACTERFLAG_MOVEMENTS_DISABLED, m_Core.m_MovingDisabled);
	DDNetFlag(CHARACTERFLAG_IN_FREEZE, m_Core.m_IsInFreeze);
	DDNetFlag(CHARACTERFLAG_PRACTICE_MODE, BlockingInputFireWeapon);
	DDNetFlag(CHARACTERFLAG_LOCK_MODE, false);
	DDNetFlag(CHARACTERFLAG_TEAM0_MODE, false);
	DDNetFlag(CHARACTERFLAG_INVINCIBLE, false);

	pDDNetCharacter->m_FreezeEnd = 0;
	pDDNetCharacter->m_Jumps = m_Core.m_Jumps;
	pDDNetCharacter->m_TeleCheckpoint = 0;
	pDDNetCharacter->m_StrongWeakId = 0;

	// Display Informations
	pDDNetCharacter->m_JumpedTotal = m_Core.m_JumpedTotal;
	pDDNetCharacter->m_NinjaActivationTick = m_Core.m_Ninja.m_ActivationTick;
	pDDNetCharacter->m_FreezeStart = m_Core.m_FreezeStart;
	pDDNetCharacter->m_TargetX = m_Core.m_Input.m_TargetX;
	pDDNetCharacter->m_TargetY = m_Core.m_Input.m_TargetY;
	pDDNetCharacter->m_TuneZoneOverride = m_TuneZoneOverride;
}

void CCharacter::PostSnap()
{
	m_TriggeredEvents = 0;
}

bool CCharacter::HandleTiles()
{
	// handle Anti-Skip tiles
	int CurrentIndex = GS()->Collision()->GetMapIndex(m_Pos);
	std::vector<int> vIndices = GS()->Collision()->GetMapIndices(m_PrevPos, m_Pos);
	if(!vIndices.empty())
	{
		for(int& Index : vIndices)
		{
			HandleTilesImpl(Index);
			if(!m_Alive)
				return false;
		}
	}
	else
	{
		HandleTilesImpl(CurrentIndex);
		if(!m_Alive)
			return false;
	}

	return true;
}

static bool IsDoorHitActive(int Number, void* pUser)
{
	auto* pChar = (CCharacter*)pUser;
	return pChar ? pChar->m_Core.m_vDoorHitSet.contains(Number) : false;
}

void CCharacter::HandleTilesImpl(int Index)
{
	m_pTilesHandler->Handle(Index);
	m_MoveRestrictions = GS()->Collision()->GetMoveRestrictions(&IsDoorHitActive, this, m_Pos, 18.0f);

	// teleport
	if(m_pTilesHandler->IsActive(TILE_TELE_FROM))
	{
		if(const auto outsPos = GS()->Collision()->TryGetTeleportOut(m_Pos))
			ChangePosition(outsPos.value());
	}

	// confirm teleport
	if(m_pTilesHandler->IsActive(TILE_TELE_FROM_CONFIRM))
	{
		if(const auto outsPos = GS()->Collision()->TryGetTeleportOut(m_Pos))
		{
			GS()->Broadcast(m_ClientID, BroadcastPriority::TitleInformation, Server()->TickSpeed(), "Use the hammer to enter");
            if(m_Core.m_ActiveWeapon == WEAPON_HAMMER && m_AttackTick == Server()->Tick() - 1)
				ChangePosition(outsPos.value());
		}
	}

	// water effect enter exit
	if(m_pTilesHandler->IsEnter(TILE_WATER) || m_pTilesHandler->IsExit(TILE_WATER))
	{
		GS()->CreateDeath(m_Pos, m_ClientID);
	}

	// handle locked view camera and tile interactions if the player is not a bot
	if(!m_pPlayer->IsBot())
	{
		// fishing information
		if(m_pTilesHandler->IsEnter(TILE_FISHING_MODE))
		{
			GS()->Broadcast(m_ClientID, BroadcastPriority::GameBasicStats, 100, "Use 'Fire' to start fishing!");
		}
		else if(m_pTilesHandler->IsExit(TILE_FISHING_MODE) && m_pFishingRod)
		{
			delete m_pFishingRod;
		}

		// zone information
		if(m_pTilesHandler->IsActive(TILE_SW_ZONE))
		{
			const auto pZone = GS()->Collision()->GetZonedetail(m_Pos);
			if(pZone && ((Server()->Tick() % Server()->TickSpeed() == 0) || m_Zonename != pZone->Name))
			{
				m_Zonename = pZone->Name;
				const auto infoZone = fmt_default("{} zone. ({})", pZone->Name, pZone->PVP ? "PVP" : "Safe");
				GS()->Broadcast(m_ClientID, BroadcastPriority::GameBasicStats, 50, infoZone.c_str());
			}
		}
		else if(m_pTilesHandler->IsExit(TILE_SW_ZONE))
		{
			m_Zonename = "unknown";
			GS()->Broadcast(m_ClientID, BroadcastPriority::GameBasicStats, 50, "");
		}

		// chairs
		if(m_pTilesHandler->IsActive(TILE_CHAIR_LV1))
			m_pPlayer->Account()->HandleChair(1);
		if(m_pTilesHandler->IsActive(TILE_CHAIR_LV2))
			m_pPlayer->Account()->HandleChair(7);
		if(m_pTilesHandler->IsActive(TILE_CHAIR_LV3))
			m_pPlayer->Account()->HandleChair(15);

		// locked view cam
		if(const auto result = GS()->Collision()->TryGetFixedCamPos(m_Pos))
			m_pPlayer->LockedView().ViewLock(result->first, result->second);

		// check from components
		GS()->Core()->OnCharacterTile(this);
	}
}

void CCharacter::GiveRandomEffects(int To)
{
	[[maybe_unused]] CPlayer* pPlayerTo = GS()->GetPlayer(To);
	if(!pPlayerTo && To != m_pPlayer->GetCID())
		return;

	// Here effects ( buffs ) from player for TO
}

bool CCharacter::HandleHammerActions(vec2 Direction, vec2 ProjStartPos)
{
	if(m_pPlayer->IsBot())
		return false;

	// take items from ground
	if(GS()->TakeItemCharacter(m_pPlayer->GetCID()))
		return true;

	// try start dialogue
	for(const auto* pTarget = (CCharacter*)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pTarget; pTarget = (CCharacter*)pTarget->TypeNext())
	{
		if(pTarget == this || !pTarget->GetPlayer()->IsBot())
			continue;

		// check distance valid
		if(distance(pTarget->m_Core.m_Pos, m_Core.m_Pos) > 64.0f)
			continue;

		// talking wth bot
		auto* pTargetPlayerBot = dynamic_cast<CPlayerBot*>(pTarget->GetPlayer());
		if(StartConversation(pTargetPlayerBot))
		{
			GS()->CreatePlayerSound(m_pPlayer->GetCID(), SOUND_TEE_CRY);
			GS()->CreateHammerHit(ProjStartPos);

			const int BotID = pTargetPlayerBot->GetBotID();
			GS()->Chat(m_pPlayer->GetCID(), "You begin speaking with '{}'.", DataBotInfo::ms_aDataBot[BotID].m_aNameBot);
			return true;
		}
	}

	// gathering items
	auto vGatheringItems = GameWorld()->FindEntities(m_Core.m_Pos, 16.f, 32, CGameWorld::ENTTYPE_GATHERING_NODE);
	for(auto* pItem : vGatheringItems)
	{
		auto* pGatheringItem = dynamic_cast<CEntityGatheringNode*>(pItem);
		if(pGatheringItem && pGatheringItem->TakeDamage(m_pPlayer))
		{
			m_ReloadTimer = Server()->TickSpeed() / 3;
			return true;
		}
	}

	return false;
}

void CCharacter::HandleTuning()
{
	m_TuneZoneOverride = -1;
	HandleIndependentTuning();
}

void CCharacter::MovingDisable(bool State)
{
	if(State)
	{
		m_Core.m_Vel = {};
		ResetHook();
	}
	m_Core.m_MovingDisabled = State;
}

void CCharacter::HandleIndependentTuning()
{
	CTuningParams* pTuningParams = &m_pPlayer->m_NextTuningParams;

	// freeze moving
	if(m_Core.m_MovingDisabled)
	{
		pTuningParams->m_GroundFriction = 1.0f;
		pTuningParams->m_GroundControlSpeed = 0.0f;
		pTuningParams->m_GroundControlAccel = 0.0f;
		pTuningParams->m_GroundJumpImpulse = 0.0f;
		pTuningParams->m_AirFriction = 1.0f;
		pTuningParams->m_AirControlSpeed = 0.0f;
		pTuningParams->m_AirControlAccel = 0.0f;
		pTuningParams->m_AirJumpImpulse = 0.0f;
		pTuningParams->m_HookLength = 0.0f;
		pTuningParams->m_HookDragAccel = 0.f;
	}

	// handle water
	HandleWater(pTuningParams);

	// potions and buffs are different
	HandleBuff(pTuningParams);
}

void CCharacter::HandleBuff(CTuningParams* TuningParams)
{
	if(m_pPlayer->m_Effects.IsActive(ECharacterEffect::SLOWNESS))
	{
		TuningParams->m_Gravity = 0.35f;
		TuningParams->m_GroundFriction = 0.45f;
		TuningParams->m_GroundControlSpeed = 100.0f / Server()->TickSpeed();
		TuningParams->m_GroundControlAccel = 0.7f;
		TuningParams->m_GroundJumpImpulse = 7.0f;
		TuningParams->m_AirFriction = 0.4f;
		TuningParams->m_AirControlSpeed = 100.0f / Server()->TickSpeed();
		TuningParams->m_AirControlAccel = 0.7f;
		TuningParams->m_AirJumpImpulse = 7.0f;
		TuningParams->m_HookLength = 0.0f;
	}

	if(m_pPlayer->m_Effects.IsActive(ECharacterEffect::STUN))
	{
		TuningParams->m_Gravity = 0.25f;
		TuningParams->m_GroundFriction = 0.45f;
		TuningParams->m_GroundControlSpeed = 30.0f / Server()->TickSpeed();
		TuningParams->m_GroundControlAccel = 0.7f;
		TuningParams->m_GroundJumpImpulse = 2.0f;
		TuningParams->m_AirFriction = 0.4f;
		TuningParams->m_AirControlSpeed = 30.0f / Server()->TickSpeed();
		TuningParams->m_AirControlAccel = 0.7f;
		TuningParams->m_AirJumpImpulse = 2.0f;
		TuningParams->m_HookLength = 0.0f;
	}

	// buffs
	if(Server()->Tick() % Server()->TickSpeed() == 0)
	{
		// fire
		if(m_pPlayer->m_Effects.IsActive(ECharacterEffect::FIRE))
		{
			const int ExplDmg = translate_to_percent_rest(m_pPlayer->GetMaxHealth(), 3);
			GS()->CreateExplosion(m_Core.m_Pos, -1, WEAPON_GAME, ExplDmg);
		}

		// poison
		if(m_pPlayer->m_Effects.IsActive(ECharacterEffect::POISON))
		{
			const int PoisonDmg = translate_to_percent_rest(m_pPlayer->GetMaxHealth(), 3);
			TakeDamage({}, PoisonDmg, -1, WEAPON_GAME);
		}

		// handle potions
		std::ranges::for_each(CItemDescription::s_vTotalPotionByItemIDList, [this](const auto& potion)
		{
			const auto ItemID = potion.first;
			const auto& PotionContext = potion.second;
			const auto Type = GS()->GetItemInfo(ItemID)->GetType();

			// increase by equip type
			if(m_pPlayer->m_Effects.IsActive(PotionContext.Effect))
			{
				if(Type == ItemType::EquipPotionHeal)
				{
					IncreaseHealth(PotionContext.Value);
				}
				else if(Type == ItemType::EquipPotionMana)
				{
					IncreaseMana(PotionContext.Value);
				}
			}
		});
	}
}

void CCharacter::HandleSafeFlags()
{
	// reset
	m_Core.m_HammerHitDisabled = false;
	m_Core.m_CollisionDisabled = false;
	m_Core.m_HookHitDisabled = false;
	m_Core.m_DamageDisabled = false;

	// set full safe for collision flag safe
	if(GS()->Collision()->CheckPoint(m_Core.m_Pos, CCollision::COLFLAG_SAFE))
		SetSafeFlags();

	// update by safe tick
	if(m_SafeTickFlags)
	{
		if(m_SafeTickFlags & SAFEFLAG_HAMMER_HIT_DISABLED)
			m_Core.m_HammerHitDisabled = true;
		if(m_SafeTickFlags & SAFEFLAG_COLLISION_DISABLED)
			m_Core.m_CollisionDisabled = true;
		if(m_SafeTickFlags & SAFEFLAG_HOOK_HIT_DISABLED)
			m_Core.m_HookHitDisabled = true;
		if(m_SafeTickFlags & SAFEFLAG_DAMAGE_DISABLED)
			m_Core.m_DamageDisabled = true;
		m_SafeTickFlags = 0;
	}
}

void CCharacter::UpdateEquippedStats(std::optional<int> UpdatedItemID)
{
	if(!m_Alive || !m_pPlayer->IsAuthed())
		return;

	// check and adjust health if necessary
	const auto MaxHealth = m_pPlayer->GetMaxHealth();
	if(m_Health > MaxHealth)
	{
		GS()->Chat(m_pPlayer->GetCID(), "Your health has been reduced.");
		m_Health = MaxHealth;
	}

	// check and adjust mana if necessary
	const auto MaxMana = m_pPlayer->GetMaxMana();
	if(m_Mana > MaxMana)
	{
		GS()->Chat(m_pPlayer->GetCID(), "Your mana has been reduced.");
		m_Mana = MaxMana;
	}

	// check and limit gold capacity
	const auto CurrentGold = m_pPlayer->Account()->GetGold();
	const auto MaxGoldCapacity = m_pPlayer->Account()->GetGoldCapacity();
	if(CurrentGold > MaxGoldCapacity)
	{
		const int excessGold = CurrentGold - MaxGoldCapacity;
		m_pPlayer->Account()->AddGoldToBank(excessGold);
		m_pPlayer->GetItem(itGold)->Remove(excessGold);
		GS()->Chat(m_pPlayer->GetCID(), "Your gold has been reduced to the maximum capacity.");
	}

	// update by item
	if(UpdatedItemID.has_value())
	{
		const auto* pItemInfo = GS()->GetItemInfo(*UpdatedItemID);

		// weapon
		const auto Type = pItemInfo->GetType();
		const auto WeaponID = GetWeaponByEquip(Type);
		if(WeaponID >= WEAPON_HAMMER)
		{
			const auto Ammo = (WeaponID == WEAPON_HAMMER ? -1 : 3);
			GiveWeapon(WeaponID, Ammo);
		}

		// eidolon
		if(Type == ItemType::EquipEidolon)
		{
			m_pPlayer->TryRemoveEidolon();
			m_pPlayer->TryCreateEidolon();
		}

		// check and limit ammo
		int totalRealAmmo = 10 + m_pPlayer->GetTotalAttributeValue(AttributeIdentifier::Ammo);
		m_Core.m_aWeapons[WEAPON_GUN].m_Ammo = minimum(m_Core.m_aWeapons[WEAPON_GUN].m_Ammo, totalRealAmmo);
		m_Core.m_aWeapons[WEAPON_SHOTGUN].m_Ammo = minimum(m_Core.m_aWeapons[WEAPON_SHOTGUN].m_Ammo, totalRealAmmo);
		m_Core.m_aWeapons[WEAPON_GRENADE].m_Ammo = minimum(m_Core.m_aWeapons[WEAPON_GRENADE].m_Ammo, totalRealAmmo);
		m_Core.m_aWeapons[WEAPON_LASER].m_Ammo = minimum(m_Core.m_aWeapons[WEAPON_LASER].m_Ammo, totalRealAmmo);
	}
}

void CCharacter::HandlePlayer()
{
	if(!m_pPlayer->IsAuthed())
		return;

	// recovery mana
	if(Server()->Tick() % (Server()->TickSpeed() * 3) == 0)
	{
		if(m_Mana < m_pPlayer->GetMaxMana())
			IncreaseMana(m_pPlayer->GetMaxMana() / 20);
	}

	// handle
	HandleHookActions();
}

bool CCharacter::IsAllowedPVP(int FromCID) const
{
	if(auto* pFrom = GS()->GetPlayer(FromCID, false, true))
	{
		// dissable self damage from weapons
		if(FromCID == m_pPlayer->GetCID() && m_pPlayer->GetItem(itDamageEqualizer)->IsEquipped())
			return false;

		// Check if damage is disabled for the current object or the object it is interacting with
		if(m_Core.m_DamageDisabled || pFrom->GetCharacter()->m_Core.m_DamageDisabled)
			return false;

		// disable damage on safe area
		if(GS()->Collision()->GetCollisionFlagsAt(m_Core.m_Pos) & CCollision::COLFLAG_SAFE
			|| GS()->Collision()->GetCollisionFlagsAt(pFrom->GetCharacter()->m_Core.m_Pos) & CCollision::COLFLAG_SAFE)
			return false;

		// skip damage intersect door
		if(GS()->Collision()->IntersectLineDoor(pFrom->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos))
			return false;

		if(pFrom->IsBot())
		{
			// can damage bot
			const auto* pBotChar = dynamic_cast<CCharacterBotAI*>(pFrom->GetCharacter());
			if(!pBotChar->AI()->CanDamage(m_pPlayer))
				return false;
		}
		else
		{
			// anti pvp on safe world or dungeon
			if(!GS()->IsAllowedPVP() || GS()->IsWorldType(WorldType::Dungeon))
				return false;

			// only for unself player
			if(FromCID != m_pPlayer->GetCID())
			{
				// anti pvp for guild players
				if(m_pPlayer->Account()->IsClientSameGuild(FromCID))
					return false;

				// anti pvp for group players
				GroupData* pGroup = m_pPlayer->Account()->GetGroup();
				if(pGroup && pGroup->HasAccountID(pFrom->Account()->GetID()))
					return false;
			}
		}
	}

	return true;
}

bool CCharacter::CanAccessWorld() const
{
	if(Server()->Tick() % Server()->TickSpeed() * 3 == 0 && m_pPlayer->IsAuthed())
	{
		// check accessible to world by level
		const auto* pAccount = m_pPlayer->Account();
		if(pAccount->GetLevel() < GS()->GetWorldData()->GetRequiredLevel())
		{
			m_pPlayer->GetSharedData().ClearSpawnPosition();
			GS()->Chat(m_pPlayer->GetCID(), "You were magically transported!");
			m_pPlayer->ChangeWorld(BASE_GAME_WORLD_ID);
			return false;
		}

		// check finished tutorial
		/*if(!m_pPlayer->GetItem(itTittleNewbie)->HasItem() && !GS()->IsPlayerInWorld(m_ClientID, TUTORIAL_WORLD_ID))
		{
			m_pPlayer->GetSharedData().ClearSpawnPosition();
			GS()->Chat(m_pPlayer->GetCID(), "You need to complete the Tutorial.");
			m_pPlayer->ChangeWorld(TUTORIAL_WORLD_ID);
			return false;
		}*/

		// TODO: remove how realized tutorial scenario
		if(GS()->IsPlayerInWorld(m_ClientID, TUTORIAL_WORLD_ID))
		{
			auto* pNewbieTittle = m_pPlayer->GetItem(itTittleNewbie);
			if(!pNewbieTittle->HasItem())
				pNewbieTittle->Add(1);

			m_pPlayer->GetSharedData().ClearSpawnPosition();
			GS()->Chat(m_pPlayer->GetCID(), "You need to complete the Tutorial.");
			m_pPlayer->ChangeWorld(BASE_GAME_WORLD_ID);
		}
	}
	return true;
}

bool CCharacter::TryUseMana(int Mana)
{
	if(m_Mana < Mana)
	{
		GS()->Broadcast(m_pPlayer->GetCID(), BroadcastPriority::GameWarning, 100, "Mana is required for the casting or continuation of this spell.");
		return false;
	}

	m_Mana -= Mana;

	GS()->MarkUpdatedBroadcast(m_pPlayer->GetCID());
	AutoUseManaPotionIfNeeded();
	return true;
}

void CCharacter::ChangePosition(vec2 NewPos)
{
	if(!m_Alive)
		return;

	GS()->CreateDeath(m_Core.m_Pos, m_pPlayer->GetCID());
	GS()->CreatePlayerSpawn(NewPos);
	m_Core.m_Pos = NewPos;
	m_Pos = NewPos;
	ResetHook();
}

bool CCharacter::StartConversation(CPlayerBot* pTarget) const
{
	if(m_pPlayer->IsBot() || !pTarget)
		return false;

	if(pTarget && pTarget->IsConversational() && pTarget->IsActiveForClient(m_pPlayer->GetCID()) != ESnappingPriority::None)
	{
		m_pPlayer->m_Dialog.Start(pTarget->GetCID());
		return true;
	}

	return false;
}

void CCharacter::TryActivateChainLightning(int ByItemId, std::optional<int> DamageOpt)
{
	if(!m_Alive || m_LastRingChainLightningAttack > Server()->Tick() || !CItemDescription::Data().contains(ByItemId))
		return;

	if(m_pPlayer->GetItem(ByItemId)->IsEquipped() || m_pPlayer->GetItem(itRingPerfectLightning)->IsEquipped())
	{
		const auto totalDamage = DamageOpt ? *DamageOpt : m_pPlayer->GetTotalAttributeValue(AttributeIdentifier::DMG);
		const auto totalChainDamage = translate_to_percent_rest(totalDamage, 20);

		new CEntityTeslaSerpent(&GS()->m_World, m_ClientID, m_Pos, random_range_pos(vec2 {}, 128.f), totalChainDamage, 500.f, 8, 0.7f);
		m_LastRingChainLightningAttack = Server()->Tick() + CalculateChainLightningCooldown();
	}
}

int CCharacter::CalculateChainLightningCooldown() const
{
	if(m_pPlayer->GetItem(itRingPerfectLightning)->IsEquipped())
		return (1 + rand() % 2) * Server()->TickSpeed();
	return (1 + rand() % 6) * Server()->TickSpeed();
}