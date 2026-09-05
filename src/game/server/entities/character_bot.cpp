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

MACRO_ALLOC_POOL_ID_IMPL(CCharacterBotAI, MAX_CLIENTS * ENGINE_MAX_WORLDS + MAX_CLIENTS)

namespace AITuning
{
	constexpr float MAX_SIGHT_DISTANCE = 1000.0f;
	constexpr float RETREAT_HP_THRESHOLD = 0.25f;    // 25% HP => start retreating
	constexpr int   MEMORY_DURATION_TICKS = 200;     // 5 sec memory of target position
	constexpr int   SEARCH_DURATION_TICKS = 250;     // 6 sec search
	constexpr int   STUCK_THRESHOLD_TICKS = 50;      // 1 sec for stuck detection
	constexpr int   REACTION_MIN_TICKS = 4;          // minimum reaction time
	constexpr int   REACTION_MAX_TICKS = 25;         // maximum reaction time
	constexpr float STUCK_MIN_DISTANCE = 1.0f;       // minimum movement to not be considered stuck
	constexpr float AIM_SMOOTH_FACTOR = 0.20f;       // 0..1, the smaller — the smoother
	constexpr float HOOK_LEAD_MULTIPLIER = 800.0f;
}

CCharacterBotAI::CCharacterBotAI(CGameWorld* pWorld) : CCharacter(pWorld)
{
	m_aDamageByPlayer.reserve(MAX_CLIENTS);
}

bool CCharacterBotAI::Spawn(CPlayer* pPlayer, vec2 Pos)
{
	if (!CCharacter::Spawn(pPlayer, Pos))
		return false;

	m_pBotPlayer = static_cast<CPlayerBot*>(pPlayer);
	const int BotType = m_pBotPlayer->GetBotType();
	const int MobID = m_pBotPlayer->GetBotMobID();

	switch (BotType)
	{
	case TYPE_BOT_NPC:
		m_pAI = std::make_unique<CNpcAI>(&NpcBotInfo::ms_aNpcBot[MobID], m_pBotPlayer, this);
		break;
	case TYPE_BOT_QUEST:
		m_pAI = std::make_unique<CQuestNpcAI>(&QuestBotInfo::ms_aQuestBot[MobID], m_pBotPlayer, this);
		break;
	case TYPE_BOT_QUEST_MOB:
		m_pAI = std::make_unique<CQuestMobAI>(&m_pBotPlayer->GetQuestBotMobInfo(), m_pBotPlayer, this);
		break;
	case TYPE_BOT_MOB:
		m_pAI = std::make_unique<CMobAI>(&m_pBotPlayer->GetMobInfo(), m_pBotPlayer, this);
		break;
	case TYPE_BOT_EIDOLON:
		m_pAI = std::make_unique<CEidolonAI>(m_pBotPlayer, this);
		break;
	default:
		dbg_assert(false, "Unknown bot type spawned");
		return false;
	}

	m_AIState = AIState::Idle;
	m_StateTimer = Server()->Tick();
	m_CurrentAimDir = vec2(0.0f, 1.0f);
	m_LastPos = m_Pos;
	m_PrevTargetPos = m_Pos;
	m_ReactionTicks = AITuning::REACTION_MIN_TICKS + (rand() % (AITuning::REACTION_MAX_TICKS - AITuning::REACTION_MIN_TICKS));

	m_pAI->OnSpawn();
	return GS()->m_pController->OnCharacterBotSpawn(this);
}

void CCharacterBotAI::GiveRandomEffects(int ClientID)
{
	if (ClientID != m_ClientID)
		m_pAI->OnGiveRandomEffect(ClientID);
}

bool CCharacterBotAI::TakeDamage(vec2 Force, int Dmg, int From, int Weapon, int ForceFlag)
{
	if (!m_pBotPlayer->IsActive())
		return false;

	auto* pFrom = GS()->GetPlayer(From, false, true);
	if (!pFrom || !IsAllowedPVP(From))
		return false;

	const int HealthBefore = m_Health;
	CCharacter::TakeDamage(Force, Dmg, From, Weapon);

	if (const auto* pFromBot = dynamic_cast<CPlayerBot*>(pFrom); pFromBot && pFromBot->GetBotType() == TYPE_BOT_EIDOLON)
	{
		From = pFromBot->GetEidolonOwner()->GetCID();
	}

	const int AppliedDamage = maximum(0, HealthBefore - m_Health);
	if (AppliedDamage > 0)
	{
		m_aDamageByPlayer[From] += AppliedDamage;
		m_pAI->OnTakeDamage(AppliedDamage, From, Weapon);
		m_LastDamageTick = Server()->Tick();

		// react to damage with a pain emote and bubble emoticon if the damage ratio is significant
		const float DmgRatio = static_cast<float>(AppliedDamage) / static_cast<float>(maximum(1, m_pBotPlayer->GetMaxHealth()));
		if (DmgRatio > 0.3f && (rand() % 2 == 0))
		{
			SetEmote(EMOTE_PAIN, Server()->TickSpeed(), true);
			SendBubbleEmoticon(EMOTICON_DROP);
			m_HesitationTicks = 10;
		}
	}

	if (m_Health <= 0)
	{
		m_DieForce = Force;
		Die(From, Weapon);
		return false;
	}

	return true;
}

void CCharacterBotAI::Die(int Killer, int Weapon)
{
	if (Weapon != WEAPON_SELF && Weapon != WEAPON_WORLD)
	{
		auto* pMobInfo = m_pBotPlayer->GetBotType() == TYPE_BOT_MOB ? &m_pBotPlayer->GetMobInfo() : nullptr;
		if (pMobInfo && pMobInfo->m_Boss && !m_aDamageByPlayer.empty())
		{
			int TotalDamage = 0;
			for (const auto& [_, Dmg] : m_aDamageByPlayer)
				TotalDamage += Dmg;

			std::vector<std::pair<int, int>> vStats(m_aDamageByPlayer.begin(), m_aDamageByPlayer.end());
			std::sort(vStats.begin(), vStats.end(), [](const auto& L, const auto& R) { return L.second > R.second; });

			for (const auto& [ForCID, _] : m_aDamageByPlayer)
			{
				GS()->Chat(ForCID, "Defeated '{}' by {} players.", pMobInfo->GetName(), (int)vStats.size());
				for (const auto& [CID, Dmg] : vStats)
				{
					if (Server()->ClientIngame(CID))
					{
						const float Percent = TotalDamage > 0 ? (static_cast<float>(Dmg) / TotalDamage) * 100.0f : 0.0f;
						GS()->Chat(ForCID, "- {~} dealt {} damage ({~.2}%).", Server()->ClientName(CID), Dmg, Percent);
					}
				}
			}
		}

		const int BotWorldID = m_pBotPlayer->GetCurrentWorldID();
		for (const auto& [ClientID, _] : m_aDamageByPlayer)
		{
			auto* pPlayer = GS()->GetPlayer(ClientID, true, true);
			if (pPlayer && GS()->IsPlayerInWorld(ClientID, BotWorldID) && distance(pPlayer->m_ViewPos, m_Pos) < 1000.0f)
			{
				m_pAI->OnRewardPlayer(pPlayer, m_DieForce);
			}
		}
	}

	m_pAI->OnDie(Killer, Weapon);
	m_aDamageByPlayer.clear();
	m_pAI->GetTarget()->Reset();
	m_pBotPlayer->m_TargetPos = std::nullopt;
	CCharacter::Die(Killer, Weapon);
}

void CCharacterBotAI::SetForcedWeapon(int WeaponID)
{
	if (WeaponID >= WEAPON_HAMMER && WeaponID <= WEAPON_NINJA)
		m_ForcedActiveWeapon = WeaponID;
	else
		dbg_msg("bot", "Invalid forced weapon id %d", WeaponID);
}

void CCharacterBotAI::ClearForcedWeapon()
{
	m_ForcedActiveWeapon.reset();
}

void CCharacterBotAI::SelectWeaponAtRandomInterval()
{
	if (m_ForcedActiveWeapon.has_value())
	{
		m_Core.m_ActiveWeapon = clamp(m_ForcedActiveWeapon.value(), (int)WEAPON_HAMMER, (int)WEAPON_LASER);
		return;
	}

	auto* pTarget = m_pAI->GetTarget();
	if (pTarget && !pTarget->IsEmpty() && pTarget->GetType() == TargetType::Active)
	{
		if (auto* pTargetChar = GS()->GetPlayerChar(pTarget->GetCID()))
		{
			const float Dist = distance(m_Pos, pTargetChar->GetPos());
			int PreferredWeapon = m_Core.m_ActiveWeapon;

			if (Dist < 80.0f && m_pBotPlayer->IsEquippedSlot(GetEquipByWeapon(WEAPON_HAMMER)))
				PreferredWeapon = WEAPON_HAMMER;
			else if (Dist > 400.0f && m_pBotPlayer->IsEquippedSlot(GetEquipByWeapon(WEAPON_LASER)))
				PreferredWeapon = WEAPON_LASER;
			else if (Dist > 300.0f && m_pBotPlayer->IsEquippedSlot(GetEquipByWeapon(WEAPON_GRENADE)))
				PreferredWeapon = WEAPON_GRENADE;

			if (PreferredWeapon != m_Core.m_ActiveWeapon && (rand() % 3 == 0))
			{
				m_Core.m_ActiveWeapon = PreferredWeapon;
				return;
			}
		}
	}

	if (--m_IntervalChangeWeapon <= 0)
	{
		m_IntervalChangeWeapon = Server()->TickSpeed() + rand() % (Server()->TickSpeed() * 2);

		int AvailableWeapons[WEAPON_LASER + 1]{};
		int WeaponCount = 0;
		for (int i = 0; i <= WEAPON_LASER; i++)
		{
			if (i != m_Core.m_ActiveWeapon && m_pBotPlayer->IsEquippedSlot(GetEquipByWeapon(i)))
				AvailableWeapons[WeaponCount++] = i;
		}

		if (WeaponCount > 0)
			m_Core.m_ActiveWeapon = AvailableWeapons[rand() % WeaponCount];
	}
}

void CCharacterBotAI::SelectEmoteAtRandomInterval()
{
	const int EmotionStyle = m_pAI->GetEmotionStyle();
	if (EmotionStyle < EMOTE_PAIN || EmotionStyle > EMOTE_BLINK)
		return;

	if (Server()->Tick() % (Server()->TickSpeed() * 3 + rand() % 10) == 0)
	{
		int Emote = EMOTE_BLINK;
		switch (m_AIState)
		{
			case AIState::Chase:   Emote = EMOTE_ANGRY; break;
			case AIState::Combat:  Emote = EMOTE_ANGRY; break;
			case AIState::Retreat: Emote = EMOTE_PAIN; break;
			case AIState::Search:  Emote = EMOTE_SURPRISE; break;
			default: Emote = EMOTE_BLINK; break;
		}
		SetEmote(Emote, 1 + rand() % 2, true);
	}
}

bool CCharacterBotAI::IsAllowedPVP(int FromID) const
{
	CPlayer* pFrom = GS()->GetPlayer(FromID);
	if (!pFrom || FromID == m_pBotPlayer->GetCID())
		return false;

	const auto* pFromChar = pFrom->GetCharacter();
	if (!pFromChar)
		return false;

	if (m_Core.m_DamageDisabled || pFromChar->m_Core.m_DamageDisabled)
		return false;

	if (GS()->Collision()->GetCollisionFlagsAt(GetPos()) & CCollision::COLFLAG_SAFE ||
		GS()->Collision()->GetCollisionFlagsAt(pFromChar->GetPos()) & CCollision::COLFLAG_SAFE)
		return false;

	if (GS()->Collision()->IntersectLineDoor(pFromChar->m_Core.m_Pos, m_Core.m_Pos))
		return false;

	return AI()->CanDamage(pFrom);
}

bool CCharacterBotAI::GiveWeapon(int Weapon, int GiveAmmo)
{
	if (Weapon < WEAPON_HAMMER || Weapon > WEAPON_NINJA)
		return false;

	auto EquipID = GetEquipByWeapon(Weapon);
	if (!m_pBotPlayer->IsEquippedSlot(EquipID))
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
	if (!IsAlive())
		return;

	if (!m_pBotPlayer->IsActive())
	{
		m_Core.m_CollisionDisabled = true;
		m_Core.m_HookHitDisabled = true;
		m_Core.m_DamageDisabled = true;
		return;
	}

	HandleTuning();

	m_Core.m_Input = m_Input;
	m_Core.Tick(true, &m_pBotPlayer->m_NextTuningParams);
	m_pBotPlayer->UpdateSharedCharacterData(m_Health, m_Mana);
	ResetInput();

	HandleSafeFlags();
	ProcessBot();

	if (!HandleTiles())
		return;

	if (GameLayerClipped(m_Pos) || GetTiles()->IsEnter(TILE_DEATH))
	{
		Die(m_pBotPlayer->GetCID(), WEAPON_SELF);
		return;
	}

	ApplyMoveRestrictions();
}

void CCharacterBotAI::TickDeferred()
{
	if (!m_pBotPlayer->IsActive() || !IsAlive())
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

	if (!m_pBotPlayer->IsVisibleForClient(SnappingClient))
		return;

	if (NetworkClippedByPriority(SnappingClient, ESnappingPriority::Lower) || !Server()->Translate(ID, SnappingClient))
		return;

	CNetObj_Character* pCharacter = static_cast<CNetObj_Character*>(Server()->SnapNewItem(NETOBJTYPE_CHARACTER, ID, sizeof(CNetObj_Character)));
	if (!pCharacter)
		return;

	if (!m_ReckoningTick)
	{
		pCharacter->m_Tick = 0;
		m_Core.Write(pCharacter);
	}

	if (m_EmoteStop < Server()->Tick())
	{
		m_EmoteType = EMOTE_NORMAL;
		m_EmoteStop = -1;
	}
	pCharacter->m_Emote = m_EmoteType;

	if (250 - ((Server()->Tick() - m_LastAction) % 250) < 5)
		pCharacter->m_Emote = EMOTE_BLINK;

	pCharacter->m_AttackTick = m_AttackTick;
	pCharacter->m_Direction = m_Input.m_Direction;
	pCharacter->m_Weapon = m_Core.m_ActiveWeapon;
	pCharacter->m_AmmoCount = 0;
	pCharacter->m_Health = 0;
	pCharacter->m_Armor = 0;
	pCharacter->m_PlayerFlags = m_pBotPlayer->m_PlayerFlags;

	CNetObj_DDNetCharacter* pDDNetCharacter = static_cast<CNetObj_DDNetCharacter*>(Server()->SnapNewItem(NETOBJTYPE_DDNETCHARACTER, ID, sizeof(CNetObj_DDNetCharacter)));
	if (!pDDNetCharacter)
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
	if (!m_pAI->GetTarget()->IsEmpty())
		m_pAI->GetTarget()->Tick();

	m_pAI->Process();

	if (m_Input.m_Direction)
		m_PrevDirection = m_Input.m_Direction;

	SelectEmoteAtRandomInterval();
	HandleWeapons();
}

void CCharacterBotAI::HandleTuning()
{
	m_TuneZoneOverride = -1;
	CTuningParams* pTuningParams = &m_pBotPlayer->m_NextTuningParams;
	m_pAI->OnHandleTunning(pTuningParams);
	HandleIndependentTuning();
}

// =============================================================================
// AI MAIN LOOP - FINITE STATE MACHINE
// =============================================================================

bool CCharacterBotAI::HasLineOfSight(const vec2& From, const vec2& To) const
{
	return !GS()->Collision()->IntersectLine(From, To, nullptr, nullptr) &&
		!GS()->Collision()->IntersectLineDoor(From, To);
}

float CCharacterBotAI::GetOptimalDistance() const
{
	switch (m_Core.m_ActiveWeapon)
	{
	case WEAPON_HAMMER:  return 48.0f;
	case WEAPON_GUN:     return 300.0f;
	case WEAPON_SHOTGUN: return 350.0f;
	case WEAPON_GRENADE: return 450.0f;
	case WEAPON_LASER:   return 550.0f;
	default:             return 200.0f;
	}
}

vec2 CCharacterBotAI::CalculateLeadShotAim(const vec2& TargetPos) const
{
	float ProjectileSpeed = 0.0f;
	switch (m_Core.m_ActiveWeapon)
	{
	case WEAPON_GUN:     ProjectileSpeed = 2200.0f; break;
	case WEAPON_SHOTGUN: ProjectileSpeed = 2750.0f; break;
	case WEAPON_GRENADE: ProjectileSpeed = 1000.0f; break;
	default: return TargetPos - m_Pos;
	}

	vec2 PredictedPos = TargetPos;
	for (int i = 0; i < 2; ++i)
	{
		float Dist = distance(m_Pos, PredictedPos);
		float TimeToHit = Dist / ProjectileSpeed;
		PredictedPos = TargetPos + m_TargetVelocity * TimeToHit * Server()->TickSpeed();
	}

	return PredictedPos - m_Pos;
}

void CCharacterBotAI::ChangeState(AIState NewState)
{
	if (m_AIState == NewState) return;

	AIState OldState = m_AIState;
	m_AIState = NewState;
	m_StateTimer = Server()->Tick();
	m_ReactionTicks = AITuning::REACTION_MIN_TICKS + (rand() % (AITuning::REACTION_MAX_TICKS - AITuning::REACTION_MIN_TICKS));

	OnStateChanged(OldState, NewState);
}

void CCharacterBotAI::OnStateChanged(AIState OldState, AIState NewState)
{
	// 1. surprise: enemy suddenly appears in sight
	if ((OldState == AIState::Idle || OldState == AIState::Search) &&
		(NewState == AIState::Chase || NewState == AIState::Combat))
	{
		// shoot emote and bubble emoticon to indicate surprise
		m_HesitationTicks = 15 + (rand() % 10);
		SetEmote(EMOTE_SURPRISE, m_HesitationTicks, true);
		SendBubbleEmoticon(EMOTICON_EXCLAMATION); // "!"
	}

	// 1. enemy lost: bot was chasing or fighting, but now has to search for the target
	if ((OldState == AIState::Chase || OldState == AIState::Combat) && NewState == AIState::Search)
	{
		SetEmote(EMOTE_NORMAL, Server()->TickSpeed(), true);
		SendBubbleEmoticon(EMOTICON_QUESTION); // "?"
		m_LookAroundTimer = 0;
	}

	// 3. retreat: bot is retreating from the enemy
	if (NewState == AIState::Retreat)
	{
		SetEmote(EMOTE_PAIN, Server()->TickSpeed() * 2, true);
		if (rand() % 2 == 0)
			SendBubbleEmoticon(EMOTICON_GHOST); // ghost
	}

	// 4. enemy lost: bot has lost sight of the target
	if (OldState == AIState::Search && NewState == AIState::Idle)
	{
		SetEmote(EMOTE_ANGRY, Server()->TickSpeed(), true);
		SendBubbleEmoticon(EMOTICON_WTF); // "WTF?!"
	}
}


void CCharacterBotAI::UpdateAIState(bool HasTarget, bool LineOfSight, float DistToTarget)
{
	const int CurrentTick = Server()->Tick();

	auto* pMobInfo = m_pBotPlayer->GetBotType() == TYPE_BOT_MOB ? &m_pBotPlayer->GetMobInfo() : nullptr;
	const bool CanRetreat = pMobInfo && !pMobInfo->m_Boss;
	const float HpRatio = static_cast<float>(m_Health) / static_cast<float>(maximum(1, m_pBotPlayer->GetMaxHealth()));

	if (CanRetreat && HasTarget && HpRatio < AITuning::RETREAT_HP_THRESHOLD)
	{
		ChangeState(AIState::Retreat);
		return;
	}

	if (HasTarget && LineOfSight)
	{
		m_LastSeenTargetTick = CurrentTick;

		if (DistToTarget < GetOptimalDistance() * 1.5f)
			ChangeState(AIState::Combat);
		else
			ChangeState(AIState::Chase);
		return;
	}

	if (m_LastKnownTargetPos.has_value())
	{
		const int TicksSinceSeen = CurrentTick - m_LastSeenTargetTick;
		if (TicksSinceSeen < AITuning::MEMORY_DURATION_TICKS)
		{
			ChangeState(AIState::Chase);
			return;
		}
		if (TicksSinceSeen < AITuning::MEMORY_DURATION_TICKS + AITuning::SEARCH_DURATION_TICKS)
		{
			ChangeState(AIState::Search);
			return;
		}

		m_LastKnownTargetPos.reset();
	}

	if (m_StuckCount > AITuning::STUCK_THRESHOLD_TICKS)
	{
		ChangeState(AIState::Stuck);
		return;
	}

	ChangeState(AIState::Idle);
}

void CCharacterBotAI::Move()
{
	m_pBotPlayer->m_PathHandle.TryGetPath();
	const auto& Path = m_pBotPlayer->m_PathHandle.vPath;
	if (Path.empty()) return;

	if (m_pTilesHandler->IsActive(TILE_TELE_FROM_CONFIRM))
	{
		ResetHook();
		m_Core.m_ActiveWeapon = WEAPON_HAMMER;
		m_Input.m_Fire++;
		m_LatestInput.m_Fire++;
	}

	auto* pTarget = m_pAI->GetTarget();
	const bool HasActiveTarget = (pTarget && !pTarget->IsEmpty() &&
		pTarget->GetType() == TargetType::Active);

	vec2 TargetPos = m_pBotPlayer->m_TargetPos.value_or(Path.back());
	CCharacter* pTargetChar = nullptr;

	if (HasActiveTarget)
	{
		pTargetChar = GS()->GetPlayerChar(pTarget->GetCID());
		if (pTargetChar)
		{
			m_TargetVelocity = (pTargetChar->GetPos() - m_PrevTargetPos) / Server()->TickSpeed();
			m_PrevTargetPos = pTargetChar->GetPos();
			TargetPos = pTargetChar->GetPos();
			m_LastKnownTargetPos = TargetPos;
		}
	}

	const bool LineOfSight = pTargetChar ? HasLineOfSight(m_Pos, pTargetChar->GetPos()) : false;
	const float DistToTarget = pTargetChar ? distance(m_Pos, pTargetChar->GetPos()) : std::numeric_limits<float>::max();

	UpdateAIState(pTargetChar != nullptr, LineOfSight, DistToTarget);

	int ActiveWayPoints = 0;
	vec2 WayPos = TargetPos;

	for (size_t i = 0; i < Path.size() && i < 30; ++i)
	{
		if (GS()->Collision()->IntersectLineWithInvisible(Path[i], m_Pos, nullptr, nullptr) ||
			GS()->Collision()->IntersectLineDoor(Path[i], m_Pos))
			break;

		ActiveWayPoints = static_cast<int>(i);
		WayPos = Path[i];
	}

	if (pTargetChar && LineOfSight)
	{
		const vec2 AimDir = CalculateLeadShotAim(pTargetChar->GetPos());
		SetAim(AimDir, false);
	}
	else
	{
		SetAim(TargetPos - m_Pos, false);
	}

	ExecuteMovement(WayPos, ActiveWayPoints, TargetPos);

	const vec2 DirToWaypoint = distance(WayPos, m_Pos) > 0.1f ? normalize(WayPos - m_Pos) : vec2(0.f, 0.f);
	UpdateJumping(DirToWaypoint, ActiveWayPoints);
	UpdateHooking(DirToWaypoint, ActiveWayPoints, pTargetChar != nullptr, TargetPos);
	HandleAntiStuck();
}

void CCharacterBotAI::ExecuteMovement(const vec2& WayPos, int ActiveWayPoints, const vec2& TargetPos)
{
	const int CurrentTick = Server()->Tick();
	int WantedDir = m_PrevDirection;

	const float DistToWaypoint = distance(WayPos, m_Pos);
	if (ActiveWayPoints > 3 && DistToWaypoint > 10.0f)
	{
		const vec2 DirToWaypoint = normalize(WayPos - m_Pos);
		WantedDir = (DirToWaypoint.x < -0.1f) ? -1 : (DirToWaypoint.x > 0.1f ? 1 : 0);
	}

	switch (m_AIState)
	{
	case AIState::Combat:
	{
		const float OptimalDist = GetOptimalDistance();
		const float DistToTarget = distance(m_Pos, TargetPos);
		const float DistDiff = DistToTarget - OptimalDist;

		if (CurrentTick - m_LastStrafeChangeTick > Server()->TickSpeed() * (1 + (rand() % 3)))
		{
			m_StrafeDirection = (rand() % 2) ? 1 : -1;
			m_LastStrafeChangeTick = CurrentTick;
		}

		const vec2 DirToTarget = normalize(TargetPos - m_Pos);
		if (m_Core.m_ActiveWeapon != WEAPON_HAMMER && DistToTarget < OptimalDist * 0.4f)
		{
			WantedDir = (DirToTarget.x > 0) ? -1 : 1;
		}
		else if (std::abs(DistDiff) < 80.0f)
		{
			WantedDir = m_StrafeDirection;
		}
		else
		{
			WantedDir = (DistDiff > 0) ? (DirToTarget.x > 0 ? 1 : -1) : (DirToTarget.x > 0 ? -1 : 1);
		}
		break;
	}

	case AIState::Retreat:
	{
		const vec2 DirToTarget = normalize(TargetPos - m_Pos);
		WantedDir = (DirToTarget.x > 0) ? -1 : 1;
		break;
	}

	case AIState::Chase:
		break;

	case AIState::Search:
	{
		// move towards the last known target position, but look around when close to it
		if (DistToWaypoint < 32.0f)
		{
			WantedDir = 0;
			m_LookAroundTimer++;

			// every half second, sharply change the view left/right
			if (m_LookAroundTimer % 25 == 0)
			{
				vec2 RandomLook = vec2((rand() % 2 == 0) ? 1.0f : -1.0f, (rand() % 100 - 50) / 100.0f);
				SetAim(RandomLook, true);
			}
		}
		else
		{
			if (CurrentTick - m_StateTimer > Server()->TickSpeed() * 2 && (CurrentTick % 30 == 0))
				WantedDir = -WantedDir;
		}
		break;
	}

	case AIState::Stuck:
	{
		WantedDir = -m_PrevDirection;
		if (WantedDir == 0)
			WantedDir = (rand() % 2) ? 1 : -1;
		m_Input.m_Jump = 1;
		break;
	}

	case AIState::Idle:
	default:
	{
		if (WantedDir == 0)
		{
			m_BoredomTimer++;
			if (m_BoredomTimer > Server()->TickSpeed() * 5)
			{
				if (rand() % 100 < 5)
				{
					int Action = rand() % 3;
					if (Action == 0) 
						m_Input.m_Jump = 1;
					else if (Action == 1) 
						SendBubbleEmoticon(EMOTICON_MUSIC);
					else if (Action == 2) 
						SetAim(vec2(0.f, -1.f), false);

					m_BoredomTimer = 0;
				}
			}
		}
		else
		{
			m_BoredomTimer = 0;
		}
		break;
	}
	}

	m_Input.m_Direction = WantedDir;

	if (IsCollisionFlag(CCollision::COLFLAG_DISALLOW_MOVE) && m_AIState == AIState::Combat)
	{
		auto* pTarget = m_pAI->GetTarget();
		if (pTarget) 
			pTarget->SetType(TargetType::Lost);
		m_Input.m_Direction = -m_Input.m_Direction;
	}
}

void CCharacterBotAI::UpdateJumping(const vec2& DirToWaypoint, int ActiveWayPoints)
{
	const bool IsOnGround = IsGrounded();
	m_Input.m_Jump = 0;

	if (DirToWaypoint.y < -0.5f && (IsOnGround || m_Core.m_Vel.y > 0))
		m_Input.m_Jump = 1;

	if (m_Input.m_Direction != 0)
	{
		const vec2 ForwardPos = m_Pos + vec2(m_Input.m_Direction * 48.0f, 0.0f);
		vec2 WallHit;

		if (GS()->Collision()->IntersectLineWithInvisible(m_Pos, ForwardPos, &WallHit, nullptr) ||
			GS()->Collision()->IntersectLineDoor(m_Pos, ForwardPos))
		{
			const float CheckHeight = IsOnGround ? -210.0f : -125.0f;
			if (GS()->Collision()->IntersectLine(WallHit, WallHit + vec2(0.f, CheckHeight), nullptr, nullptr))
				m_Input.m_Jump = 1;
		}

		if (IsOnGround && !m_Input.m_Jump && m_AIState != AIState::Retreat)
		{
			const vec2 GroundCheck = ForwardPos + vec2(0.f, 64.f);
			if (!GS()->Collision()->IntersectLine(ForwardPos, GroundCheck, nullptr, nullptr))
				m_Input.m_Jump = 1;
		}
	}

	if (m_Input.m_Jump && (DirToWaypoint.y >= 0 || ActiveWayPoints < 3) && m_AIState != AIState::Combat)
		m_Input.m_Jump = 0;

	vec2 IntersectPos;
	if (CCharacter* pChar = GameWorld()->IntersectCharacter(m_Pos, m_Pos + vec2(m_Input.m_Direction * 64.0f, 0.0f), 16.0f, IntersectPos, this))
	{
		if (!pChar->GetPlayer()->IsBot())
			m_Input.m_Jump = 1;
	}
}

void CCharacterBotAI::UpdateHooking(const vec2& DirToWaypoint, int ActiveWayPoints, bool HasTarget, const vec2& TargetPos)
{
	if (m_Input.m_Hook || m_Input.m_Jump)
		return;

	if (m_Core.m_HookState == HOOK_GRABBED && m_Core.m_HookedPlayer == -1)
	{
		vec2 HookVel = normalize(m_Core.m_HookPos - m_Pos) * GS()->Tuning()->m_HookDragAccel;
		HookVel.y = (HookVel.y > 0) ? HookVel.y * 0.3f : HookVel.y;
		HookVel.x *= ((HookVel.x < 0 && m_Input.m_Direction < 0) || (HookVel.x > 0 && m_Input.m_Direction > 0)) ? 0.95f : 0.75f;

		vec2 Target(m_Input.m_TargetX, m_Input.m_TargetY);
		if (dot(Target, HookVel) > 0 || (Target.y < 0 && m_Core.m_Vel.y > 0.f && m_Core.m_HookTick < Server()->TickSpeed() * 1.5f))
			m_Input.m_Hook = 1;

		if (m_Core.m_HookTick > 3 * Server()->TickSpeed() || distance(m_Core.m_HookPos, m_Pos) < 20.0f)
			m_Input.m_Hook = 0;
		return;
	}

	if (m_Core.m_HookState == HOOK_FLYING)
	{
		m_Input.m_Hook = 1;
		return;
	}

	if (ActiveWayPoints <= 2 || (DirToWaypoint.x == 0 && DirToWaypoint.y == 0))
		return;

	if (m_LatestInput.m_Hook != 0 || m_Core.m_HookState != HOOK_IDLE)
		return;

	if (m_AIState == AIState::Chase && HasTarget && distance(m_Pos, TargetPos) < GS()->Tuning()->m_HookLength * 0.8f)
	{
		const vec2 DirToTarget = normalize(TargetPos - m_Pos);
		const vec2 HookEnd = m_Pos + DirToTarget * GS()->Tuning()->m_HookLength;
		vec2 HitPos;
		int HitFlags = GS()->Collision()->IntersectLine(m_Pos, HookEnd, &HitPos, nullptr);
		if ((HitFlags & CCollision::COLFLAG_SOLID) && !(HitFlags & CCollision::COLFLAG_NOHOOK))
		{
			SetAim(HitPos - m_Pos, true);
			m_Input.m_Hook = 1;
			return;
		}
	}

	if (rand() % 4 != 0)
		return;

	vec2 BestHookDir(0.f, 0.f);
	float MaxForce = 0;
	constexpr int NumDirections = 16;

	for (int i = 0; i < NumDirections; i++)
	{
		vec2 Dir = direction(2.0f * i * pi / NumDirections);
		vec2 Pos = m_Pos + Dir * GS()->Tuning()->m_HookLength;

		if ((GS()->Collision()->IntersectLine(m_Pos, Pos, &Pos, nullptr) & (CCollision::COLFLAG_SOLID | CCollision::COLFLAG_NOHOOK)) == CCollision::COLFLAG_SOLID)
		{
			vec2 IntersectPos;
			if (GameWorld()->IntersectCharacter(m_Pos, Pos, 16.0f, IntersectPos, this))
				continue;

			vec2 HookVel = Dir * GS()->Tuning()->m_HookDragAccel;
			HookVel.y = (HookVel.y > 0) ? HookVel.y * 0.3f : HookVel.y;
			HookVel.x *= ((HookVel.x < 0 && m_Input.m_Direction < 0) || (HookVel.x > 0 && m_Input.m_Direction > 0)) ? 0.95f : 0.75f;
			HookVel += vec2(0, 1) * GS()->Tuning()->m_Gravity;

			float ps = dot(DirToWaypoint, HookVel);
			if (ps > MaxForce)
			{
				MaxForce = ps;
				BestHookDir = Pos - m_Pos;
			}
		}
	}

	if (length(BestHookDir) > 32.f)
	{
		SetAim(BestHookDir, true);
		m_Input.m_Hook = 1;
	}
}

void CCharacterBotAI::HandleAntiStuck()
{
	const float MoveDelta = distance(m_Pos, m_LastPos);
	if (MoveDelta > AITuning::STUCK_MIN_DISTANCE)
	{
		m_MoveTick = Server()->Tick();
		m_StuckCount = 0;
		m_LastPos = m_Pos;
	}
	else
	{
		m_StuckCount++;
		if (Server()->Tick() - m_MoveTick > Server()->TickSpeed() / 2)
		{
			m_Input.m_Direction = (m_Input.m_Direction != 0) ? -m_Input.m_Direction : (rand() % 2 ? 1 : -1);
			m_Input.m_Jump = 1;
			m_MoveTick = Server()->Tick();
			m_LastPos = m_Pos;
		}
	}
}

void CCharacterBotAI::SetAim(vec2 Dir, bool Instant)
{
	if (length(Dir) < 1.0f)
		Dir = vec2(0.f, 1.f);

	const vec2 NormalizedTarget = normalize(Dir);
	m_CurrentAimDir = Instant ? NormalizedTarget : normalize(mix(m_CurrentAimDir, NormalizedTarget, AITuning::AIM_SMOOTH_FACTOR));

	m_Input.m_TargetX = static_cast<int>(m_CurrentAimDir.x * 200.0f);
	m_Input.m_TargetY = static_cast<int>(m_CurrentAimDir.y * 200.0f);
	m_LatestInput.m_TargetX = m_Input.m_TargetX;
	m_LatestInput.m_TargetY = m_Input.m_TargetY;
}

void CCharacterBotAI::Fire()
{
	// don't fire if the bot is hesitating, has no target, or the target is invalid
	if (--m_HesitationTicks > 0)
		return;

	auto* pTarget = AI()->GetTarget();
	if (pTarget->IsEmpty() || pTarget->IsCollided())
		return;

	if (!GS()->GetPlayerChar(pTarget->GetCID()))
		return;

	if ((m_Input.m_Hook && m_Core.m_HookState == HOOK_IDLE) || m_ReloadTimer != 0)
		return;

	if (m_AIState == AIState::Retreat && m_Core.m_ActiveWeapon != WEAPON_HAMMER)
	{
		if (rand() % 3 != 0)
			return;
	}

	m_Input.m_Fire++;
	m_LatestInput.m_Fire++;
}

void CCharacterBotAI::UpdateTarget(float Radius) const
{
	auto* pTarget = m_pAI->GetTarget();
	if (!pTarget->IsEmpty())
	{
		const auto* pTargetChar = GS()->GetPlayerChar(pTarget->GetCID());
		if (!pTargetChar || distance(pTargetChar->GetPos(), m_Pos) > AITuning::MAX_SIGHT_DISTANCE)
		{
			m_pBotPlayer->m_TargetPos.reset();
			pTarget->Reset();
			return;
		}

		const bool Blocked = GS()->Collision()->IntersectLineWithInvisible(m_Core.m_Pos, pTargetChar->m_Core.m_Pos, nullptr, nullptr) ||
			GS()->Collision()->IntersectLineDoor(m_Core.m_Pos, pTargetChar->m_Core.m_Pos);

		pTarget->UpdateCollided(Blocked);

		if (pTargetChar->m_Core.m_DamageDisabled || Blocked)
		{
			if (pTarget->SetType(TargetType::Lost))
				GS()->SendEmoticon(m_pBotPlayer->GetCID(), EMOTICON_QUESTION);
		}
	}

	m_pAI->OnTargetRules(Radius);
}

void CCharacterBotAI::SendBubbleEmoticon(int EmoticonID)
{
	GS()->SendEmoticon(m_pBotPlayer->GetCID(), EmoticonID);
}