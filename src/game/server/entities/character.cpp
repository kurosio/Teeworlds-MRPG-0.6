/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "character.h"
#include <engine/shared/config.h>
#include <generated/server_data.h>

#include <game/server/gamecontext.h>

#include "laser.h"
#include "projectile.h"

#include <game/server/mmocore/Components/Bots/BotData.h>
#include <game/server/mmocore/Components/Guilds/GuildCore.h>
#include <game/server/mmocore/Components/Houses/HouseCore.h>
#include <game/server/mmocore/Components/Quests/QuestCore.h>
#include <game/server/mmocore/Components/Worlds/WorldData.h>

#include <game/server/mmocore/GameEntities/jobitems.h>
#include <game/server/mmocore/GameEntities/snapfull.h>

#include "game/server/mmocore/GameEntities/quest_path_finder.h"

MACRO_ALLOC_POOL_ID_IMPL(CCharacter, MAX_CLIENTS * ENGINE_MAX_WORLDS + MAX_CLIENTS)

CCharacter::CCharacter(CGameWorld *pWorld)
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

bool CCharacter::Spawn(CPlayer *pPlayer, vec2 Pos)
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
	GS()->m_World.m_Core.m_apCharacters[m_pPlayer->GetCID()] = &m_Core;

	m_ReckoningTick = 0;
	mem_zero(&m_SendCore, sizeof(m_SendCore));
	mem_zero(&m_ReckoningCore, sizeof(m_ReckoningCore));
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

		GS()->Mmo()->Quest()->UpdateArrowStep(m_pPlayer);
		GS()->Mmo()->Quest()->AcceptNextStoryQuestStep(m_pPlayer);

		m_AmmoRegen = m_pPlayer->GetAttributeSize(AttributeIdentifier::AmmoRegen, true);
		GS()->UpdateVotes(m_pPlayer->GetCID(), m_pPlayer->m_OpenVoteMenu);
		m_pPlayer->ShowInformationStats();
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
	if(GS()->Collision()->CheckPoint(m_Pos.x+GetProximityRadius()/2, m_Pos.y+GetProximityRadius()/2+5))
		return true;
	if(GS()->Collision()->CheckPoint(m_Pos.x-GetProximityRadius()/2, m_Pos.y+GetProximityRadius()/2+5))
		return true;

	int MoveRestrictionsBelow = GS()->Collision()->GetMoveRestrictions(m_Pos + vec2(0, GetProximityRadius() / 2 + 4), 0.0f);
	return (MoveRestrictionsBelow & CANTMOVE_DOWN) != 0;
}

bool CCharacter::IsCollisionFlag(int Flag) const
{
	if(GS()->Collision()->CheckPoint(m_Pos.x+GetProximityRadius()/2, m_Pos.y+GetProximityRadius()/2+10, Flag))
		return true;
	if(GS()->Collision()->CheckPoint(m_Pos.x-GetProximityRadius()/2, m_Pos.y+GetProximityRadius()/2+10, Flag))
		return true;
	if(GS()->Collision()->CheckPoint(m_Pos.x+GetProximityRadius()/2, m_Pos.y-GetProximityRadius()/2+10, Flag))
		return true;
	if(GS()->Collision()->CheckPoint(m_Pos.x-GetProximityRadius()/2, m_Pos.y-GetProximityRadius()/2+10, Flag))
		return true;
	return false;
}

CPlayer* CCharacter::GetHookedPlayer() const
{
	if(m_Core.m_HookedPlayer > 0 && m_Core.m_HookedPlayer < MAX_CLIENTS)
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
			WantedWeapon = (WantedWeapon+1)%NUM_WEAPONS;
			if(m_Core.m_aWeapons[WantedWeapon].m_Got)
				Next--;
		}
	}

	if(Prev < 128) // make sure we only try sane stuff
	{
		while(Prev) // Prev Weapon selection
		{
			WantedWeapon = (WantedWeapon-1)<0?NUM_WEAPONS-1:WantedWeapon-1;
			if(m_Core.m_aWeapons[WantedWeapon].m_Got)
				Prev--;
		}
	}

	// Direct Weapon selection
	if(m_LatestInput.m_WantedWeapon)
		WantedWeapon = m_Input.m_WantedWeapon-1;

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

		if (InteractiveType == DECORATIONS_HOUSE)
		{
			const int HouseID = GS()->Mmo()->House()->PlayerHouseID(m_pPlayer);
			if (GS()->Mmo()->House()->AddDecorationHouse(DecoID, HouseID, GetMousePos()))
			{
				GS()->Chat(ClientID, "You added {STR}, to your house!", GS()->GetItemInfo(DecoID)->GetName());
				m_pPlayer->GetItem(DecoID)->Remove(1);
				GS()->UpdateVotes(ClientID, MENU_HOUSE_DECORATION);
				return true;
			}
		}
		else if (InteractiveType == DECORATIONS_GUILD_HOUSE)
		{
			const int GuildID = m_pPlayer->Acc().m_GuildID;
			if (GS()->Mmo()->Member()->AddDecorationHouse(DecoID, GuildID, GetMousePos()))
			{
				GS()->Chat(ClientID, "You added {STR}, to your guild house!", GS()->GetItemInfo(DecoID)->GetName());
				m_pPlayer->GetItem(DecoID)->Remove(1);
				GS()->UpdateVotes(ClientID, MENU_GUILD_HOUSE_DECORATION);
				return true;
			}
		}

		GS()->Chat(ClientID, "Distance House and Decoration maximal {INT} block!", g_Config.m_SvLimitDecoration);
		GS()->Chat(ClientID, "Setting object reset, use repeat!");
		GS()->UpdateVotes(ClientID, MENU_HOUSE_DECORATION);
		return true;
	}
	return false;
}

void CCharacter::FireWeapon()
{
	if(m_ReloadTimer != 0)
		return;

	DoWeaponSwitch();
	
	// check if we gonna auto fire
	bool FullAuto = false;
	if(m_pPlayer->GetSkill(SkillMasterWeapon)->IsLearned())
		FullAuto = true;
	
	// check if we gonna fire
	bool WillFire = false;
	if(CountInput(m_LatestPrevInput.m_Fire, m_LatestInput.m_Fire).m_Presses)
		WillFire = true;
	
	if(FullAuto && (m_LatestInput.m_Fire&1) && m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo)
		WillFire = true;

	if(!WillFire)
		return;

	const bool IsBot = m_pPlayer->IsBot();
	if(!IsBot)
	{
		if(DecoInteractive())
			return;

		if(!m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo)
		{
			m_ReloadTimer = 125 * Server()->TickSpeed() / 1000;
			if(m_LastNoAmmoSound+Server()->TickSpeed() <= Server()->Tick())
			{
				GS()->CreateSound(m_Pos, SOUND_WEAPON_NOAMMO);
				m_LastNoAmmoSound = Server()->Tick();
			}
			return;
		}
	}

	const vec2 Direction = normalize(vec2(m_LatestInput.m_TargetX, m_LatestInput.m_TargetY));
	const vec2 ProjStartPos = m_Pos+Direction*GetProximityRadius()*0.75f;
	switch(m_Core.m_ActiveWeapon)
	{
		case WEAPON_HAMMER:
		{
			if (InteractiveHammer(Direction, ProjStartPos))
			{
				m_ReloadTimer = Server()->TickSpeed() / 3;
				return;
			}

			bool Hits = false;
			bool StartedTalking = false;
			const float PlayerRadius = (float)m_pPlayer->GetAttributeSize(AttributeIdentifier::HammerPower, true);
			const float Radius = clamp(PlayerRadius / 5.0f, 1.7f, 8.0f);
			GS()->CreateSound(m_Pos, SOUND_HAMMER_FIRE);

			CCharacter *apEnts[MAX_CLIENTS];
			const int Num = GS()->m_World.FindEntities(ProjStartPos, GetProximityRadius()* Radius, (CEntity**)apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
			for (int i = 0; i < Num; ++i)
			{
				CCharacter* pTarget = apEnts[i];
				if((pTarget == this) || GS()->Collision()->IntersectLineWithInvisible(ProjStartPos, pTarget->m_Pos, nullptr, nullptr))
					continue;

				// talking wth bot
				if (!StartedTalking && StartConversation(pTarget->GetPlayer()))
				{
					m_pPlayer->SetTalking(pTarget->GetPlayer()->GetCID(), true);
					GS()->CreatePlayerSound(m_pPlayer->GetCID(), SOUND_PLAYER_SPAWN);
					GS()->CreateHammerHit(ProjStartPos);
					StartedTalking = true;
					Hits = true;

					const int BotID = pTarget->GetPlayer()->GetBotID();
					GS()->Chat(m_pPlayer->GetCID(), "You start dialogue with {STR}!", DataBotInfo::ms_aDataBot[BotID].m_aNameBot);
					break;
				}

				// dissalow hammer hit from self eidolon
				if(m_pPlayer->GetEidolon() && m_pPlayer->GetEidolon()->GetCID() == pTarget->GetPlayer()->GetCID())
					continue;

				if (pTarget->m_Core.m_CollisionDisabled)
					continue;

				if(length(pTarget->m_Pos-ProjStartPos) > 0.0f)
					GS()->CreateHammerHit(pTarget->m_Pos-normalize(pTarget->m_Pos-ProjStartPos) * GetProximityRadius() * Radius);
				else
					GS()->CreateHammerHit(ProjStartPos);

				vec2 Dir = vec2(0.f, -1.f);
				if (length(pTarget->m_Pos - m_Pos) > 0.0f)
					Dir = normalize(pTarget->m_Pos - m_Pos);

				pTarget->TakeDamage(vec2(0.f, -1.f) + normalize(Dir + vec2(0.f, -1.1f)) * 10.0f, g_pData->m_Weapons.m_Hammer.m_pBase->m_Damage, m_pPlayer->GetCID(), m_Core.m_ActiveWeapon);
				Hits = true;
			}
			if(Hits)
				m_ReloadTimer = Server()->TickSpeed()/3;
		} break;

		case WEAPON_GUN:
		{
			const bool IsExplosive = m_pPlayer->GetItem(itExplosiveGun)->IsEquipped();
			new CProjectile(GameWorld(), WEAPON_GUN, m_pPlayer->GetCID(), ProjStartPos, Direction, (int)(Server()->TickSpeed()*GS()->Tuning()->m_GunLifetime),
				g_pData->m_Weapons.m_Gun.m_pBase->m_Damage, IsExplosive, 0, -1, WEAPON_GUN);

			GS()->CreateSound(m_Pos, SOUND_GUN_FIRE);
		} break;

		case WEAPON_SHOTGUN:
		{
			const bool IsExplosive = m_pPlayer->GetItem(itExplosiveShotgun)->IsEquipped();
			const int ShotSpread = min(2 + m_pPlayer->GetAttributeSize(AttributeIdentifier::SpreadShotgun), 36);
			CMsgPacker Msg(NETMSGTYPE_SV_EXTRAPROJECTILE);
			Msg.AddInt(ShotSpread);
			for (int i = 1; i <= ShotSpread; ++i)
			{
				const float Spreading = ((0.0058945f*(9.0f*ShotSpread)/2)) - (0.0058945f*(9.0f*i));
				const float a = angle(Direction) + Spreading;
				const float Speed = (float)GS()->Tuning()->m_ShotgunSpeeddiff + frandom()*0.2f;
				new CProjectile(GameWorld(), WEAPON_SHOTGUN, m_pPlayer->GetCID(), ProjStartPos,
					vec2(cosf(a), sinf(a))*Speed,
					(int)(Server()->TickSpeed() * GS()->Tuning()->m_ShotgunLifetime),
					g_pData->m_Weapons.m_Shotgun.m_pBase->m_Damage, IsExplosive, 0, 15, WEAPON_SHOTGUN);
			}
			Server()->SendMsg(&Msg, MSGFLAG_VITAL, m_pPlayer->GetCID());
			GS()->CreateSound(m_Pos, SOUND_SHOTGUN_FIRE);
		} break;

		case WEAPON_GRENADE:
		{
			const int ShotSpread = min(1 + m_pPlayer->GetAttributeSize(AttributeIdentifier::SpreadGrenade), 21);
			CMsgPacker Msg(NETMSGTYPE_SV_EXTRAPROJECTILE);
			Msg.AddInt(ShotSpread);
			for (int i = 1; i < ShotSpread; ++i)
			{
				const float Spreading = ((0.0058945f*(9.0f*ShotSpread)/2)) - (0.0058945f*(9.0f*i));
				const float a = angle(Direction) + Spreading;
				new CProjectile(GameWorld(), WEAPON_GRENADE, m_pPlayer->GetCID(), ProjStartPos,
					vec2(cosf(a), sinf(a)),
					(int)(Server()->TickSpeed()*GS()->Tuning()->m_GrenadeLifetime),
					g_pData->m_Weapons.m_Grenade.m_pBase->m_Damage, true, 0, SOUND_GRENADE_EXPLODE, WEAPON_GRENADE);
			}
			Server()->SendMsg(&Msg, MSGFLAG_VITAL, m_pPlayer->GetCID());
			GS()->CreateSound(m_Pos, SOUND_GRENADE_FIRE);
		} break;

		case WEAPON_LASER:
		{
			const int ShotSpread = min(1 + m_pPlayer->GetAttributeSize(AttributeIdentifier::SpreadRifle), 36);
			for (int i = 1; i < ShotSpread; ++i)
			{
				const float Spreading = ((0.0058945f*(9.0f*ShotSpread)/2)) - (0.0058945f*(9.0f*i));
				const float a = angle(Direction) + Spreading;
				new CLaser(GameWorld(), m_Pos, vec2(cosf(a), sinf(a)), GS()->Tuning()->m_LaserReach, m_pPlayer->GetCID());
			}
			GS()->CreateSound(m_Pos, SOUND_LASER_FIRE);
		} break;
		case WEAPON_NINJA:
			break;
	}

	m_AttackTick = Server()->Tick();

	if(m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo > 0)
		m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo--;

	if(!m_ReloadTimer)
	{
		const int ReloadArt = m_pPlayer->GetAttributeSize(AttributeIdentifier::Dexterity);
		m_ReloadTimer = g_pData->m_Weapons.m_aId[m_Core.m_ActiveWeapon].m_Firedelay * Server()->TickSpeed() / (1000 + ReloadArt);
	}
}

void CCharacter::HandleWeapons()
{
	if(m_ReloadTimer)
	{
		m_ReloadTimer--;
		return;
	}

	FireWeapon();

	if(m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo >= 0)
	{
		const int AmmoRegenTime = (m_Core.m_ActiveWeapon == (int)WEAPON_GUN ? (Server()->TickSpeed() / 2) : (max(5000 - m_AmmoRegen, 1000)) / 10);
		if (m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_AmmoRegenStart < 0)
			m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_AmmoRegenStart = Server()->Tick() + AmmoRegenTime;

		if (m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_AmmoRegenStart <= Server()->Tick())
		{
			const int RealAmmo = 10 + m_pPlayer->GetAttributeSize(AttributeIdentifier::Ammo);
			m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo = min(m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo + 1, RealAmmo);
			m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_AmmoRegenStart = -1;
		}
	}

	HandleHookActions();
}

void CCharacter::HandleHookActions()
{
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
			float Distance = min((float)m_pPlayer->m_NextTuningParams.m_HookLength - m_pPlayer->m_NextTuningParams.m_HookFireSpeed, distance(GetMousePos(), m_Core.m_Pos));
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
		if(m_pPlayer->GetItem(itExplodeImpulseHook)->IsEquipped())
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

	const int GotAmmo = IsHammer ? -1 : m_Core.m_aWeapons[WeaponID].m_Got ? min(m_Core.m_aWeapons[WeaponID].m_Ammo + Ammo, MaximumAmmo) : min(Ammo, MaximumAmmo);
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

void CCharacter::SetEmote(int Emote, int Sec)
{
	if (m_Alive && m_EmoteStop < Server()->Tick())
	{
		m_EmoteType = Emote;
		m_EmoteStop = Server()->Tick() + Sec*Server()->TickSpeed();
	}
}

void CCharacter::OnPredictedInput(CNetObj_PlayerInput *pNewInput)
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

void CCharacter::OnDirectInput(CNetObj_PlayerInput *pNewInput)
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
	if((m_Input.m_Fire&1) != 0)
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
		GS()->Chat(m_pPlayer->GetCID(), "This chapter is still closed, you magically transported first zone!");
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
	else if(GetHelper()->TileExit(Index, TILE_WORLD_SWAP)) {}

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

	Amount = clamp(Amount, 1, Amount);
	m_Health = clamp(m_Health+Amount, 0, m_pPlayer->GetStartHealth());
	m_pPlayer->ShowInformationStats();
	m_pPlayer->SetSnapHealthTick(2);
	return true;
}

bool CCharacter::IncreaseMana(int Amount)
{
	if(m_Mana >= m_pPlayer->GetStartMana())
		return false;

	Amount = clamp(Amount, 1, Amount);
	m_Mana = clamp(m_Mana + Amount, 0, m_pPlayer->GetStartMana());
	m_pPlayer->ShowInformationStats();
	return true;
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
		if(RespawnWorldID >= 0 && !m_pPlayer->IsBot() && GS()->m_apPlayers[Killer])
		{
			// potion resurrection
			CPlayerItem* pPlayerItem = m_pPlayer->GetItem(itPotionResurrection);
			if(pPlayerItem->IsEquipped())
			{
				pPlayerItem->Use(1);
			}
			else
			{
				GS()->Chat(ClientID, "You are dead, you will be treated in {STR}", Server()->GetWorldName(RespawnWorldID));
				m_pPlayer->GetTempData().m_TempSafeSpawn = true;
			}
		}
	}

	m_pPlayer->m_aPlayerTick[Respawn] = Server()->Tick() + Server()->TickSpeed() / 2;
	if(m_pPlayer->GetBotType() == TYPE_BOT_MOB)
	{
		const int SubBotID = m_pPlayer->GetBotMobID();
		m_pPlayer->m_aPlayerTick[Respawn] = Server()->Tick() + MobBotInfo::ms_aMobBot[SubBotID].m_RespawnTick*Server()->TickSpeed();
	}

	// a nice sound
	GS()->m_pController->OnCharacterDeath(this, GS()->m_apPlayers[Killer], Weapon);
	GS()->CreateSound(m_Pos, SOUND_PLAYER_DIE);
	m_pPlayer->ClearTalking();

	// send the kill message
	CNetMsg_Sv_KillMsg Msg;
	Msg.m_Killer = Killer;
	Msg.m_Victim = m_pPlayer->GetCID();
	Msg.m_Weapon = Weapon;
	Msg.m_ModeSpecial = 0;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1, -1, GS()->GetWorldID());

	// respawn
	m_pPlayer->m_aPlayerTick[TickState::Die] = Server()->Tick()/2;
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

	Dmg = (FromCID == m_pPlayer->GetCID() ? max(1, Dmg/2) : max(1, Dmg));

	int CritDamage = 0;
	CPlayer* pFrom = GS()->GetPlayer(FromCID);
	if(FromCID != m_pPlayer->GetCID() && pFrom->GetCharacter())
	{
		if(pFrom->GetCharacter()->m_Core.m_ActiveWeapon == WEAPON_GUN)
			Dmg += pFrom->GetAttributeSize(AttributeIdentifier::GunPower, true);
		else if(pFrom->GetCharacter()->m_Core.m_ActiveWeapon == WEAPON_SHOTGUN)
			Dmg += pFrom->GetAttributeSize(AttributeIdentifier::ShotgunPower, true);
		else if(pFrom->GetCharacter()->m_Core.m_ActiveWeapon == WEAPON_GRENADE)
			Dmg += pFrom->GetAttributeSize(AttributeIdentifier::GrenadePower, true);
		else if(pFrom->GetCharacter()->m_Core.m_ActiveWeapon == WEAPON_LASER)
			Dmg += pFrom->GetAttributeSize(AttributeIdentifier::RiflePower, true);
		else
			Dmg += pFrom->GetAttributeSize(AttributeIdentifier::HammerPower, true);

		const int EnchantBonus = pFrom->GetAttributeSize(AttributeIdentifier::Strength, true);
		Dmg += EnchantBonus;

		// vampirism replenish your health
		int TempInt = pFrom->GetAttributeSize(AttributeIdentifier::Vampirism, true);
		if(min(8.0f + (float)TempInt * 0.0015f, 30.0f) > frandom() * 100.0f)
		{
			pFrom->GetCharacter()->IncreaseHealth(max(1, Dmg/2));
			GS()->SendEmoticon(FromCID, EMOTICON_DROP);
		}

		// miss out on damage
		TempInt = pFrom->GetAttributeSize(AttributeIdentifier::Lucky, true);
		if(min(5.0f + (float)TempInt * 0.0015f, 20.0f) > frandom() * 100.0f)
		{
			GS()->SendEmoticon(FromCID, EMOTICON_HEARTS);
			return false;
		}

		// critical damage
		TempInt = pFrom->GetAttributeSize(AttributeIdentifier::DirectCriticalHit, true);
		if(Dmg && !pFrom->IsBot() && min(8.0f + (float)TempInt * 0.0015f, 30.0f) > frandom() * 100.0f)
		{
			CritDamage = 100 + max(pFrom->GetAttributeSize(AttributeIdentifier::CriticalHit, true), 1);
			const float CritDamageFormula = (float)Dmg + ((float)CritDamage * ((float)Dmg / 100.0f));
			const float CritRange = (CritDamageFormula + (CritDamageFormula / 2.0f) / 2.0f);
			Dmg = (int)CritDamageFormula + random_int()%(int)CritRange;
			
			pFrom->GetCharacter()->SetEmote(EMOTE_ANGRY, 2);
			GS()->SendEmoticon(FromCID, EMOTICON_EXCLAMATION);
		}

		// fix quick killer spread players
		if(pFrom->GetCharacter()->m_Core.m_ActiveWeapon != WEAPON_HAMMER &&
			distance(m_Core.m_Pos, pFrom->GetCharacter()->m_Core.m_Pos) < ms_PhysSize+90.0f)
			Dmg = max(1, Dmg/3);

		// give effects from player or bot to who got damage
		pFrom->GetCharacter()->GiveRandomEffects(m_pPlayer->GetCID());
	}

	const int OldHealth = m_Health;
	if(Dmg)
	{
		GS()->m_pController->OnCharacterDamage(pFrom, m_pPlayer, min(Dmg, m_Health));
		m_Health -= Dmg;
		m_pPlayer->ShowInformationStats();
		m_pPlayer->SetSnapHealthTick(2);
	}

	// create healthmod indicator
	const bool IsCriticalDamage = (CritDamage > 0);
	GS()->CreateDamage(m_Pos, FromCID, OldHealth-m_Health, IsCriticalDamage);

	if(FromCID != m_pPlayer->GetCID())
		GS()->CreatePlayerSound(FromCID, SOUND_HIT);

	// verify death
	if(m_Health <= 0)
	{
		if(FromCID != m_pPlayer->GetCID() && pFrom->GetCharacter())
			pFrom->GetCharacter()->SetEmote(EMOTE_HAPPY, 1);

		// do not kill the bot it is still running in CCharacterBotAI::TakeDamage
		if(m_pPlayer->IsBot())
			return false;

		m_Health = 0;
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

	GS()->CreateSound(m_Pos, IsCriticalDamage ? (int)SOUND_PLAYER_PAIN_LONG : (int)SOUND_PLAYER_PAIN_SHORT);
	m_EmoteType = EMOTE_PAIN;
	m_EmoteStop = Server()->Tick() + 500 * Server()->TickSpeed() / 1000;
	return true;
}

void CCharacter::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	CNetObj_Character *pCharacter = static_cast<CNetObj_Character *>(Server()->SnapNewItem(NETOBJTYPE_CHARACTER, m_pPlayer->GetCID(), sizeof(CNetObj_Character)));
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
	if (m_EmoteStop < Server()->Tick())
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
		// snap quest pathfinder
		for(const auto& p : m_pPlayer->m_aQuestPathFinders)
		{
			p->PathSnap(vec2(pCharacter->m_X, pCharacter->m_Y));
		}

		if(m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo > 0)
		{
			const int MaximumAmmo = 10 + m_pPlayer->GetAttributeSize(AttributeIdentifier::Ammo);
			const int AmmoPercent =translate_to_percent(MaximumAmmo, m_Core.m_aWeapons[m_Core.m_ActiveWeapon].m_Ammo, 10.0f);
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
	CNetObj_DDNetCharacter *pDDNetCharacter = static_cast<CNetObj_DDNetCharacter *>(Server()->SnapNewItem(NETOBJTYPE_DDNETCHARACTER, m_pPlayer->GetCID(), sizeof(CNetObj_DDNetCharacter)));
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
		SetEmote(EMOTE_HAPPY, 1);
		if(random_int() % 50 == 0)
		{
			GS()->SendEmoticon(m_pPlayer->GetCID(), 1 + random_int() % 2);
			GS()->CreateDeath(m_Core.m_Pos, m_pPlayer->GetCID());
		}
	}

	else if(m_Event == TILE_EVENT_LIKE)
	{
		SetEmote(EMOTE_HAPPY, 1);
	}
}

void CCharacter::GiveRandomEffects(int To)
{
	[[maybe_unused]]CPlayer* pPlayerTo = GS()->GetPlayer(To);
	if(!pPlayerTo && To != m_pPlayer->GetCID())
		return;

	// Here effects ( buffs ) from player for TO
}

bool CCharacter::InteractiveHammer(vec2 Direction, vec2 ProjStartPos)
{
	if (m_pPlayer->IsBot())
		return false;

	if (GS()->TakeItemCharacter(m_pPlayer->GetCID()))
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
		SetEmote(EMOTE_BLINK, 1);
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
		GS()->Chat(m_pPlayer->GetCID(), "Your health has been lowered.");
		GS()->Chat(m_pPlayer->GetCID(), "You may have removed equipment that gave it away.");
		m_Health = m_pPlayer->GetStartHealth();
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
		m_AmmoRegen = m_pPlayer->GetAttributeSize(AttributeIdentifier::AmmoRegen, true);
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
}

bool CCharacter::IsAllowedPVP(int FromID) const
{
	CPlayer* pFrom = GS()->GetPlayer(FromID, false, true);

	// eidolon
	if(pFrom && pFrom->IsBot() && pFrom->GetBotType() == TYPE_BOT_EIDOLON)
	{
		// enable damage from eidolon to mobs
		if(m_pPlayer->IsBot() && m_pPlayer->GetBotType() == TYPE_BOT_MOB)
			return true;

		// dissalow  damage from self eidolon
		CPlayerBot* pFromBot = dynamic_cast<CPlayerBot*>(pFrom);
		if(pFromBot->GetEidolonOwner() && pFromBot->GetEidolonOwner()->GetCID() == m_pPlayer->GetCID())
			return false;
	}

	if(!pFrom || (m_DamageDisabled || pFrom->GetCharacter()->m_DamageDisabled) || (m_pPlayer->IsBot() && pFrom->IsBot()))
		return false;

	// pvp only for mobs
	if((m_pPlayer->IsBot() && m_pPlayer->GetBotType() != TYPE_BOT_MOB) || (pFrom->IsBot() && pFrom->GetBotType() !=
		TYPE_BOT_MOB))
		return false;

	// disable damage on invisible wall
	if(GS()->Collision()->GetParseTilesAt(GetPos().x, GetPos().y) == TILE_INVISIBLE_WALL
		|| GS()->Collision()->GetParseTilesAt(pFrom->GetCharacter()->GetPos().x, pFrom->GetCharacter()->GetPos().y) == TILE_INVISIBLE_WALL)
		return false;

	// players anti pvp
	if(!m_pPlayer->IsBot() && !pFrom->IsBot())
	{
		// anti settings pvp
		if(!pFrom->GetItem(itModePVP)->IsEquipped() || !m_pPlayer->GetItem(itModePVP)->IsEquipped())
			return false;

		// anti pvp on safe world or dungeon
		if(!GS()->IsAllowedPVP() || GS()->IsDungeon())
			return false;

		// anti pvp for guild players
		if(pFrom->Acc().m_GuildID > 0 && pFrom->Acc().m_GuildID == m_pPlayer->Acc().m_GuildID)
			return false;
	}

	// anti pvp strong
	const int FromAttributeLevel = pFrom->GetAttributesSize();
	const int PlayerAttributeLevel = m_pPlayer->GetAttributesSize();
	if(!pFrom->IsBot() && !m_pPlayer->IsBot() && ((FromAttributeLevel - PlayerAttributeLevel > g_Config.m_SvStrongAntiPVP) || (PlayerAttributeLevel - FromAttributeLevel > g_Config.m_SvStrongAntiPVP)))
		return false;

	return true;
}

bool CCharacter::CheckAllowedWorld() const
{
	if(Server()->Tick() % Server()->TickSpeed() * 3 == 0 && m_pPlayer->IsAuthed())
	{
		CQuestDataInfo* pQuestInfo = GS()->GetWorldData()->GetRequiredQuest();
		if(pQuestInfo && !m_pPlayer->GetQuest(pQuestInfo->m_QuestID).IsComplected())
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
		GS()->Broadcast(m_pPlayer->GetCID(), BroadcastPriority::GAME_WARNING, 100, "No mana for use this or for maintenance.");
		return true;
	}

	m_Mana -= Mana;
	if(m_Mana <= m_pPlayer->GetStartMana() / 5 && !m_pPlayer->IsActiveEffect("RegenMana") && m_pPlayer->GetItem(itPotionManaRegen)->IsEquipped())
		m_pPlayer->GetItem(itPotionManaRegen)->Use(1);

	m_pPlayer->ShowInformationStats();
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
	if (m_Core.m_Jumped >= 2)
		m_Core.m_Jumped = 1;
}

// talking system
bool CCharacter::StartConversation(CPlayer *pTarget)
{
	if (!m_pPlayer || m_pPlayer->IsBot() || !pTarget->IsBot())
		return false;

	// skip if not NPC, or it is not drawn
	CPlayerBot* pTargetBot = static_cast<CPlayerBot*>(pTarget);
	if (!pTargetBot || pTargetBot->GetBotType() == TYPE_BOT_MOB
		|| pTargetBot->GetBotType() == TYPE_BOT_EIDOLON
		|| (pTarget->GetBotType() == TYPE_BOT_QUEST && !QuestBotInfo::ms_aQuestBot[pTarget->GetBotMobID()].m_HasAction)
		|| !pTargetBot->IsVisibleForClient(m_pPlayer->GetCID()))
		return false;
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
