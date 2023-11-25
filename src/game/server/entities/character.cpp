/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "character.h"
#include <engine/shared/config.h>
#include <generated/server_data.h>

#include <game/server/gamecontext.h>

#include "laser.h"
#include "projectile.h"

#include <game/server/mmocore/Components/Bots/BotData.h>
#include <game/server/mmocore/Components/Groups/GroupData.h>
#include <game/server/mmocore/Components/Guilds/GuildManager.h>
#include <game/server/mmocore/Components/Houses/HouseManager.h>
#include <game/server/mmocore/Components/Quests/QuestManager.h>
#include <game/server/mmocore/Components/Worlds/WorldData.h>

#include <game/server/mmocore/GameEntities/jobitems.h>
#include <game/server/mmocore/GameEntities/snapfull.h>


MACRO_ALLOC_POOL_ID_IMPL(CCharacter, MAX_CLIENTS* ENGINE_MAX_WORLDS + MAX_CLIENTS)

CCharacter::CCharacter(CGameWorld* pWorld)
	: CEntity(pWorld, CGameWorld::ENTTYPE_CHARACTER, vec2(0, 0), ms_PhysSize)
{
	m_pHelper = new TileHandle();
	m_DoorHit = false;
	m_Health = 0;
	m_TriggeredEvents = 0;
}

CCharacter::~CCharacter()
{
	delete m_pHelper;
	m_pHelper = nullptr;
	GS()->m_World.m_Core.m_apCharacters[m_pPlayer->GetCID()] = nullptr;
}

int CCharacter::GetSnapFullID() const
{
	return m_pPlayer->GetCID() * SNAPPLAYER;
}

bool CCharacter::Spawn(CPlayer* pPlayer, vec2 Pos)
{
	m_pPlayer = pPlayer;

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
	m_ReckoningTick = {};
	GS()->m_World.InsertEntity(this);
	m_Alive = true;
	m_NumInputs = 0;

	m_Mana = 0;
	m_OldPos = Pos;
	m_DamageDisabled = false;
	m_Core.m_CollisionDisabled = false;
	m_Event = TILE_CLEAR_EVENTS;
	m_Core.m_WorldID = m_pPlayer->GetPlayerWorldID();

	if(!m_pPlayer->IsBot())
	{
		m_pPlayer->m_MoodState = m_pPlayer->GetMoodState();

		GS()->Mmo()->Quest()->UpdateSteps(m_pPlayer);
		GS()->Mmo()->Quest()->AcceptNextStoryQuestStep(m_pPlayer);

		m_AmmoRegen = m_pPlayer->GetAttributeSize(AttributeIdentifier::AmmoRegen);
		GS()->UpdateVotes(m_pPlayer->GetCID(), m_pPlayer->m_CurrentVoteMenu);
		GS()->MarkUpdatedBroadcast(m_pPlayer->GetCID());
	}

	const bool Spawned = GS()->m_pController->OnCharacterSpawn(this);
	return Spawned;
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
	if(GS()->Collision()->CheckPoint(m_Pos.x + GetProximityRadius() / 2, m_Pos.y + GetProximityRadius() / 2 + 5))
		return true;
	if(GS()->Collision()->CheckPoint(m_Pos.x - GetProximityRadius() / 2, m_Pos.y + GetProximityRadius() / 2 + 5))
		return true;

	int MoveRestrictionsBelow = GS()->Collision()->GetMoveRestrictions(m_Pos + vec2(0, GetProximityRadius() / 2 + 4), 0.0f);
	return (MoveRestrictionsBelow & CANTMOVE_DOWN) != 0;
}

bool CCharacter::IsCollisionFlag(int Flag) const
{
	if(GS()->Collision()->CheckPoint(m_Pos.x + GetProximityRadius() / 2, m_Pos.y + GetProximityRadius() / 2 + 10, Flag))
		return true;
	if(GS()->Collision()->CheckPoint(m_Pos.x - GetProximityRadius() / 2, m_Pos.y + GetProximityRadius() / 2 + 10, Flag))
		return true;
	if(GS()->Collision()->CheckPoint(m_Pos.x + GetProximityRadius() / 2, m_Pos.y - GetProximityRadius() / 2 + 10, Flag))
		return true;
	if(GS()->Collision()->CheckPoint(m_Pos.x - GetProximityRadius() / 2, m_Pos.y - GetProximityRadius() / 2 + 10, Flag))
		return true;
	return false;
}

CPlayer* CCharacter::GetHookedPlayer() const
{
	if(m_Core.m_HookedPlayer > 0 && m_Core.m_HookedPlayer < MAX_CLIENTS && m_Core.m_HookState == HOOK_GRABBED)
		return GS()->m_apPlayers[m_Core.m_HookedPlayer];
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

bool CCharacter::DecoInteractive()
{
	const int ClientID = m_pPlayer->GetCID();
	if(m_pPlayer->GetTempData().m_TempDecoractionID > 0)
	{
		const int DecoID = m_pPlayer->GetTempData().m_TempDecoractionID;
		const int InteractiveType = m_pPlayer->GetTempData().m_TempDecorationType;
		m_pPlayer->GetTempData().m_TempDecoractionID = -1;
		m_pPlayer->GetTempData().m_TempDecorationType = -1;
		if(m_pPlayer->GetItem(DecoID)->GetValue() <= 0 || GS()->GetItemInfo(DecoID)->GetType() != ItemType::TYPE_DECORATION)
			return false;

		if(InteractiveType == DECORATIONS_HOUSE)
		{
			CHouseData* pHouse = m_pPlayer->Acc().GetHouse();
			if(pHouse && pHouse->AddDecoration(DecoID, GetMousePos()))
			{
				GS()->Chat(ClientID, "You have added {STR} to your house!", GS()->GetItemInfo(DecoID)->GetName());
				m_pPlayer->GetItem(DecoID)->Remove(1);
			}
		}
		else if(InteractiveType == DECORATIONS_GUILD_HOUSE)
		{
			const int GuildID = m_pPlayer->Acc().m_GuildID;
			if(GS()->Mmo()->Member()->AddDecorationHouse(DecoID, GuildID, GetMousePos()))
			{
				GS()->Chat(ClientID, "You have added {STR} to your guild house!", GS()->GetItemInfo(DecoID)->GetName());
				m_pPlayer->GetItem(DecoID)->Remove(1);
			}
			else
			{
				GS()->Chat(ClientID, "The maximum distance between the House and the Decoration is {INT} blocks!", g_Config.m_SvLimitDecoration);
				GS()->Chat(ClientID, "Setting object reset, use repeat!");
			}
		}

		GS()->UpdateVotes(ClientID, m_pPlayer->m_LastVoteMenu);
		return true;
	}
	return false;
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
		// Check if the player can interact with decorations
		if(DecoInteractive())
		{
			return;
		}

		// Check if the active weapon has no ammo
		if(!m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo)
		{
			HandleReload();
			return;
		}
	}

	const vec2 Direction = normalize(vec2(m_LatestInput.m_TargetX, m_LatestInput.m_TargetY));
	const vec2 ProjStartPos = m_Pos + Direction * GetProximityRadius() * 0.75f;
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
			const float PlayerRadius = (float)m_pPlayer->GetAttributeSize(AttributeIdentifier::HammerDMG);
			const float Radius = clamp(PlayerRadius / 5.0f, IsCharBot ? 1.7f : 3.2f, 8.0f);
			GS()->CreateSound(m_Pos, SOUND_HAMMER_FIRE);

			CCharacter* apEnts[MAX_CLIENTS];
			const int Num = GS()->m_World.FindEntities(ProjStartPos, GetProximityRadius() * Radius, (CEntity**)apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
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
					GS()->Chat(m_pPlayer->GetCID(), "You begin speaking with {STR}.", DataBotInfo::ms_aDataBot[BotID].m_aNameBot);
					break;
				}

				// dissalow hammer hit from self eidolon
				if(m_pPlayer->GetEidolon() && m_pPlayer->GetEidolon()->GetCID() == pTarget->GetPlayer()->GetCID())
					continue;

				if(pTarget->m_Core.m_CollisionDisabled)
					continue;

				if(length(pTarget->m_Pos - ProjStartPos) > 0.0f)
					GS()->CreateHammerHit(pTarget->m_Pos - normalize(pTarget->m_Pos - ProjStartPos) * GetProximityRadius() * Radius);
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
			new CProjectile(GameWorld(), WEAPON_GUN, m_pPlayer->GetCID(), ProjStartPos, Direction, (int)(Server()->TickSpeed() * GS()->Tuning()->m_GunLifetime),
				g_pData->m_Weapons.m_Gun.m_pBase->m_Damage, IsExplosive, 0, -1, WEAPON_GUN);

			GS()->CreateSound(m_Pos, SOUND_GUN_FIRE);
		} break;

		case WEAPON_SHOTGUN:
		{
			const bool IsExplosive = m_pPlayer->GetItem(itExplosiveShotgun)->IsEquipped();
			const int ShotSpread = minimum(2 + m_pPlayer->GetAttributeSize(AttributeIdentifier::SpreadShotgun), 36);
			CMsgPacker Msg(NETMSGTYPE_SV_EXTRAPROJECTILE);
			Msg.AddInt(ShotSpread);
			for(int i = 1; i <= ShotSpread; ++i)
			{
				const float Spreading = ((0.0058945f * (9.0f * ShotSpread) / 2)) - (0.0058945f * (9.0f * i));
				const float a = angle(Direction) + Spreading;
				const float Speed = (float)GS()->Tuning()->m_ShotgunSpeeddiff + random_float(0.2f);
				new CProjectile(GameWorld(), WEAPON_SHOTGUN, m_pPlayer->GetCID(), ProjStartPos,
					vec2(cosf(a), sinf(a)) * Speed,
					(int)(Server()->TickSpeed() * GS()->Tuning()->m_ShotgunLifetime),
					g_pData->m_Weapons.m_Shotgun.m_pBase->m_Damage, IsExplosive, 0, 15, WEAPON_SHOTGUN);
			}
			Server()->SendMsg(&Msg, MSGFLAG_VITAL, m_pPlayer->GetCID());
			GS()->CreateSound(m_Pos, SOUND_SHOTGUN_FIRE);
		} break;

		case WEAPON_GRENADE:
		{
			const int ShotSpread = minimum(1 + m_pPlayer->GetAttributeSize(AttributeIdentifier::SpreadGrenade), 21);
			CMsgPacker Msg(NETMSGTYPE_SV_EXTRAPROJECTILE);
			Msg.AddInt(ShotSpread);
			for(int i = 1; i < ShotSpread; ++i)
			{
				const float Spreading = ((0.0058945f * (9.0f * ShotSpread) / 2)) - (0.0058945f * (9.0f * i));
				const float a = angle(Direction) + Spreading;
				new CProjectile(GameWorld(), WEAPON_GRENADE, m_pPlayer->GetCID(), ProjStartPos,
					vec2(cosf(a), sinf(a)),
					(int)(Server()->TickSpeed() * GS()->Tuning()->m_GrenadeLifetime),
					g_pData->m_Weapons.m_Grenade.m_pBase->m_Damage, true, 0, SOUND_GRENADE_EXPLODE, WEAPON_GRENADE);
			}
			Server()->SendMsg(&Msg, MSGFLAG_VITAL, m_pPlayer->GetCID());
			GS()->CreateSound(m_Pos, SOUND_GRENADE_FIRE);
		} break;

		case WEAPON_LASER:
		{
			const int ShotSpread = minimum(1 + m_pPlayer->GetAttributeSize(AttributeIdentifier::SpreadRifle), 36);
			for(int i = 1; i < ShotSpread; ++i)
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
		const int ReloadArt = m_pPlayer->GetAttributeSize(AttributeIdentifier::AttackSPD);
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
			const int RealAmmo = 10 + m_pPlayer->GetAttributeSize(AttributeIdentifier::Ammo);
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
		GS()->Collision()->MoveBox(&m_Core.m_Pos, &m_Core.m_Vel, vec2(GetProximityRadius(), GetProximityRadius()), 0.f);

		// reset velocity so the client doesn't predict stuff
		m_Core.m_Vel = vec2(0.f, 0.f);

		int ClientID = m_pPlayer->GetCID();
		const float Radius = GetProximityRadius() * 2.0f;
		for(CCharacter* pChar = (CCharacter*)GameWorld()->FindFirst(CGameWorld::ENTTYPE_CHARACTER); pChar; pChar = (CCharacter*)pChar->TypeNext())
		{
			if(distance(pChar->m_Core.m_Pos, m_Core.m_Pos) < Radius)
			{
				if(pChar->GetPlayer()->GetCID() != ClientID && pChar->IsAllowedPVP(ClientID))
					pChar->TakeDamage(vec2(0, -10.0f), 1, m_pPlayer->GetCID(), WEAPON_NINJA);
			}
		}
	}
}

void CCharacter::HandleHookActions()
{
	if(!m_Alive)
		return;

	int ClientID = m_pPlayer->GetCID();
	CPlayer* pHookedPlayer = GetHookedPlayer();
	if(pHookedPlayer && pHookedPlayer->GetCharacter())
	{
		// poison hook :: damage increase with hammer damage
		if(Server()->Tick() % (Server()->TickSpeed() / 2) == 0)
		{
			if(m_pPlayer->GetItem(itPoisonHook)->IsEquipped())
				pHookedPlayer->GetCharacter()->TakeDamage({}, 1, ClientID, WEAPON_HAMMER);
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
			GS()->CreateExplosion(m_Core.m_HookPos, ClientID, WEAPON_GRENADE, 1);
	}
}


bool CCharacter::GiveWeapon(int Weapon, int Ammo)
{
	const int WeaponID = clamp(Weapon, (int)WEAPON_HAMMER, (int)WEAPON_NINJA);
	const bool IsHammer = WeaponID == WEAPON_HAMMER;
	if(m_pPlayer->GetEquippedItemID((ItemFunctional)WeaponID) <= 0 && !IsHammer)
	{
		if(RemoveWeapon(WeaponID) && WeaponID == m_Core.m_ActiveWeapon)
			SetWeapon(m_Core.m_aWeapons[m_LastWeapon].m_Got ? m_LastWeapon : (int)WEAPON_HAMMER);
		return false;
	}

	const int MaximumAmmo = 10 + m_pPlayer->GetAttributeSize(AttributeIdentifier::Ammo);
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
		HandleWeaponSwitch();
		FireWeapon();
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

	// check safe area
	ResetSafe();
	if(m_SafeAreaForTick || GS()->Collision()->CheckPoint(m_Core.m_Pos, CCollision::COLFLAG_SAFE_AREA))
		SetSafe();

	// check allowed world for player
	if(CheckAllowedWorld())
	{
		m_pPlayer->GetTempData().m_TempTeleportPos = vec2(-1, -1);
		GS()->Chat(m_pPlayer->GetCID(), "This chapter is still closed.");
		GS()->Chat(m_pPlayer->GetCID(), "You were magically transported to the first zone!");
		m_pPlayer->ChangeWorld(MAIN_WORLD_ID);
		return;
	}

	// handle player
	HandlePlayer();

	// handle tiles
	// safe change world data from tick
	int Index = TILE_AIR;
	HandleTilesets(&Index);
	if(GetHelper()->TileEnter(Index, TILE_WORLD_SWAP))
	{
		GS()->GetWorldData()->Move(m_pPlayer);
		return;
	}
	else if(GetHelper()->TileExit(Index, TILE_WORLD_SWAP)) { }

	// handle
	HandleWeapons();
	HandleTuning();

	// core
	m_Core.m_Input = m_Input;
	m_Core.Tick(true, &m_pPlayer->m_NextTuningParams);
	m_pPlayer->UpdateTempData(m_Health, m_Mana);

	// game clipped
	if(GameLayerClipped(m_Pos))
		Die(m_pPlayer->GetCID(), WEAPON_SELF);

	// door
	if(!m_DoorHit)
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
	if(m_DoorHit)
	{
		ResetDoorPos();
		m_DoorHit = false;
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
	if(m_Health >= m_pPlayer->GetStartHealth())
		return false;

	Amount = maximum(Amount, 1);
	m_Health = clamp(m_Health + Amount, 0, m_pPlayer->GetStartHealth());
	GS()->MarkUpdatedBroadcast(m_pPlayer->GetCID());
	m_pPlayer->SetSnapHealthTick(2);
	return true;
}

bool CCharacter::IncreaseMana(int Amount)
{
	if(m_Mana >= m_pPlayer->GetStartMana())
		return false;

	Amount = maximum(Amount, 1);
	m_Mana = clamp(m_Mana + Amount, 0, m_pPlayer->GetStartMana());
	GS()->MarkUpdatedBroadcast(m_pPlayer->GetCID());
	return true;
}

void CCharacter::HandleEventsDeath(int Killer, vec2 Force) const
{
	// Get the client ID of the player
	const int ClientID = m_pPlayer->GetCID();

	// Check if the killer player exists
	if(!GS()->m_apPlayers[Killer] || (Killer == ClientID))
		return;

	// Get the pointer to the killer player object from the game state
	CPlayer* pKiller = GS()->m_apPlayers[Killer];

	// Check if the killer is a guardian bot
	bool KillerIsGuardian = (pKiller->IsBot() && dynamic_cast<CPlayerBot*>(pKiller)->GetBotType() == TYPE_BOT_NPC &&
		NpcBotInfo::ms_aNpcBot[dynamic_cast<CPlayerBot*>(pKiller)->GetBotMobID()].m_Function == FUNCTION_NPC_GUARDIAN);

	// Check if the killer is a player
	bool KillerIsPlayer = !pKiller->IsBot();

	// Loss gold at death
	if(g_Config.m_SvLossGoldAtDeath && KillerIsPlayer)
	{
		// Get the Gold item from the player
		CPlayerItem* pItemGold = m_pPlayer->GetItem(itGold);
		const int LossGold = minimum(translate_to_percent_rest(pItemGold->GetValue(), (float)g_Config.m_SvLossGoldAtDeath), pItemGold->GetValue());

		// Swap loss gold near Killer and Player
		if(LossGold > 0 && pItemGold->Remove(LossGold))
		{
			GS()->CreateDropItem(m_Pos, Killer, { itGold, LossGold }, Force);
			GS()->Chat(ClientID, "You lost {INT}%({VAL}) gold, killer {STR}!", g_Config.m_SvLossGoldAtDeath, LossGold, Server()->ClientName(Killer));
		}
	}

	// Relationship system
	{
		if(m_pPlayer->Acc().IsRelationshipsDeterioratedToMax() && (KillerIsGuardian || KillerIsPlayer))
		{
			// Get the Gold item from the player
			CPlayerItem* pItemGold = m_pPlayer->GetItem(itGold);

			// Reset player's relations and save relations
			m_pPlayer->Acc().m_Relations = 0;
			GS()->Mmo()->SaveAccount(m_pPlayer, SAVE_RELATIONS);

			// Translate the value of the Gold item to a percentage for arrest and remove arrest
			const int Arrest = minimum(translate_to_percent_rest(pItemGold->GetValue(), (float)g_Config.m_SvArrestGoldAtDeath), pItemGold->GetValue());
			if(Arrest > 0 && pItemGold->Remove(Arrest))
			{
				// Check if the killer is not a bot
				// And add the Arrest amount to the killer's gold item
				if(KillerIsPlayer)
				{
					pKiller->GetItem(itGold)->Add(Arrest);
					GS()->Chat(-1, "{STR} killed {STR}, who was wanted. The reward is {VAL} gold!",
						Server()->ClientName(m_pPlayer->GetCID()), Server()->ClientName(Killer), Arrest);
				}

				// Send a chat message to the client with their arrest information
				GS()->Chat(ClientID, "Treasury confiscates {INT}%({VAL}) of gold.", g_Config.m_SvArrestGoldAtDeath, Arrest);
			}
		}
		else if(KillerIsPlayer)
		{
			// Increase the relations of the player identified by the "Killer" index by 25
			pKiller->IncreaseRelations(25);
		}
	}
}

void CCharacter::Die(int Killer, int Weapon)
{
	m_Alive = false;

	// change to safe zone
	const int ClientID = m_pPlayer->GetCID();
	if(Weapon != WEAPON_WORLD && !GS()->IsDungeon())
	{
		m_pPlayer->ClearEffects();
		m_pPlayer->UpdateTempData(0, 0);

		const int RespawnWorldID = GS()->GetRespawnWorld();
		if(RespawnWorldID >= 0 && GS()->m_apPlayers[Killer])
		{
			GS()->Chat(ClientID, "You've been defeated, and now you'll be healed in {STR}!", Server()->GetWorldName(RespawnWorldID));
			m_pPlayer->GetTempData().m_TempSafeSpawn = true;
		}
	}

	// a nice sound
	GS()->m_pController->OnCharacterDeath(this, GS()->m_apPlayers[Killer], Weapon);
	GS()->CreateSound(m_Pos, SOUND_PLAYER_DIE);

	// send the kill message
	CNetMsg_Sv_KillMsg Msg;
	Msg.m_Killer = Killer;
	Msg.m_Victim = m_pPlayer->GetCID();
	Msg.m_Weapon = Weapon;
	Msg.m_ModeSpecial = 0;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1, -1, m_pPlayer->GetPlayerWorldID());

	// respawn
	m_pPlayer->m_aPlayerTick[Respawn] = Server()->Tick() + Server()->TickSpeed() / 2;
	m_pPlayer->m_aPlayerTick[TickState::Die] = Server()->Tick() / 2;
	m_pPlayer->m_Spawned = true;
	GS()->m_World.RemoveEntity(this);
	GS()->m_World.m_Core.m_apCharacters[ClientID] = nullptr;
	GS()->CreateDeath(m_Pos, ClientID);
}

bool CCharacter::TakeDamage(vec2 Force, int Dmg, int FromCID, int Weapon)
{
	// force
	m_Core.m_Vel += Force;
	const float MaximumVel = GS()->IsDungeon() ? 16.0f : 24.0f;
	if(length(m_Core.m_Vel) > MaximumVel)
		m_Core.m_Vel = normalize(m_Core.m_Vel) * MaximumVel;

	// check disallow damage
	if(!IsAllowedPVP(FromCID))
		return false;

	Dmg = (FromCID == m_pPlayer->GetCID() ? maximum(1, Dmg / 2) : maximum(1, Dmg));

	int CritDamage = 0;
	CPlayer* pFrom = GS()->GetPlayer(FromCID);
	if(FromCID != m_pPlayer->GetCID() && pFrom->GetCharacter())
	{
		if(Weapon == WEAPON_GUN)
			Dmg = pFrom->GetAttributeSize(AttributeIdentifier::GunDMG);
		else if(Weapon == WEAPON_SHOTGUN)
			Dmg = pFrom->GetAttributeSize(AttributeIdentifier::ShotgunDMG);
		else if(Weapon == WEAPON_GRENADE)
			Dmg = pFrom->GetAttributeSize(AttributeIdentifier::GrenadeDMG);
		else if(Weapon == WEAPON_LASER)
			Dmg = pFrom->GetAttributeSize(AttributeIdentifier::RifleDMG);
		else
			Dmg = pFrom->GetAttributeSize(AttributeIdentifier::HammerDMG);

		const int EnchantBonus = pFrom->GetAttributeSize(AttributeIdentifier::DMG);
		Dmg += EnchantBonus;

		// vampirism replenish your health
		if(m_pPlayer->GetAttributePercent(AttributeIdentifier::Vampirism) > random_float(100.0f))
		{
			const int Recovery = maximum(1, Dmg / 2);
			GS()->Chat(FromCID, ":: Vampirism stolen: {INT}HP.", Recovery);
			pFrom->GetCharacter()->IncreaseHealth(Recovery);
			GS()->SendEmoticon(FromCID, EMOTICON_DROP);
		}

		// miss out on damage
		if(m_pPlayer->GetAttributePercent(AttributeIdentifier::Lucky) > random_float(100.0f))
		{
			GS()->SendEmoticon(m_pPlayer->GetCID(), EMOTICON_HEARTS);
			return false;
		}

		// critical damage
		if(Dmg && !pFrom->IsBot() && m_pPlayer->GetAttributePercent(AttributeIdentifier::Crit) > random_float(100.0f))
		{
			CritDamage = 100 + maximum(pFrom->GetAttributeSize(AttributeIdentifier::CritDMG), 1);
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
	}

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

	// health recovery potion worker health potions
	if(m_pPlayer->m_aPlayerTick[PotionRecast] < Server()->Tick() && !m_pPlayer->IsBot() && m_Health <= m_pPlayer->GetStartHealth() / 3)
	{
		std::for_each(PotionTools::Heal::getList().begin(), PotionTools::Heal::getList().end(), [this](const PotionTools::Heal& p)
		{
			CPlayerItem* pPlayerItem = m_pPlayer->GetItem(p.getItemID());
			if(!m_pPlayer->IsActiveEffect(p.getEffect()) && pPlayerItem->IsEquipped())
				pPlayerItem->Use(1);
		});
	}

	return true;
}

void CCharacter::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	CNetObj_Character* pCharacter = static_cast<CNetObj_Character*>(Server()->SnapNewItem(NETOBJTYPE_CHARACTER, m_pPlayer->GetCID(), sizeof(CNetObj_Character)));
	if(!pCharacter)
		return;

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
			const int MaximumAmmo = 10 + m_pPlayer->GetAttributeSize(AttributeIdentifier::Ammo);
			const int AmmoPercent = translate_to_percent(MaximumAmmo, m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo, 10.0f);
			pCharacter->m_AmmoCount = clamp(AmmoPercent, 1, 10);
		}

		if(m_Health > 0)
		{
			const int HealthPercent = translate_to_percent(m_pPlayer->GetStartHealth(), m_Health, 10.0f);
			pCharacter->m_Health = clamp(HealthPercent, 1, 10);
		}

		if(m_Mana > 0)
		{
			const int ManaPercent = translate_to_percent(m_pPlayer->GetStartMana(), m_Mana, 10.0f);
			pCharacter->m_Armor = clamp(ManaPercent, 1, 10);
		}
	}

	// DDNetCharacter
	CNetObj_DDNetCharacter* pDDNetCharacter = static_cast<CNetObj_DDNetCharacter*>(Server()->SnapNewItem(NETOBJTYPE_DDNETCHARACTER, m_pPlayer->GetCID(), sizeof(CNetObj_DDNetCharacter)));
	if(!pDDNetCharacter)
		return;

	pDDNetCharacter->m_Flags = 0;
#define DDNetFlag(flag, check) if(check) { pDDNetCharacter->m_Flags |= (flag); }
	DDNetFlag(CHARACTERFLAG_SOLO, m_Core.m_Solo)
		DDNetFlag(CHARACTERFLAG_SUPER, m_Core.m_Super)
		DDNetFlag(CHARACTERFLAG_ENDLESS_HOOK, m_Core.m_EndlessHook)
		DDNetFlag(CHARACTERFLAG_ENDLESS_JUMP, m_Core.m_EndlessJump)
		DDNetFlag(CHARACTERFLAG_JETPACK, m_Core.m_Jetpack)
		DDNetFlag(CHARACTERFLAG_COLLISION_DISABLED, (m_Core.m_CollisionDisabled || !(bool)m_pPlayer->m_NextTuningParams.m_PlayerCollision))
		DDNetFlag(CHARACTERFLAG_HOOK_HIT_DISABLED, (m_Core.m_HookHitDisabled || !(bool)m_pPlayer->m_NextTuningParams.m_PlayerHooking))
		DDNetFlag(CHARACTERFLAG_HAMMER_HIT_DISABLED, m_Core.m_HammerHitDisabled)
		DDNetFlag(CHARACTERFLAG_SHOTGUN_HIT_DISABLED, m_Core.m_ShotgunHitDisabled)
		DDNetFlag(CHARACTERFLAG_GRENADE_HIT_DISABLED, m_Core.m_GrenadeHitDisabled)
		DDNetFlag(CHARACTERFLAG_LASER_HIT_DISABLED, m_Core.m_LaserHitDisabled)
		DDNetFlag(CHARACTERFLAG_TELEGUN_GUN, m_Core.m_HasTelegunGun)
		DDNetFlag(CHARACTERFLAG_TELEGUN_GRENADE, m_Core.m_HasTelegunGrenade)
		DDNetFlag(CHARACTERFLAG_TELEGUN_LASER, m_Core.m_HasTelegunLaser)
		DDNetFlag(CHARACTERFLAG_WEAPON_HAMMER, m_Core.m_aWeapons[WEAPON_HAMMER].m_Got)
		DDNetFlag(CHARACTERFLAG_WEAPON_GUN, m_Core.m_aWeapons[WEAPON_GUN].m_Got)
		DDNetFlag(CHARACTERFLAG_WEAPON_SHOTGUN, m_Core.m_aWeapons[WEAPON_SHOTGUN].m_Got)
		DDNetFlag(CHARACTERFLAG_WEAPON_GRENADE, m_Core.m_aWeapons[WEAPON_GRENADE].m_Got)
		DDNetFlag(CHARACTERFLAG_WEAPON_LASER, m_Core.m_aWeapons[WEAPON_LASER].m_Got)
		DDNetFlag(CHARACTERFLAG_WEAPON_NINJA, m_Core.m_ActiveWeapon == WEAPON_NINJA)
		DDNetFlag(CHARACTERFLAG_MOVEMENTS_DISABLED, m_Core.m_LiveFrozen)
		DDNetFlag(CHARACTERFLAG_IN_FREEZE, m_Core.m_IsInFreeze)
		DDNetFlag(CHARACTERFLAG_PRACTICE_MODE, false)
#undef DDNetFlag

		pDDNetCharacter->m_FreezeEnd = 0;
	pDDNetCharacter->m_Jumps = m_Core.m_Jumps;
	pDDNetCharacter->m_TeleCheckpoint = 0;
	pDDNetCharacter->m_StrongWeakID = 0; // ???

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

void CCharacter::HandleTilesets(int* pIndex)
{
	// get index tileset char pos component items
	const int Tile = GS()->Collision()->GetParseTilesAt(m_Core.m_Pos.x, m_Core.m_Pos.y);
	if(pIndex)
	{
		(*pIndex) = Tile;
	}

	if(!m_pPlayer->IsBot() && GS()->Mmo()->OnPlayerHandleTile(this, Tile))
		return;

	// next for all bots & players
	for(int i = TILE_CLEAR_EVENTS; i <= TILE_EVENT_HEALTH; i++)
	{
		if(m_pHelper->TileEnter(Tile, i))
			SetEvent(i);
		else if(m_pHelper->TileExit(Tile, i)) { }
	}

	// water effect enter exit
	if(m_pHelper->TileEnter(Tile, TILE_WATER) || m_pHelper->TileExit(Tile, TILE_WATER))
	{
		GS()->CreateDeath(m_Pos, m_pPlayer->GetCID());
	}
}

void CCharacter::HandleEvent()
{
	if(m_Event == TILE_EVENT_PARTY)
	{
		SetEmote(EMOTE_HAPPY, 1, false);
		if(rand() % 50 == 0)
		{
			GS()->SendEmoticon(m_pPlayer->GetCID(), 1 + rand() % 2);
			GS()->CreateDeath(m_Core.m_Pos, m_pPlayer->GetCID());
		}
	}

	else if(m_Event == TILE_EVENT_LIKE)
	{
		SetEmote(EMOTE_HAPPY, 1, false);
	}
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

	if(CJobItems* pJobItem = (CJobItems*)GameWorld()->ClosestEntity(m_Pos, 15, CGameWorld::ENTTYPE_JOBITEMS, nullptr))
	{
		pJobItem->Work(m_pPlayer->GetCID());
		m_ReloadTimer = Server()->TickSpeed() / 3;
		return true;
	}
	return false;
}
/*
void CCharacter::InteractiveGun(vec2 Direction, vec2 ProjStartPos)
{
	return;
}

void CCharacter::InteractiveShotgun(vec2 Direction, vec2 ProjStartPos)
{
	return;
}

void CCharacter::InteractiveGrenade(vec2 Direction, vec2 ProjStartPos)
{
	return;
}

void CCharacter::InteractiveRifle(vec2 Direction, vec2 ProjStartPos)
{
	return;
}
*/
void CCharacter::HandleTuning()
{
	//CTuningParams* pTuningParams = &m_pPlayer->m_NextTuningParams;

	HandleIndependentTuning();
}

void CCharacter::HandleIndependentTuning()
{
	CTuningParams* pTuningParams = &m_pPlayer->m_NextTuningParams;

	// water
	if(m_pHelper->BoolIndex(TILE_WATER))
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

	// poisons
	if(Server()->Tick() % Server()->TickSpeed() == 0)
	{
		if(m_pPlayer->IsActiveEffect("Fire"))
		{
			const int ExplodeDamageSize = translate_to_percent_rest(m_pPlayer->GetStartHealth(), 3);
			GS()->CreateExplosion(m_Core.m_Pos, m_pPlayer->GetCID(), WEAPON_GRENADE, 0);
			TakeDamage(vec2(0, 0), ExplodeDamageSize, m_pPlayer->GetCID(), WEAPON_SELF);
		}
		if(m_pPlayer->IsActiveEffect("Poison"))
		{
			const int PoisonSize = translate_to_percent_rest(m_pPlayer->GetStartHealth(), 3);
			TakeDamage(vec2(0, 0), PoisonSize, m_pPlayer->GetCID(), WEAPON_SELF);
		}
		if(m_pPlayer->IsActiveEffect("RegenMana"))
		{
			const int RestoreMana = translate_to_percent_rest(m_pPlayer->GetStartMana(), 5);
			IncreaseMana(RestoreMana);
		}

		// worker health potions
		std::for_each(PotionTools::Heal::getList().begin(), PotionTools::Heal::getList().end(), [this](const PotionTools::Heal& p)
		{
			if(m_pPlayer->IsActiveEffect(p.getEffect()))
				IncreaseHealth(p.getRecovery());
		});
	}
}

void CCharacter::SetSafe(int FlagsDisallow)
{
	if(FlagsDisallow & CHARACTERFLAG_HAMMER_HIT_DISABLED)
		m_Core.m_HammerHitDisabled = true;
	if(FlagsDisallow & CHARACTERFLAG_COLLISION_DISABLED)
		m_Core.m_CollisionDisabled = true;
	if(FlagsDisallow & CHARACTERFLAG_HOOK_HIT_DISABLED)
		m_Core.m_HookHitDisabled = true;
	m_DamageDisabled = true;
	m_SafeAreaForTick = false;
}

void CCharacter::ResetSafe()
{
	m_Core.m_HammerHitDisabled = false;
	m_Core.m_CollisionDisabled = false;
	m_Core.m_HookHitDisabled = false;
	m_DamageDisabled = false;
}

void CCharacter::UpdateEquipingStats(int ItemID)
{
	if(!m_Alive || !m_pPlayer->IsAuthed())
		return;

	if(m_Health > m_pPlayer->GetStartHealth())
	{
		GS()->Chat(m_pPlayer->GetCID(), "Your health has been reduced.");
		GS()->Chat(m_pPlayer->GetCID(), "You may have removed equipment that gave it away.");
		m_Health = m_pPlayer->GetStartHealth();
	}

	if(m_Mana > m_pPlayer->GetStartMana())
	{
		GS()->Chat(m_pPlayer->GetCID(), "Your mana has been reduced.");
		GS()->Chat(m_pPlayer->GetCID(), "You may have removed equipment that gave it away.");
		m_Mana = m_pPlayer->GetStartMana();
	}

	CItemDescription* pItemInfo = GS()->GetItemInfo(ItemID);
	if((pItemInfo->GetFunctional() >= EQUIP_HAMMER && pItemInfo->GetFunctional() <= EQUIP_LASER))
		m_pPlayer->GetCharacter()->GiveWeapon(pItemInfo->GetFunctional(), 3);

	if(pItemInfo->GetFunctional() == EQUIP_EIDOLON)
	{
		m_pPlayer->TryRemoveEidolon();
		m_pPlayer->TryCreateEidolon();
	}

	if(pItemInfo->GetInfoEnchantStats(AttributeIdentifier::AmmoRegen) > 0)
		m_AmmoRegen = m_pPlayer->GetAttributeSize(AttributeIdentifier::AmmoRegen);
}

void CCharacter::HandlePlayer()
{
	if(!m_pPlayer->IsAuthed())
		return;

	// recovery mana
	if(m_Mana < m_pPlayer->GetStartMana() && Server()->Tick() % (Server()->TickSpeed() * 3) == 0)
		IncreaseMana(m_pPlayer->GetStartMana() / 20);

	// handle
	HandleEvent();
	HandleHookActions();
}

bool CCharacter::IsAllowedPVP(int FromID) const
{
	CPlayer* pFrom = GS()->GetPlayer(FromID, false, true);

	// Check if pFrom is null or if damage is disabled for the object or if damage is disabled for the object's character and it is not a specific type of bot
	if(!pFrom || m_DamageDisabled || (pFrom->GetCharacter()->m_DamageDisabled && pFrom->GetBotType() != TYPE_BOT_EIDOLON))
		return false;

	// Check if the sender is a bot and the bot type is TYPE_BOT_EIDOLON
	if(pFrom->GetBotType() == TYPE_BOT_EIDOLON)
	{
		// Check if the player is a bot and if the bot type is either TYPE_BOT_MOB or TYPE_BOT_QUEST_MOB and is active for the specific client with ID FromID
		// Also, check if the bot type is TYPE_BOT_NPC and the function of the NPC bot is FUNCTION_NPC_GUARDIAN
		// If any of these conditions are true, return true, otherwise return false.
		if(m_pPlayer->GetBotType() == TYPE_BOT_MOB ||
			(m_pPlayer->GetBotType() == TYPE_BOT_QUEST_MOB && dynamic_cast<CPlayerBot*>(m_pPlayer)->GetQuestBotMobInfo().m_ActiveForClient[FromID]) ||
			(m_pPlayer->GetBotType() == TYPE_BOT_NPC && NpcBotInfo::ms_aNpcBot[m_pPlayer->GetBotMobID()].m_Function == FUNCTION_NPC_GUARDIAN))
		{
			return true;
		}

		return false;
	}

	// Allow self damage without some item
	if(FromID == m_pPlayer->GetCID() && !m_pPlayer->GetItem(itRingSelfine)->IsEquipped())
		return true;

	// Allow damage if the player is a bot and is a quest mob, and the quest mob is active for the client, and the damage is coming from another player who is not a bot
	// OR if the damage is coming from another bot who is a quest mob, and the quest mob is active for the player, and the player is not a bot
	if((m_pPlayer->GetBotType() == TYPE_BOT_QUEST_MOB && dynamic_cast<CPlayerBot*>(m_pPlayer)->GetQuestBotMobInfo().m_ActiveForClient[FromID] && !pFrom->IsBot()) ||
		(pFrom->GetBotType() == TYPE_BOT_QUEST_MOB && dynamic_cast<CPlayerBot*>(pFrom)->GetQuestBotMobInfo().m_ActiveForClient[m_pPlayer->GetCID()] && !m_pPlayer->IsBot()))
	{
		return true;
	}

	// Check if the player or the sender is a NPC bot of type "Guardian"
	if((m_pPlayer->GetBotType() == TYPE_BOT_NPC && NpcBotInfo::ms_aNpcBot[m_pPlayer->GetBotMobID()].m_Function == FUNCTION_NPC_GUARDIAN) ||
		(pFrom->GetBotType() == TYPE_BOT_NPC && NpcBotInfo::ms_aNpcBot[pFrom->GetBotMobID()].m_Function == FUNCTION_NPC_GUARDIAN &&
			(m_pPlayer->IsBot() || m_pPlayer->Acc().IsRelationshipsDeterioratedToMax())))
	{
		return true;
	}

	// Dissable damage for From bot and current bot
	if(m_pPlayer->IsBot() && pFrom->IsBot())
		return false;

	// pvp only for mobs
	if((m_pPlayer->IsBot() && m_pPlayer->GetBotType() != TYPE_BOT_MOB) || (pFrom->IsBot() && pFrom->GetBotType() != TYPE_BOT_MOB))
		return false;

	// disable damage on safe area
	if(GS()->Collision()->GetTile(GetPos()) & CCollision::COLFLAG_SAFE_AREA || GS()->Collision()->GetTile(pFrom->GetCharacter()->GetPos()) & CCollision::COLFLAG_SAFE_AREA)
		return false;

	// players anti pvp
	if(!m_pPlayer->IsBot() && !pFrom->IsBot())
	{
		// anti pvp on safe world or dungeon
		if(!GS()->IsAllowedPVP() || GS()->IsDungeon())
			return false;

		// anti pvp for guild players
		if(pFrom->Acc().m_GuildID > 0 && pFrom->Acc().m_GuildID == m_pPlayer->Acc().m_GuildID)
			return false;

		// anti pvp for group players
		{
			GroupData* pGroup = m_pPlayer->Acc().GetGroup();
			if(pGroup && pGroup->HasAccountID(pFrom->Acc().GetID()))
				return false;
		}
	}

	return true;
}

bool CCharacter::CheckAllowedWorld() const
{
	if(Server()->Tick() % Server()->TickSpeed() * 3 == 0 && m_pPlayer->IsAuthed())
	{
		CQuestDescription* pQuestInfo = GS()->GetWorldData()->GetRequiredQuest();
		if(pQuestInfo && !m_pPlayer->GetQuest(pQuestInfo->GetID())->IsCompleted())
		{
			const int CheckHouseID = GS()->Mmo()->Member()->GetPosHouseID(m_Core.m_Pos);
			if(CheckHouseID <= 0)
				return true;
		}
	}
	return false;
}

bool CCharacter::CheckFailMana(int Mana)
{
	if(m_Mana < Mana)
	{
		GS()->Broadcast(m_pPlayer->GetCID(), BroadcastPriority::GAME_WARNING, 100, "Mana is required for the casting or continuation of this spell.");
		return true;
	}

	m_Mana -= Mana;
	if(m_Mana <= m_pPlayer->GetStartMana() / 5 && !m_pPlayer->IsActiveEffect("RegenMana") && m_pPlayer->GetItem(itPotionManaRegen)->IsEquipped())
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
	ResetHook();
}

void CCharacter::ResetDoorPos()
{
	m_Core.m_Pos = m_OlderPos;
	m_Core.m_Vel = vec2(0, 0);
	if(m_Core.m_Jumped >= 2)
		m_Core.m_Jumped = 1;
}

// talking system
bool CCharacter::StartConversation(CPlayer* pTarget)
{
	if(!m_pPlayer || m_pPlayer->IsBot() || !pTarget->IsBot())
		return false;

	// skip if not NPC, or it is not drawn
	CPlayerBot* pTargetBot = static_cast<CPlayerBot*>(pTarget);
	if(!pTargetBot
		|| pTargetBot->GetBotType() == TYPE_BOT_MOB
		|| pTargetBot->GetBotType() == TYPE_BOT_QUEST_MOB
		|| pTargetBot->GetBotType() == TYPE_BOT_EIDOLON
		|| (pTarget->GetBotType() == TYPE_BOT_QUEST && !QuestBotInfo::ms_aQuestBot[pTarget->GetBotMobID()].m_HasAction)
		|| (pTarget->GetBotType() == TYPE_BOT_NPC && NpcBotInfo::ms_aNpcBot[pTarget->GetBotMobID()].m_Function == FUNCTION_NPC_GUARDIAN)
		|| !pTargetBot->IsActive()
		|| !pTargetBot->IsActiveForClient(m_pPlayer->GetCID()))
		return false;

	m_pPlayer->m_Dialog.Start(m_pPlayer, pTarget->GetCID());
	return true;
}

// decoration player's
void CCharacter::CreateSnapProj(int SnapID, int Value, int TypeID, bool Dynamic, bool Projectile)
{
	CSnapFull* pSnapItem = (CSnapFull*)GameWorld()->ClosestEntity(m_Pos, 300, CGameWorld::ENTTYPE_SNAPEFFECT, nullptr);
	if(pSnapItem && pSnapItem->GetOwner() == m_pPlayer->GetCID())
	{
		pSnapItem->AddItem(Value, TypeID, Projectile, Dynamic, SnapID);
		return;
	}
	new CSnapFull(&GS()->m_World, m_Core.m_Pos, SnapID, m_pPlayer->GetCID(), Value, TypeID, Dynamic, Projectile);
}

void CCharacter::RemoveSnapProj(int Value, int SnapID, bool Effect)
{
	CSnapFull* pSnapItem = (CSnapFull*)GameWorld()->ClosestEntity(m_Pos, 300, CGameWorld::ENTTYPE_SNAPEFFECT, nullptr);
	if(pSnapItem && pSnapItem->GetOwner() == m_pPlayer->GetCID())
	{
		pSnapItem->RemoveItem(Value, SnapID, Effect);
		return;
	}
}
