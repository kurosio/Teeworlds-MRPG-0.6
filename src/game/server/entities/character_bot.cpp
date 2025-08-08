/* (c) Alexandre Díaz. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/collision.h>
#include <game/server/core/tools/path_finder.h>
#include "character_bot.h"

#include "ai_core/npc_ai.h"
#include "ai_core/eidolon_ai.h"
#include "ai_core/mob_ai.h"
#include "ai_core/quest_mob_ai.h"
#include "ai_core/quest_npc_ai.h"

#include <game/server/core/components/Bots/BotData.h>
#include <game/server/gamecontext.h>
#include <game/server/playerbot.h>

#include "pickup.h"

MACRO_ALLOC_POOL_ID_IMPL(CCharacterBotAI, MAX_CLIENTS* ENGINE_MAX_WORLDS + MAX_CLIENTS)

CCharacterBotAI::CCharacterBotAI(CGameWorld* pWorld) : CCharacter(pWorld)
{
	m_aListDmgPlayers.reserve(MAX_CLIENTS);
}

bool CCharacterBotAI::Spawn(CPlayer* pPlayer, vec2 Pos)
{
	if(CCharacter::Spawn(pPlayer, Pos))
	{
		m_pBotPlayer = static_cast<CPlayerBot*>(pPlayer);

		const int Bottype = m_pBotPlayer->GetBotType();
		if(Bottype == TYPE_BOT_NPC)
		{
			const int MobID = m_pBotPlayer->GetBotMobID();
			m_pAI = std::make_unique<CNpcAI>(&NpcBotInfo::ms_aNpcBot[MobID], m_pBotPlayer, this);
		}
		else if(Bottype == TYPE_BOT_QUEST)
		{
			const int MobID = m_pBotPlayer->GetBotMobID();
			m_pAI = std::make_unique<CQuestNpcAI>(&QuestBotInfo::ms_aQuestBot[MobID], m_pBotPlayer, this);
		}
		else if(Bottype == TYPE_BOT_QUEST_MOB)
		{
			m_pAI = std::make_unique<CQuestMobAI>(&m_pBotPlayer->GetQuestBotMobInfo(), m_pBotPlayer, this);
		}
		else if(Bottype == TYPE_BOT_MOB)
		{
			const int MobID = m_pBotPlayer->GetBotMobID();
			m_pAI = std::make_unique<CMobAI>(&m_pBotPlayer->GetMobInfo(), m_pBotPlayer, this);
		}
		else if(Bottype == TYPE_BOT_EIDOLON)
		{
			m_pAI = std::make_unique<CEidolonAI>(m_pBotPlayer, this);
		}

		m_pAI->OnSpawn();

		const bool Spawned = GS()->m_pController->OnCharacterBotSpawn(this);
		return Spawned;
	}

	return false;
}

void CCharacterBotAI::GiveRandomEffects(int ClientID)
{
	if(ClientID != m_ClientID)
	{
		m_pAI->OnGiveRandomEffect(ClientID);
	}
}

bool CCharacterBotAI::TakeDamage(vec2 Force, int Dmg, int From, int Weapon, int ForceFlag)
{
	if(!m_pBotPlayer->IsActive())
		return false;

	auto* pFrom = GS()->GetPlayer(From, false, true);
	if(!pFrom || !IsAllowedPVP(From))
		return false;

	// Between player damage disabled
	if(GS()->Collision()->IntersectLineColFlag(m_Core.m_Pos, pFrom->GetCharacter()->m_Core.m_Pos, nullptr, nullptr, CCollision::COLFLAG_DISALLOW_MOVE))
		return false;

	// Take damage
	CCharacter::TakeDamage(Force, Dmg, From, Weapon);

	// Convert from (eidolon to eidolon owner)
	const auto* pFromBot = dynamic_cast<CPlayerBot*>(pFrom);
	if(pFromBot && pFromBot->GetBotType() == TYPE_BOT_EIDOLON)
	{
		From = pFromBot->GetEidolonOwner()->GetCID();
	}

	// Add player to the list of damaged players
	if(m_aListDmgPlayers.find(From) == m_aListDmgPlayers.end())
	{
		m_aListDmgPlayers.insert(From);
	}

	m_pAI->OnTakeDamage(Dmg, From, Weapon);

	// Verify death
	if(m_Health <= 0)
	{
		m_DieForce = Force;
		Die(From, Weapon);
		return false;
	}

	return true;
}

void CCharacterBotAI::Die(int Killer, int Weapon)
{
	// Reward players
	if(Weapon != WEAPON_SELF && Weapon != WEAPON_WORLD)
	{
		for(const auto& ClientID : m_aListDmgPlayers)
		{
			CPlayer* pPlayer = GS()->GetPlayer(ClientID, true, true);
			if(!pPlayer)
				continue;

			if(!GS()->IsPlayerInWorld(ClientID, m_pBotPlayer->GetCurrentWorldID()))
				continue;

			if(distance(pPlayer->m_ViewPos, m_Core.m_Pos) > 1000.0f)
				continue;

			m_pAI->OnRewardPlayer(pPlayer, m_DieForce);
		}
	}

	// Die
	m_pAI->OnDie(Killer, Weapon);
	m_aListDmgPlayers.clear();
	m_pAI->GetTarget()->Reset();
	m_pBotPlayer->m_TargetPos = std::nullopt;
	CCharacter::Die(Killer, Weapon);
}

void CCharacterBotAI::SetForcedWeapon(int WeaponID)
{
	if(WeaponID >= WEAPON_HAMMER && WeaponID <= WEAPON_NINJA)
	{
		m_ForcedActiveWeapon = WeaponID;
		return;
	}

	dbg_msg("bot", "Invalid forced weapon id %d", WeaponID);
}

void CCharacterBotAI::ClearForcedWeapon()
{
	m_ForcedActiveWeapon.reset();
}

void CCharacterBotAI::SelectWeaponAtRandomInterval()
{
	// If an active weapon is forced, set it.
	if(m_ForcedActiveWeapon.has_value())
	{
		const int weaponID = m_ForcedActiveWeapon.value();
		m_Core.m_ActiveWeapon = clamp(weaponID, (int)WEAPON_HAMMER, (int)WEAPON_LASER);
		return;
	}

	// interval change the weapon
	if(--m_IntervalChangeWeapon <= 0)
	{
		m_IntervalChangeWeapon = 25 + rand() % 100;

		int AvailableWeapons[WEAPON_LASER + 1] {};
		int WeaponCount = 0;
		for(int i = 0; i <= WEAPON_LASER; i++)
		{
			if(i != m_Core.m_ActiveWeapon && m_pBotPlayer->IsEquippedSlot(GetEquipByWeapon(i)))
				AvailableWeapons[WeaponCount++] = i;
		}

		if(WeaponCount > 0)
		{
			m_Core.m_ActiveWeapon = AvailableWeapons[rand() % WeaponCount];
		}
	}
}

void CCharacterBotAI::SelectEmoteAtRandomInterval()
{
	const int EmotionStyle = m_pAI->GetEmotionStyle();

	if(EmotionStyle < EMOTE_PAIN || EmotionStyle > EMOTE_BLINK)
		return;

	const int emoteInterval = Server()->TickSpeed() * 3 + rand() % 10;
	if(Server()->Tick() % emoteInterval == 0)
	{
		const int duration = 1 + rand() % 2;
		SetEmote(EMOTE_BLINK, duration, true);
	}
}

bool CCharacterBotAI::IsAllowedPVP(int FromID) const
{
	CPlayer* pFrom = GS()->GetPlayer(FromID);
	if(!pFrom || FromID == m_pBotPlayer->GetCID())
		return false;

	// Skip invalid character
	const auto* pFromChar = pFrom->GetCharacter();
	if(!pFromChar)
		return false;

	// Check if damage is disabled for the current object or the object it is interacting with
	if(m_Core.m_DamageDisabled || pFromChar->m_Core.m_DamageDisabled)
		return false;

	// disable damage on safe area
	if(GS()->Collision()->GetCollisionFlagsAt(GetPos()) & CCollision::COLFLAG_SAFE
		|| GS()->Collision()->GetCollisionFlagsAt(pFromChar->GetPos()) & CCollision::COLFLAG_SAFE)
		return false;

	// skip damage intersect door
	if(GS()->Collision()->IntersectLineDoor(pFromChar->m_Core.m_Pos, m_Core.m_Pos))
		return false;

	return AI()->CanDamage(pFrom);
}

bool CCharacterBotAI::GiveWeapon(int Weapon, int GiveAmmo)
{
	if(Weapon < WEAPON_HAMMER || Weapon > WEAPON_NINJA)
		return false;

	auto EquipID = GetEquipByWeapon(Weapon);
	if(!m_pBotPlayer->IsEquippedSlot(EquipID))
	{
		RemoveWeapon(Weapon);
		return false;
	}

	m_Core.m_aWeapons[Weapon].m_Got = true;
	m_Core.m_aWeapons[Weapon].m_Ammo = GiveAmmo;
	return true;
}

void CCharacterBotAI::Tick()
{
	if(!IsAlive())
		return;

	// Check if the bot player is active
	if(!m_pBotPlayer->IsActive())
	{
		// Disable collision, hook hit, and damage
		m_Core.m_CollisionDisabled = true;
		m_Core.m_HookHitDisabled = true;
		m_Core.m_DamageDisabled = true;
		return;
	}

	HandleTuning();

	// core
	m_Core.m_Input = m_Input;
	m_Core.Tick(true, &m_pBotPlayer->m_NextTuningParams);
	m_pBotPlayer->UpdateSharedCharacterData(m_Health, m_Mana);
	ResetInput();

	// handles
	HandleSafeFlags();
	ProcessBot();
	if(!HandleTiles())
		return;

	// game clipped
	if(GameLayerClipped(m_Pos) || GetTiles()->IsEnter(TILE_DEATH))
	{
		Die(m_pBotPlayer->GetCID(), WEAPON_SELF);
		return;
	}

	ApplyMoveRestrictions();
}

void CCharacterBotAI::TickDeferred()
{
	// check active this bot
	if(!m_pBotPlayer->IsActive() || !IsAlive())
		return;

	CCharacterCore::CParams PlayerTune(&m_pBotPlayer->m_NextTuningParams);
	m_Core.Move(&PlayerTune);
	m_Core.Quantize();
	m_PrevPos = m_Pos;
	m_Pos = m_Core.m_Pos;
}

void CCharacterBotAI::Snap(int SnappingClient)
{
	int ID = m_pBotPlayer->GetCID();

	// check active this bot
	if(!m_pBotPlayer->IsActive() || m_pBotPlayer->IsActiveForClient(SnappingClient) == ESnappingPriority::None)
		return;

	// check network clipped and translate state
	if(NetworkClippedByPriority(SnappingClient, ESnappingPriority::Lower) || !Server()->Translate(ID, SnappingClient))
		return;

	CNetObj_Character* pCharacter = static_cast<CNetObj_Character*>(Server()->SnapNewItem(NETOBJTYPE_CHARACTER, ID, sizeof(CNetObj_Character)));
	if(!pCharacter)
		return;

	// write down the m_Core
	if(!m_ReckoningTick)
	{
		// no dead reckoning when paused because the client doesn't know
		// how far to perform the reckoning
		pCharacter->m_Tick = 0;
		m_Core.Write(pCharacter);
	}

	// set emote
	if(m_EmoteStop < Server()->Tick())
	{
		m_EmoteType = EMOTE_NORMAL;
		m_EmoteStop = -1;
	}
	pCharacter->m_Emote = m_EmoteType;

	// some time blink eyes
	if(250 - ((Server()->Tick() - m_LastAction) % (250)) < 5)
	{
		pCharacter->m_Emote = EMOTE_BLINK;
	}

	pCharacter->m_AttackTick = m_AttackTick;
	pCharacter->m_Direction = m_Input.m_Direction;
	pCharacter->m_Weapon = m_Core.m_ActiveWeapon;
	pCharacter->m_AmmoCount = 0;
	pCharacter->m_Health = 0;
	pCharacter->m_Armor = 0;
	pCharacter->m_PlayerFlags = m_pBotPlayer->m_PlayerFlags;

	// DDNetCharacter
	CNetObj_DDNetCharacter* pDDNetCharacter = static_cast<CNetObj_DDNetCharacter*>(Server()->SnapNewItem(NETOBJTYPE_DDNETCHARACTER, ID, sizeof(CNetObj_DDNetCharacter)));
	if(!pDDNetCharacter)
		return;

	pDDNetCharacter->m_Flags = 0;
	pDDNetCharacter->m_Jumps = m_Core.m_Jumps;
	pDDNetCharacter->m_TeleCheckpoint = 0;
	pDDNetCharacter->m_StrongWeakId = 0;
	pDDNetCharacter->m_TargetX = m_Core.m_Input.m_TargetX;
	pDDNetCharacter->m_TargetY = m_Core.m_Input.m_TargetY;
	pDDNetCharacter->m_TuneZoneOverride = m_TuneZoneOverride;
	m_pAI->OnSnapDDNetCharacter(SnappingClient, pDDNetCharacter);
}

void CCharacterBotAI::ProcessBot()
{
	if(!m_pAI->GetTarget()->IsEmpty())
		m_pAI->GetTarget()->Tick();

	m_pAI->Process();

	if(m_Input.m_Direction)
	{
		m_PrevDirection = m_Input.m_Direction;
	}

	SelectEmoteAtRandomInterval();
	HandleWeapons();
}

void CCharacterBotAI::HandleTuning()
{
	CTuningParams* pTuningParams = &m_pBotPlayer->m_NextTuningParams;
	m_pAI->OnHandleTunning(pTuningParams);
	HandleIndependentTuning();
}

void CCharacterBotAI::Move()
{
	// attempt to retrieve the path to the target
	m_pBotPlayer->m_PathHandle.TryGetPath();
	if(m_pBotPlayer->m_PathHandle.vPath.empty())
		return;

	// auto enter to teleport confirm tile (door)
	if(m_pTilesHandler->IsActive(TILE_TELE_FROM_CONFIRM))
	{
		ResetHook();
		m_Core.m_ActiveWeapon = WEAPON_HAMMER;
		m_Input.m_Fire++;
		m_LatestInput.m_Fire++;
	}

	// target pos
	vec2 TargetPos = m_pBotPlayer->m_TargetPos.value_or(m_pBotPlayer->m_PathHandle.vPath.back());

	// find the next available path point
	int Index = -1;
	int ActiveWayPoints = 0;
	vec2 WayPos = TargetPos;
	for(int i = 0; i < (int)m_pBotPlayer->m_PathHandle.vPath.size() && i < 30; i++)
	{
		if(GS()->Collision()->IntersectLineWithInvisible(m_pBotPlayer->m_PathHandle.vPath[i], m_Pos, nullptr, nullptr))
			break;
		Index = i;
		ActiveWayPoints = i;
	}
	if(Index > -1)
	{
		WayPos = m_pBotPlayer->m_PathHandle.vPath[Index];
	}

	// set aim target
	SetAim(TargetPos - m_Pos);

	// direction to waypoint
	float DistToWaypoint = distance(WayPos, m_Core.m_Pos);
	vec2 DirectionToWaypoint = DistToWaypoint > 0.1f ? normalize(WayPos - m_Core.m_Pos) : vec2(0.0f, 0.0f);

	// determine the movement direction
	int PathDirection = (ActiveWayPoints > 3) ?
		(DirectionToWaypoint.x < -0.1f ? -1 : (DirectionToWaypoint.x > 0.1f ? 1 : 0)) :
		m_PrevDirection;

	// determine the optimal distance to the target based on the active weapon
	bool HasActiveTarget = (!m_pAI->GetTarget()->IsEmpty() &&
							m_pAI->GetTarget()->GetType() == TargetType::Active &&
							!m_pAI->GetTarget()->IsCollided());
	int DistanceDirection = 0;

	if(HasActiveTarget)
	{
		// get optional distance
		float OptimalDistance = 64.0f;
		switch(m_Core.m_ActiveWeapon)
		{
			case WEAPON_GUN:      OptimalDistance = 300.0f; break;
			case WEAPON_SHOTGUN:  OptimalDistance = 400.0f; break;
			case WEAPON_GRENADE:  OptimalDistance = 500.0f; break;
			case WEAPON_LASER:    OptimalDistance = 600.0f; break;
		}

		float DistanceToTarget = distance(GetPos(), TargetPos);
		float DistanceDifference = DistanceToTarget - OptimalDistance;
		bool LineOfSightClear = !GS()->Collision()->IntersectLine(m_Core.m_Pos, TargetPos, nullptr, nullptr);

		// strafing
		int CurrentTick = Server()->Tick();
		if(CurrentTick - m_LastStrafeChangeTick > Server()->TickSpeed() * (1 + (rand() % 2)))
		{
			m_StrafeDirection = (rand() % 2) ? 1 : -1;
			m_LastStrafeChangeTick = CurrentTick;
		}

		// move to optional distance or strafe
		if(LineOfSightClear && fabs(DistanceDifference) < 80.0f)
		{
			DistanceDirection = m_StrafeDirection;
		}
		else if(LineOfSightClear && fabs(DistanceDifference) >= 80.0f)
		{
			vec2 DirToTarget = normalize(TargetPos - m_Core.m_Pos);
			DistanceDirection = (DistanceDifference > 0) ?
				(DirToTarget.x > 0 ? 1 : -1) :
				(DirToTarget.x > 0 ? -1 : 1);
		}
		else
		{
			// if there is a wall, move towards the target to navigate around it
			vec2 DirToTarget = normalize(TargetPos - m_Core.m_Pos);
			DistanceDirection = (DirToTarget.x > 0) ? 1 : -1;
		}
	}

	// set direction result
	m_Input.m_Direction = (DistanceDirection != 0) ? DistanceDirection : PathDirection;

	// reverse input direction
	if(IsCollisionFlag(CCollision::COLFLAG_DISALLOW_MOVE) && HasActiveTarget)
	{
		AI()->GetTarget()->SetType(TargetType::Lost);
		m_Input.m_Direction = -m_Input.m_Direction;
	}

	// jump ground
	const bool IsOnGround = IsGrounded();
	if((IsOnGround && DirectionToWaypoint.y < -0.5f) ||
		(!IsOnGround && DirectionToWaypoint.y < -0.5f && m_Core.m_Vel.y > 0))
	{
		m_Input.m_Jump = 1;
	}

	vec2 WallPosition;
	if(GS()->Collision()->IntersectLineWithInvisible(m_Pos, m_Pos + vec2(m_Input.m_Direction * 32.0f, 0), &WallPosition, nullptr))
	{
		float CheckHeight = IsOnGround ? -210.0f : -125.0f;
		if(GS()->Collision()->IntersectLine(WallPosition, WallPosition + vec2(0, CheckHeight), nullptr, nullptr))
			m_Input.m_Jump = 1;
	}

	// disable jump is down
	if(m_Input.m_Jump == 1 && (DirectionToWaypoint.y >= 0 || ActiveWayPoints < 3))
		m_Input.m_Jump = 0;

	// check for characters ahead to potentially jump over them
	vec2 IntersectPos;
	CCharacter* pChar = GameWorld()->IntersectCharacter(m_Core.m_Pos,
		m_Core.m_Pos + vec2(m_Input.m_Direction * 64.0f, 0), 16.0f, IntersectPos, this);
	if(pChar && !pChar->GetPlayer()->IsBot())
		m_Input.m_Jump = 1;

	// hooking
	if(ActiveWayPoints > 2 && !m_Input.m_Hook && (DirectionToWaypoint.x != 0 || DirectionToWaypoint.y != 0) && !pChar)
	{
		if(m_Core.m_HookState == HOOK_GRABBED && m_Core.m_HookedPlayer == -1)
		{
			vec2 HookVel = normalize(m_Core.m_HookPos - GetPos()) * GS()->Tuning()->m_HookDragAccel;
			if(HookVel.y > 0)
				HookVel.y *= 0.3f;
			if((HookVel.x < 0 && m_Input.m_Direction < 0) || (HookVel.x > 0 && m_Input.m_Direction > 0))
				HookVel.x *= 0.95f;
			else
				HookVel.x *= 0.75f;

			vec2 Target = vec2(m_Input.m_TargetX, m_Input.m_TargetY);
			float ps = dot(Target, HookVel);
			if(ps > 0 || (Target.y < 0 && m_Core.m_Vel.y > 0.f && m_Core.m_HookTick < SERVER_TICK_SPEED + SERVER_TICK_SPEED / 2))
				m_Input.m_Hook = 1;
			if(m_Core.m_HookTick > 4 * SERVER_TICK_SPEED || length(m_Core.m_HookPos - GetPos()) < 20.0f)
				m_Input.m_Hook = 0;
		}
		else if(m_Core.m_HookState == HOOK_FLYING)
			m_Input.m_Hook = 1;
		else if(m_LatestInput.m_Hook == 0 && m_Core.m_HookState == HOOK_IDLE && rand() % 3 == 0)
		{
			int NumDir = 32;
			vec2 HookDir(0.0f, 0.0f);
			float MaxForce = 0;
			for(int i = 0; i < NumDir; i++)
			{
				float a = 2 * i * pi / NumDir;
				vec2 dir = direction(a);
				vec2 Pos = GetPos() + dir * GS()->Tuning()->m_HookLength;

				if((GS()->Collision()->IntersectLine(GetPos(), Pos, &Pos, 0) & (CCollision::COLFLAG_SOLID | CCollision::COLFLAG_NOHOOK)) == CCollision::COLFLAG_SOLID)
				{
					vec2 HookVel = dir * GS()->Tuning()->m_HookDragAccel;
					if(HookVel.y > 0)
						HookVel.y *= 0.3f;
					if((HookVel.x < 0 && m_Input.m_Direction < 0) || (HookVel.x > 0 && m_Input.m_Direction > 0))
						HookVel.x *= 0.95f;
					else
						HookVel.x *= 0.75f;

					HookVel += vec2(0, 1) * GS()->Tuning()->m_Gravity;

					float ps = dot(DirectionToWaypoint, HookVel);
					if(ps > MaxForce)
					{
						if(GameWorld()->IntersectCharacter(GetPos(), Pos, 16.0f, IntersectPos, this))
							continue;

						MaxForce = ps;
						HookDir = Pos - GetPos();
					}
				}
			}

			if(length(HookDir) > 32.f)
			{
				SetAim(HookDir);
				m_Input.m_Hook = 1;
			}
		}
	}

	// handle cases where the bot might be stuck
	if(m_Pos.x != m_PrevPos.x)
	{
		m_MoveTick = Server()->Tick();
	}
	else if(Server()->Tick() - m_MoveTick > Server()->TickSpeed() / 2)
	{
		m_Input.m_Direction = -m_Input.m_Direction;
		m_Input.m_Jump = 1;
		m_MoveTick = Server()->Tick();
	}
}

void CCharacterBotAI::Fire()
{
	auto* pChar = GS()->GetPlayerChar(AI()->GetTarget()->GetCID());
	if(!pChar || AI()->GetTarget()->IsEmpty() || AI()->GetTarget()->IsCollided())
		return;

	// if hooking or reloading
	if((m_Input.m_Hook && m_Core.m_HookState == HOOK_IDLE) || m_ReloadTimer != 0)
		return;

	// toggle the fire button state
	m_Input.m_Fire++;
	m_LatestInput.m_Fire++;
}

void CCharacterBotAI::SetAim(vec2 Dir)
{
	m_Input.m_TargetX = (int)Dir.x;
	m_Input.m_TargetY = (int)Dir.y;
	m_LatestInput.m_TargetX = (int)Dir.x;
	m_LatestInput.m_TargetY = (int)Dir.y;
}

void CCharacterBotAI::UpdateTarget(float Radius) const
{
	if(!m_pAI->GetTarget()->IsEmpty())
	{
		const auto* pTargetChar = GS()->GetPlayerChar(m_pAI->GetTarget()->GetCID());
		if(!pTargetChar || distance(pTargetChar->GetPos(), m_Pos) > 800.0f)
		{
			m_pBotPlayer->m_TargetPos.reset();
			m_pAI->GetTarget()->Reset();
			return;
		}

		// update collided
		const bool IntersectedWithInvisibleLine = GS()->Collision()->IntersectLineWithInvisible(m_Core.m_Pos, pTargetChar->m_Core.m_Pos, nullptr, nullptr);
		m_pAI->GetTarget()->UpdateCollided(IntersectedWithInvisibleLine);

		// lost target
		if(pTargetChar->m_Core.m_DamageDisabled || IntersectedWithInvisibleLine)
		{
			if(AI()->GetTarget()->SetType(TargetType::Lost))
			{
				GS()->SendEmoticon(m_pBotPlayer->GetCID(), EMOTICON_QUESTION);
			}
		}
	}

	m_pAI->OnTargetRules(Radius);
}