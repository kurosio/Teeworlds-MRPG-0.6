/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
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

#include <game/server/core/entities/items/harvesting_item.h>
#include <game/server/core/entities/tools/multiple_orbite.h>

#include "character_bot.h"

MACRO_ALLOC_POOL_ID_IMPL(CCharacter, MAX_CLIENTS* ENGINE_MAX_WORLDS + MAX_CLIENTS)

CCharacter::CCharacter(CGameWorld* pWorld)
	: CEntity(pWorld, CGameWorld::ENTTYPE_CHARACTER, vec2(0, 0), ms_PhysSize)
{
	m_pTilesHandler = new CTileHandler(pWorld->GS()->Collision(), this);
}

CCharacter::~CCharacter()
{
	delete m_pTilesHandler;
	m_pMultipleOrbite = nullptr;
	GS()->m_World.m_Core.m_apCharacters[m_pPlayer->GetCID()] = nullptr;
}

bool CCharacter::Spawn(CPlayer* pPlayer, vec2 Pos)
{
	m_pPlayer = pPlayer;
	m_ClientID = pPlayer->GetCID();

	m_EmoteStop = -1;
	m_LastAction = -1;
	m_LastNoAmmoSound = -1;
	m_LastWeapon = WEAPON_HAMMER;
	m_QueuedWeapon = -1;

	m_Pos = Pos;
	m_Core.Reset();
	m_Core.Init(&GS()->m_World.m_Core, GS()->Collision());
	m_Core.m_ActiveWeapon = WEAPON_HAMMER;
	m_Core.m_Pos = m_Pos;
	m_SpawnPoint = m_Core.m_Pos;
	GS()->m_World.m_Core.m_apCharacters[m_pPlayer->GetCID()] = &m_Core;

	m_ReckoningTick = 0;
	m_SendCore = {};
	GS()->m_World.InsertEntity(this);
	m_Alive = true;
	m_NumInputs = 0;

	m_Mana = 0;
	m_OldPos = Pos;
	m_Core.m_DamageDisabled = false;
	m_Core.m_CollisionDisabled = false;
	m_Core.m_WorldID = m_pPlayer->GetCurrentWorldID();

	if(!m_pPlayer->IsBot())
	{
		m_pPlayer->m_MoodState = m_pPlayer->GetMoodState();

		GS()->Core()->QuestManager()->Update(m_pPlayer);
		GS()->Core()->QuestManager()->TryAcceptNextQuestChainAll(m_pPlayer);

		m_AmmoRegen = m_pPlayer->GetTotalAttributeValue(AttributeIdentifier::AmmoRegen);
		m_pPlayer->m_VotesData.UpdateCurrentVotes();
		GS()->MarkUpdatedBroadcast(m_pPlayer->GetCID());

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
	return false;
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
	bool FullAuto = (IsCharBot || m_pPlayer->GetSkill(SkillMasterWeapon)->IsLearned());

	// Check if the character will fire their weapon
	bool WillFire = CountInput(m_LatestPrevInput.m_Fire, m_LatestInput.m_Fire).m_Presses;

	// Check if the player has the FullAuto skill, the fire button is pressed, and there is ammo in the active weapon
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
		// Check if the active weapon has no ammo
		if(!m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo)
		{
			HandleReload();
			return;
		}
	}

	const vec2 MouseTarget = vec2(m_LatestInput.m_TargetX, m_LatestInput.m_TargetY);
	const vec2 Direction = normalize(MouseTarget);
	const vec2 ProjStartPos = m_Pos + Direction * GetRadius() * 0.75f;
	switch(m_Core.m_ActiveWeapon)
	{
		case WEAPON_HAMMER:
		{
			if(InteractiveHammer(Direction, ProjStartPos))
			{
				m_ReloadTimer = Server()->TickSpeed() / 3;
				return;
			}

			bool Hits = false;
			constexpr float Radius = 3.2f;
			GS()->CreateSound(m_Pos, SOUND_HAMMER_FIRE);

			CCharacter* apEnts[MAX_CLIENTS];
			const int Num = GS()->m_World.FindEntities(ProjStartPos, GetRadius() * Radius, (CEntity**)apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
			for(int i = 0; i < Num; ++i)
			{
				CCharacter* pTarget = apEnts[i];
				if((pTarget == this) || GS()->Collision()->IntersectLineWithInvisible(ProjStartPos, pTarget->m_Pos, nullptr, nullptr))
					continue;

				// talking wth bot
				if(StartConversation(pTarget->GetPlayer()))
				{
					GS()->CreatePlayerSound(m_pPlayer->GetCID(), SOUND_PLAYER_SPAWN);
					GS()->CreateHammerHit(ProjStartPos);
					Hits = true;

					const int BotID = pTarget->GetPlayer()->GetBotID();
					GS()->Chat(m_pPlayer->GetCID(), "You begin speaking with {}.", DataBotInfo::ms_aDataBot[BotID].m_aNameBot);
					break;
				}

				// dissalow hammer hit from self eidolon
				if(m_pPlayer->GetEidolon() && m_pPlayer->GetEidolon()->GetCID() == pTarget->GetPlayer()->GetCID())
					continue;

				if(pTarget->m_Core.m_CollisionDisabled)
					continue;

				if(length(pTarget->m_Pos - ProjStartPos) > 0.0f)
					GS()->CreateHammerHit(pTarget->m_Pos - normalize(pTarget->m_Pos - ProjStartPos) * GetRadius() * Radius);
				else
					GS()->CreateHammerHit(ProjStartPos);

				vec2 Dir = vec2(0.f, -1.f);
				if(length(pTarget->m_Pos - m_Pos) > 0.0f)
					Dir = normalize(pTarget->m_Pos - m_Pos);

				pTarget->TakeDamage(vec2(0.f, -1.f) + normalize(Dir + vec2(0.f, -1.1f)) * 10.0f, g_pData->m_Weapons.m_Hammer.m_pBase->m_Damage, m_pPlayer->GetCID(), m_Core.m_ActiveWeapon);
				Hits = true;
			}
			if(Hits)
				m_ReloadTimer = Server()->TickSpeed() / 3;
		} break;

		case WEAPON_GUN:
		{
			const bool IsExplosive = m_pPlayer->GetItem(itExplosiveGun)->IsEquipped();
			new CProjectile(
				GameWorld(), 
				WEAPON_GUN, 
				m_pPlayer->GetCID(), 
				ProjStartPos, 
				Direction, 
				(int)(Server()->TickSpeed() * GS()->Tuning()->m_GunLifetime),
				g_pData->m_Weapons.m_Gun.m_pBase->m_Damage, 
				IsExplosive, 
				0, 
				-1,
				MouseTarget,
				WEAPON_GUN);

			GS()->CreateSound(m_Pos, SOUND_GUN_FIRE);
		} break;

		case WEAPON_SHOTGUN:
		{
			const bool IsExplosive = m_pPlayer->GetItem(itExplosiveShotgun)->IsEquipped();
			const int ShotSpread = 5;
			for(int i = 0; i < ShotSpread; ++i)
			{
				const float Spreading = ((0.0058945f * (9.0f * ShotSpread) / 2)) - (0.0058945f * (9.0f * i));
				const float a = angle(Direction) + Spreading;
				const float Speed = (float)GS()->Tuning()->m_ShotgunSpeeddiff + random_float(0.2f);
				vec2 TargetPos = MouseTarget + vec2(cosf(a), sinf(a)) * (Speed * 500.0f);

				new CProjectile(
					GameWorld(), 
					WEAPON_SHOTGUN, 
					m_pPlayer->GetCID(), 
					ProjStartPos,
					vec2(cosf(a), sinf(a)) * Speed,
					(int)(Server()->TickSpeed() * GS()->Tuning()->m_ShotgunLifetime),
					g_pData->m_Weapons.m_Shotgun.m_pBase->m_Damage, 
					IsExplosive, 
					0, 
					15,
					TargetPos,
					WEAPON_SHOTGUN);
			}
			GS()->CreateSound(m_Pos, SOUND_SHOTGUN_FIRE);
		} break;

		case WEAPON_GRENADE:
		{
			const int ShotSpread = 1;
			for(int i = 0; i < ShotSpread; ++i)
			{
				const float Spreading = ((0.0058945f * (9.0f * ShotSpread) / 2)) - (0.0058945f * (9.0f * i));
				const float a = angle(Direction) + Spreading;
				const float Speed = (float)GS()->Tuning()->m_GrenadeSpeed + random_float(0.2f);
				vec2 TargetPos = MouseTarget + vec2(cosf(a), sinf(a)) * (Speed * 500.0f);

				new CProjectile(
					GameWorld(), 
					WEAPON_GRENADE, 
					m_pPlayer->GetCID(), 
					ProjStartPos,
					vec2(cosf(a), sinf(a)),
					(int)(Server()->TickSpeed() * GS()->Tuning()->m_GrenadeLifetime),
					g_pData->m_Weapons.m_Grenade.m_pBase->m_Damage, 
					true, 
					0, 
					SOUND_GRENADE_EXPLODE,
					TargetPos,
					WEAPON_GRENADE);
			}
			GS()->CreateSound(m_Pos, SOUND_GRENADE_FIRE);
		} break;

		case WEAPON_LASER:
		{
			const int ShotSpread = 1;
			for(int i = 0; i < ShotSpread; ++i)
			{
				const float Spreading = ((0.0058945f * (9.0f * ShotSpread) / 2)) - (0.0058945f * (9.0f * i));
				const float a = angle(Direction) + Spreading;
				new CLaser(GameWorld(), m_Pos, vec2(cosf(a), sinf(a)), GS()->Tuning()->m_LaserReach, m_pPlayer->GetCID());
			}
			GS()->CreateSound(m_Pos, SOUND_LASER_FIRE);
		} break;

		case WEAPON_NINJA:
		{
			m_Ninja.m_ActivationDir = Direction;
			m_Ninja.m_CurrentMoveTime = g_pData->m_Weapons.m_Ninja.m_Movetime * Server()->TickSpeed() / 1000;
			m_Ninja.m_OldVelAmount = length(m_Core.m_Vel);

			GS()->CreateSound(m_Pos, SOUND_NINJA_FIRE);
		} break;
	}

	m_AttackTick = Server()->Tick();

	if(m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo > 0)
		m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo--;

	if(!m_ReloadTimer)
	{
		const int ReloadArt = m_pPlayer->GetTotalAttributeValue(AttributeIdentifier::AttackSPD);
		m_ReloadTimer = g_pData->m_Weapons.m_aId[m_Core.m_ActiveWeapon].m_Firedelay * Server()->TickSpeed() / (1000 + ReloadArt);
	}
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
		const int AmmoRegenTime = (m_Core.m_ActiveWeapon == (int)WEAPON_GUN ? (Server()->TickSpeed() / 2) : (maximum(5000 - m_AmmoRegen, 1000)) / 10);
		if(m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_AmmoRegenStart < 0)
			m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_AmmoRegenStart = Server()->Tick() + AmmoRegenTime;

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
					pChar->TakeDamage(vec2(0, -10.0f), 1, m_pPlayer->GetCID(), WEAPON_NINJA);
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
				pHookedPlayer->GetCharacter()->TakeDamage({}, 1, m_ClientID, WEAPON_HAMMER);
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
			GS()->CreateExplosion(m_Core.m_HookPos, m_ClientID, WEAPON_GRENADE, 1);
	}
}

void CCharacter::AddMultipleOrbite(int Amount, int Type, int Subtype)
{
	if(!m_pMultipleOrbite)
	{
		m_pMultipleOrbite = new CMultipleOrbite(GameWorld(), this);
		m_pMultipleOrbite->SetClientID(m_pPlayer->GetCID());
	}

	m_pMultipleOrbite->Add(Amount, Type, Subtype);
}

void CCharacter::RemoveMultipleOrbite(int Amount, int Type, int Subtype) const
{
	if(!m_pMultipleOrbite)
		return;

	m_pMultipleOrbite->Remove(Amount, Type, Subtype);
}

bool CCharacter::GiveWeapon(int Weapon, int Ammo)
{
	const int WeaponID = clamp(Weapon, (int)WEAPON_HAMMER, (int)WEAPON_NINJA);
	const bool IsHammer = WeaponID == WEAPON_HAMMER;
	if(!m_pPlayer->IsEquipped((ItemFunctional)WeaponID) && !IsHammer)
	{
		if(RemoveWeapon(WeaponID) && WeaponID == m_Core.m_ActiveWeapon)
			SetWeapon(m_Core.m_aWeapons[m_LastWeapon].m_Got ? m_LastWeapon : (int)WEAPON_HAMMER);
		return false;
	}

	const int MaximumAmmo = 10 + m_pPlayer->GetTotalAttributeValue(AttributeIdentifier::Ammo);
	if(m_Core.m_aWeapons[WeaponID].m_Ammo >= MaximumAmmo)
		return false;

	const int GotAmmo = IsHammer ? -1 : m_Core.m_aWeapons[WeaponID].m_Got ? minimum(m_Core.m_aWeapons[WeaponID].m_Ammo + Ammo, MaximumAmmo) : minimum(Ammo, MaximumAmmo);
	m_Core.m_aWeapons[WeaponID].m_Got = true;
	m_Core.m_aWeapons[WeaponID].m_Ammo = GotAmmo;
	return true;
}

bool CCharacter::RemoveWeapon(int Weapon)
{
	const bool Reverse = m_Core.m_aWeapons[Weapon].m_Got;
	m_Core.m_aWeapons[Weapon].m_Got = false;
	m_Core.m_aWeapons[Weapon].m_Ammo = -1;
	return Reverse;
}

// This function sets the character's emote and its duration
void CCharacter::SetEmote(int Emote, int Sec, bool StartEmoticion)
{
	// Reset by default emote
	if(Emote == EMOTE_NORMAL)
	{
		m_EmoteType = EMOTE_NORMAL;
		m_EmoteStop = -1;
		return;
	}

	// Check if the character is alive and the emote has stopped
	if(m_EmoteStop < Server()->Tick())
	{
		// Set the emote type
		m_EmoteType = Emote;
		// Set the time when the emote should stop
		m_EmoteStop = Server()->Tick() + Sec * Server()->TickSpeed();
	}

	// Check if the emoticion should be started
	if(StartEmoticion)
	{
		// Send the corresponding emoticon based on the emote type
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
	if(!IsWorldAccessible())
		return;

	// handler's
	HandleSafeFlags();
	HandlePlayer();
	HandleTiles();
	HandleWeapons();
	HandleTuning();

	// to end the tick on the destroy caused by the change of worlds
	if(m_pTilesHandler->IsEnter(TILE_WORLD_SWAPPER))
	{
		GS()->GetWorldData()->Move(m_pPlayer);
		return;
	}

	// core
	m_Core.m_Input = m_Input;
	m_Core.Tick(true, &m_pPlayer->m_NextTuningParams);
	m_pPlayer->UpdateTempData(m_Health, m_Mana);

	// game clipped
	if(GameLayerClipped(m_Pos))
	{
		Die(m_pPlayer->GetCID(), WEAPON_SELF);
	}

	// freeze position by old core and current core
	if(length(m_NormalDoorHit) < 0.1f)
	{
		m_OlderPos = m_OldPos;
		m_OldPos = m_Core.m_Pos;
	}
}

void CCharacter::TickDeferred()
{
	if(!m_Alive)
		return;

	// door reset
	if(length(m_NormalDoorHit) >= 0.1f)
	{
		HandleDoorHit();
		ResetDoorHit();
	}

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
	m_pPlayer->SetSnapHealthTick(2);
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
	CPlayer* pKiller = GS()->GetPlayer(Killer);
	if(!pKiller || (Killer == m_ClientID))
		return;

	// initialize variables
	bool KillerIsGuardian = (pKiller->IsBot() && dynamic_cast<CPlayerBot*>(pKiller)->GetBotType() == TYPE_BOT_NPC &&
		NpcBotInfo::ms_aNpcBot[dynamic_cast<CPlayerBot*>(pKiller)->GetBotMobID()].m_Function == FUNCTION_NPC_GUARDIAN);
	bool KillerIsPlayer = !pKiller->IsBot();
	CPlayerItem* pItemGold = m_pPlayer->GetItem(itGold);

	// loss gold at death
	if(g_Config.m_SvGoldLossOnDeath)
	{
		const int LossGold = minimum(translate_to_percent_rest(pItemGold->GetValue(), (float)g_Config.m_SvGoldLossOnDeath), pItemGold->GetValue());
		if(LossGold > 0 && pItemGold->Remove(LossGold))
		{
			GS()->EntityManager()->DropItem(m_Pos, Killer >= MAX_PLAYERS ? -1 : Killer, { itGold, LossGold }, Force);
			if(KillerIsPlayer)
			{
				GS()->Chat(m_ClientID, "You lost {}% ({$}) gold, killer {}!", g_Config.m_SvGoldLossOnDeath, LossGold, Server()->ClientName(Killer));
			}
			else
			{
				GS()->Chat(m_ClientID, "You lost {}% ({$}) gold due to death!", g_Config.m_SvGoldLossOnDeath, LossGold, Server()->ClientName(Killer));
			}
		}
	}

	// Crime score system
	if(m_pPlayer->Account()->IsCrimeScoreMaxedOut() && (KillerIsGuardian || KillerIsPlayer))
	{
		const int Arrest = minimum(translate_to_percent_rest(pItemGold->GetValue(), (float)g_Config.m_SvArrestGoldOnDeath), pItemGold->GetValue());
		if(Arrest > 0 && pItemGold->Remove(Arrest))
		{
			if(KillerIsPlayer)
			{
				pKiller->Account()->AddGold(Arrest, false);
				GS()->Chat(-1, "{} killed {}, who was wanted. The reward is {$} gold!",
					Server()->ClientName(m_pPlayer->GetCID()), Server()->ClientName(Killer), Arrest);
			}

			GS()->Chat(m_ClientID, "Treasury confiscates {}% ({$}) of your gold.", g_Config.m_SvArrestGoldOnDeath, Arrest);
			m_pPlayer->Account()->GetPrisonManager().Imprison(360);
		}

		m_pPlayer->Account()->ResetCrimeScore();
	}
	else if(KillerIsPlayer)
	{
		// Increase the relations of the player identified by the "Killer" index by 25
		pKiller->Account()->IncreaseCrimeScore(25);
	}
}

void CCharacter::Die(int Killer, int Weapon)
{
	m_Alive = false;

	// a nice sound
	GS()->m_pController->OnCharacterDeath(m_pPlayer, GS()->GetPlayer(Killer), Weapon);
	GS()->CreateSound(m_Pos, SOUND_PLAYER_DIE);

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
	GS()->m_World.RemoveEntity(this);
	GS()->m_World.m_Core.m_apCharacters[m_ClientID] = nullptr;
	GS()->CreateDeath(m_Pos, m_ClientID);
}

void CCharacter::AutoUseHealingPotionIfNeeded() const
{
	// automatically use equipped heal potion if conditions are met
	if(m_pPlayer->m_aPlayerTick[PotionRecast] >= Server()->Tick() || m_Health > m_pPlayer->GetMaxHealth() / 3)
		return;

	const auto equippedHeal = m_pPlayer->GetEquippedItemID(EQUIP_POTION_HEAL);
	if(!equippedHeal)
		return;

	const auto potion = PotionTools::Heal::getHealInfo(equippedHeal.value());
	if(!potion)
		return;

	const auto pPlayerItem = m_pPlayer->GetItem(potion->getItemID());
	if(m_pPlayer->IsActiveEffect(potion->getEffect()) || !pPlayerItem->IsEquipped())
		return;

	pPlayerItem->Use(1);
}

bool CCharacter::TakeDamage(vec2 Force, int Dmg, int FromCID, int Weapon)
{
	// force
	m_Core.m_Vel += Force;
	const float MaximumVel = GS()->IsWorldType(WorldType::Dungeon) ? 16.0f : 24.0f;
	if(length(m_Core.m_Vel) > MaximumVel)
	{
		m_Core.m_Vel = normalize(m_Core.m_Vel) * MaximumVel;
	}

	// check disallow damage
	if(!IsAllowedPVP(FromCID))
		return false;

	// calculate damage
	int CritDamage = 0;
	CPlayer* pFrom = GS()->GetPlayer(FromCID);
	if(FromCID != m_pPlayer->GetCID() && pFrom->GetCharacter())
	{
		if(Weapon == WEAPON_GUN)
		{
			Dmg = pFrom->GetTotalAttributeValue(AttributeIdentifier::GunDMG);
		}
		else if(Weapon == WEAPON_SHOTGUN)
		{
			Dmg = pFrom->GetTotalAttributeValue(AttributeIdentifier::ShotgunDMG);
		}
		else if(Weapon == WEAPON_GRENADE)
		{
			Dmg = pFrom->GetTotalAttributeValue(AttributeIdentifier::GrenadeDMG);
		}
		else if(Weapon == WEAPON_LASER)
		{
			Dmg = pFrom->GetTotalAttributeValue(AttributeIdentifier::RifleDMG);
		}
		else
		{
			Dmg = pFrom->GetTotalAttributeValue(AttributeIdentifier::HammerDMG);
		}

		const int EnchantBonus = translate_to_percent_rest(pFrom->GetTotalAttributeValue(AttributeIdentifier::DMG), pFrom->GetClassData().GetExtraDMG());
		Dmg += EnchantBonus;

		// vampirism replenish your health
		if(m_pPlayer->GetAttributeChance(AttributeIdentifier::Vampirism) > random_float(100.0f))
		{
			const int Recovery = maximum(1, Dmg / 2);
			GS()->Chat(FromCID, ":: Vampirism stolen: {}HP.", Recovery);
			pFrom->GetCharacter()->IncreaseHealth(Recovery);
			GS()->SendEmoticon(FromCID, EMOTICON_DROP);
		}

		// miss out on damage
		if(m_pPlayer->GetAttributeChance(AttributeIdentifier::Lucky) > random_float(100.0f))
		{
			GS()->SendEmoticon(m_pPlayer->GetCID(), EMOTICON_HEARTS);
			return false;
		}

		// critical damage
		if(Dmg && !pFrom->IsBot() && m_pPlayer->GetAttributeChance(AttributeIdentifier::Crit) > random_float(100.0f))
		{
			CritDamage = 100 + maximum(pFrom->GetTotalAttributeValue(AttributeIdentifier::CritDMG), 1);
			const float CritDamageFormula = (float)Dmg + ((float)CritDamage * ((float)Dmg / 100.0f));
			const float CritRange = (CritDamageFormula + (CritDamageFormula / 2.0f) / 2.0f);
			Dmg = (int)CritDamageFormula + rand() % (int)CritRange;

			pFrom->GetCharacter()->SetEmote(EMOTE_ANGRY, 2, true);
		}

		// fix quick killer spread players
		if(pFrom->GetCharacter()->m_Core.m_ActiveWeapon != WEAPON_HAMMER &&
			distance(m_Core.m_Pos, pFrom->GetCharacter()->m_Core.m_Pos) < ms_PhysSize + 90.0f)
			Dmg = maximum(1, Dmg / 3);

		// give effects from player or bot to who got damage
		pFrom->GetCharacter()->GiveRandomEffects(m_pPlayer->GetCID());
		m_LastDamageTakenTick[FromCID] = Server()->Tick();
	}
	Dmg = (FromCID == m_pPlayer->GetCID() ? maximum(1, Dmg / 2) : maximum(1, Dmg));

	// update health
	const int OldHealth = m_Health;
	if(Dmg)
	{
		GS()->m_pController->OnCharacterDamage(pFrom, m_pPlayer, minimum(Dmg, m_Health));
		m_Health -= Dmg;
		GS()->MarkUpdatedBroadcast(m_pPlayer->GetCID());
		m_pPlayer->SetSnapHealthTick(2);
	}

	// create healthmod indicator & effects
	const bool IsCriticalDamage = (CritDamage > 0);
	GS()->CreateDamage(m_Pos, FromCID, OldHealth - m_Health, IsCriticalDamage);
	GS()->CreateSound(m_Pos, IsCriticalDamage ? (int)SOUND_PLAYER_PAIN_LONG : (int)SOUND_PLAYER_PAIN_SHORT);
	m_EmoteType = EMOTE_PAIN;
	m_EmoteStop = Server()->Tick() + 500 * Server()->TickSpeed() / 1000;
	if(FromCID != m_pPlayer->GetCID())
		GS()->CreatePlayerSound(FromCID, SOUND_HIT);

	// verify death
	if(m_Health <= 0)
	{
		if(FromCID != m_pPlayer->GetCID() && pFrom->GetCharacter())
			pFrom->GetCharacter()->SetEmote(EMOTE_HAPPY, 1, false);

		// do not kill the bot it is still running in CCharacterBotAI::TakeDamage
		if(m_pPlayer->IsBot())
			return false;

		m_Health = 0;
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
	if(!m_ReckoningTick || GS()->m_World.m_Paused)
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
}

void CCharacter::PostSnap()
{
	m_TriggeredEvents = 0;
}

void CCharacter::HandleTiles()
{
	// handle tilesets
	m_pTilesHandler->Handle(m_Core.m_Pos);

	// teleport
	if(m_pTilesHandler->IsActive(TILE_TELE_FROM))
	{
		if(const auto outsPos = GS()->Collision()->TryGetTeleportOut(m_Core.m_Pos))
			ChangePosition(outsPos.value());
	}

	// confirm teleport
	if(m_pTilesHandler->IsActive(TILE_TELE_FROM_CONFIRM))
	{
		if(const auto outsPos = GS()->Collision()->TryGetTeleportOut(m_Core.m_Pos))
		{
			GS()->Broadcast(m_ClientID, BroadcastPriority::TITLE_INFORMATION, Server()->TickSpeed(), "Use the hammer to enter");
			if(m_Core.m_ActiveWeapon == WEAPON_HAMMER && m_ReloadTimer)
				ChangePosition(outsPos.value());
		}
	}

	// handle locked view camera and tile interactions if the player is not a bot
	if(!m_pPlayer->IsBot())
	{
		// locked view cam
		if(const auto result = GS()->Collision()->TryGetFixedCamPos(m_Core.m_Pos))
			m_pPlayer->LockedView().ViewLock(result->first, result->second);

		// zone information
		if(m_pTilesHandler->IsActive(TILE_ZONE))
		{
			const auto pZone = GS()->Collision()->GetZonedetail(m_Core.m_Pos);
			if(pZone && m_Zonename != pZone->GetName())
			{
				const auto infoZone = fmt_default("{}. ({})", pZone->GetName(), pZone->IsPVP() ? "PVP" : "Safe");
				const int wrapLength = infoZone.length() / 2;
				const auto result = fmt_default("{}\n{}\n{}",
					mystd::aesthetic::wrapLineConfident(wrapLength), infoZone, mystd::aesthetic::wrapLineConfident(wrapLength));

				GS()->Broadcast(m_ClientID, BroadcastPriority::TITLE_INFORMATION, 150, result.c_str());
				m_Zonename = pZone->GetName();
			}
		}
		else if(m_pTilesHandler->IsExit(TILE_ZONE))
		{
			m_Zonename = "unknown";
		}

		// check from components
		GS()->Core()->OnCharacterTile(this);
	}

	// water effect enter exit
	if(m_pTilesHandler->IsEnter(TILE_WATER) || m_pTilesHandler->IsExit(TILE_WATER))
		GS()->CreateDeath(m_Core.m_Pos, m_ClientID);

	// chairs
	if(m_pTilesHandler->IsActive(TILE_CHAIR_LV1))
		m_pPlayer->Account()->HandleChair(1, 1);
	if(m_pTilesHandler->IsActive(TILE_CHAIR_LV2))
		m_pPlayer->Account()->HandleChair(3, 3);
	if(m_pTilesHandler->IsActive(TILE_CHAIR_LV3))
		m_pPlayer->Account()->HandleChair(5, 5);
}

void CCharacter::GiveRandomEffects(int To)
{
	[[maybe_unused]] CPlayer* pPlayerTo = GS()->GetPlayer(To);
	if(!pPlayerTo && To != m_pPlayer->GetCID())
		return;

	// Here effects ( buffs ) from player for TO
}

bool CCharacter::InteractiveHammer(vec2 Direction, vec2 ProjStartPos)
{
	if(m_pPlayer->IsBot())
		return false;

	if(GS()->TakeItemCharacter(m_pPlayer->GetCID()))
		return true;

	if(CEntityHarvestingItem* pJobItem = (CEntityHarvestingItem*)GameWorld()->ClosestEntity(m_Pos, 15, CGameWorld::ENTTYPE_HERVESTING_ITEM, nullptr))
	{
		pJobItem->Process(m_pPlayer->GetCID());
		m_ReloadTimer = Server()->TickSpeed() / 3;
		return true;
	}
	return false;
}

void CCharacter::HandleTuning()
{
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
};

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

	// water
	if(m_pTilesHandler->IsActive(TILE_WATER))
	{
		pTuningParams->m_Gravity = -0.05f;
		pTuningParams->m_GroundFriction = 0.95f;
		pTuningParams->m_GroundControlSpeed = 250.0f / Server()->TickSpeed();
		pTuningParams->m_GroundControlAccel = 1.5f;
		pTuningParams->m_AirFriction = 0.95f;
		pTuningParams->m_AirControlSpeed = 250.0f / Server()->TickSpeed();
		pTuningParams->m_AirControlAccel = 1.5f;
		SetEmote(EMOTE_BLINK, 1, false);
	}

	// potions and buffs are different
	HandleBuff(pTuningParams);
}

void CCharacter::HandleBuff(CTuningParams* TuningParams)
{
	if(m_pPlayer->IsActiveEffect("Slowdown"))
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

	if(m_pPlayer->IsActiveEffect("Stun"))
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

	// poisons
	if(Server()->Tick() % Server()->TickSpeed() == 0)
	{
		if(m_pPlayer->IsActiveEffect("Fire"))
		{
			const int ExplodeDamageSize = translate_to_percent_rest(m_pPlayer->GetMaxHealth(), 3);
			GS()->CreateExplosion(m_Core.m_Pos, m_pPlayer->GetCID(), WEAPON_GRENADE, 0);
			TakeDamage(vec2(0, 0), ExplodeDamageSize, m_pPlayer->GetCID(), WEAPON_SELF);
		}
		if(m_pPlayer->IsActiveEffect("Poison"))
		{
			const int PoisonSize = translate_to_percent_rest(m_pPlayer->GetMaxHealth(), 3);
			TakeDamage(vec2(0, 0), PoisonSize, m_pPlayer->GetCID(), WEAPON_SELF);
		}
		if(m_pPlayer->IsActiveEffect("RegenMana"))
		{
			const int RestoreMana = translate_to_percent_rest(m_pPlayer->GetMaxMana(), 5);
			IncreaseMana(RestoreMana);
		}

		// worker health potions
		std::ranges::for_each(PotionTools::Heal::getList(), [this](const PotionTools::Heal& p)
		{
			if(m_pPlayer->IsActiveEffect(p.getEffect()))
				IncreaseHealth(p.getRecovery());
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

void CCharacter::UpdateEquipingStats(int ItemID)
{
	if(!m_Alive || !m_pPlayer->IsAuthed())
		return;

	// health check
	if(m_Health > m_pPlayer->GetMaxHealth())
	{
		GS()->Chat(m_pPlayer->GetCID(), "Your health has been reduced.");
		GS()->Chat(m_pPlayer->GetCID(), "You may have removed equipment that gave it away.");
		m_Health = m_pPlayer->GetMaxHealth();
	}

	// mana check
	if(m_Mana > m_pPlayer->GetMaxMana())
	{
		GS()->Chat(m_pPlayer->GetCID(), "Your mana has been reduced.");
		GS()->Chat(m_pPlayer->GetCID(), "You may have removed equipment that gave it away.");
		m_Mana = m_pPlayer->GetMaxMana();
	}

	// checking and limiting the gold capacity
	int currentGold = m_pPlayer->Account()->GetGold();
	int maxGoldCapacity = m_pPlayer->Account()->GetGoldCapacity();
	if(currentGold > maxGoldCapacity)
	{
		const int excessGold = currentGold - maxGoldCapacity;
		m_pPlayer->Account()->DepositGoldToBank(excessGold);
		GS()->Chat(m_pPlayer->GetCID(), "Your gold has been reduced to the maximum capacity.");
	}

	// weapon update if the item is functional as a weapon
	CItemDescription* pItemInfo = GS()->GetItemInfo(ItemID);
	if(pItemInfo->GetFunctional() >= EQUIP_HAMMER && pItemInfo->GetFunctional() <= EQUIP_LASER)
	{
		m_pPlayer->GetCharacter()->GiveWeapon(pItemInfo->GetFunctional(), 3);
	}

	// eidolon functional processing
	if(pItemInfo->GetFunctional() == EQUIP_EIDOLON)
	{
		m_pPlayer->TryRemoveEidolon();
		m_pPlayer->TryCreateEidolon();
	}

	// update ammo regeneration
	if(pItemInfo->GetInfoEnchantStats(AttributeIdentifier::AmmoRegen) > 0)
		m_AmmoRegen = m_pPlayer->GetTotalAttributeValue(AttributeIdentifier::AmmoRegen);
}

void CCharacter::SetDoorHit(vec2 Start, vec2 End)
{
	m_NormalDoorHit = GS()->Collision()->GetDoorNormal(Start, End, m_Core.m_Pos);
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

bool CCharacter::IsAllowedPVP(int FromID) const
{
	CPlayer* pFrom = GS()->GetPlayer(FromID, false, true);

	// Dissable self damage without some item
	if(!pFrom || (FromID == m_pPlayer->GetCID() && m_pPlayer->GetItem(itDamageEqualizer)->IsEquipped()))
		return false;

	// Check if damage is disabled for the current object or the object it is interacting with
	if(m_Core.m_DamageDisabled || pFrom->GetCharacter()->m_Core.m_DamageDisabled)
		return false;

	// disable damage on safe area
	if(GS()->Collision()->GetCollisionFlagsAt(m_Core.m_Pos) & CCollision::COLFLAG_SAFE 
		|| GS()->Collision()->GetCollisionFlagsAt(pFrom->GetCharacter()->m_Core.m_Pos) & CCollision::COLFLAG_SAFE)
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
	}

	// only for unself player
	if(FromID != m_pPlayer->GetCID())
	{
		// anti pvp for guild players
		if(m_pPlayer->Account()->IsClientSameGuild(FromID))
			return false;

		// anti pvp for group players
		GroupData* pGroup = m_pPlayer->Account()->GetGroup();
		if(pGroup && pGroup->HasAccountID(pFrom->Account()->GetID()))
			return false;
	}

	return true;
}

bool CCharacter::IsWorldAccessible() const
{
	if(Server()->Tick() % Server()->TickSpeed() * 3 == 0 && m_pPlayer->IsAuthed())
	{
		// check accessible to world by level
		const auto* pAccount = m_pPlayer->Account();
		if(pAccount->GetLevel() < GS()->GetWorldData()->GetRequiredLevel())
		{
			if(!GS()->Core()->GuildManager()->GetHouseByPos(m_Core.m_Pos))
			{
				m_pPlayer->GetTempData().ClearTeleportPosition();
				GS()->Chat(m_pPlayer->GetCID(), "You were magically transported!");
				m_pPlayer->ChangeWorld(MAIN_WORLD_ID);
				return false;
			}
		}

		// check finished tutorial
		if(!pAccount->IsClassSelected() && !GS()->IsPlayerInWorld(m_ClientID, TUTORIAL_WORLD_ID))
		{
			m_pPlayer->GetTempData().ClearTeleportPosition();
			GS()->Chat(m_pPlayer->GetCID(), "You will need to take the training and select a class!");
			m_pPlayer->ChangeWorld(TUTORIAL_WORLD_ID);
			return false;
		}
	}
	return true;
}

bool CCharacter::CheckFailMana(int Mana)
{
	if(m_Mana < Mana)
	{
		GS()->Broadcast(m_pPlayer->GetCID(), BroadcastPriority::GAME_WARNING, 100, "Mana is required for the casting or continuation of this spell.");
		return true;
	}

	m_Mana -= Mana;

	// try auto use regen mana
	if(m_Mana <= m_pPlayer->GetMaxMana() / 5 && !m_pPlayer->IsActiveEffect("RegenMana") && m_pPlayer->GetItem(itPotionManaRegen)->IsEquipped())
		m_pPlayer->GetItem(itPotionManaRegen)->Use(1);

	GS()->MarkUpdatedBroadcast(m_pPlayer->GetCID());
	return false;
}

void CCharacter::ChangePosition(vec2 NewPos)
{
	if(!m_Alive)
		return;

	GS()->CreateDeath(m_Core.m_Pos, m_pPlayer->GetCID());
	GS()->CreatePlayerSpawn(NewPos);
	m_Core.m_Pos = NewPos;
	m_Pos = NewPos;
	ResetDoorHit();
	ResetHook();
}

void CCharacter::HandleDoorHit()
{
	const float dotProduct = dot(m_Core.m_Vel, m_NormalDoorHit);
	m_Core.m_Vel -= m_NormalDoorHit * dotProduct;

	if(dot(m_Core.m_Pos - m_OlderPos, m_NormalDoorHit) > 0)
		m_Core.m_Pos -= m_NormalDoorHit * dot(m_Core.m_Pos - m_OlderPos, m_NormalDoorHit);

	if(m_Core.m_Jumped >= 2)
		m_Core.m_Jumped = 1;
}

bool CCharacter::StartConversation(CPlayer* pTarget) const
{
	if(m_pPlayer->IsBot() || !pTarget->IsBot())
		return false;

	const auto pTargetBot = static_cast<CPlayerBot*>(pTarget);
	if(pTargetBot && pTargetBot->IsConversational() && pTargetBot->IsActiveForClient(m_pPlayer->GetCID()))
	{
		m_pPlayer->m_Dialog.Start(pTarget->GetCID());
		return true;
	}

	return false;
}
