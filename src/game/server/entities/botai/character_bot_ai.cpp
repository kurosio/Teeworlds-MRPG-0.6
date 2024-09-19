/* (c) Alexandre Díaz. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/server/gamecontext.h>
#include <game/server/playerbot.h>
#include "character_bot_ai.h"

#include <game/collision.h>

#include <game/server/core/components/skills/entities/heart_healer.h> // for nurse
#include <game/server/core/components/Bots/BotData.h>

#include <game/server/core/tools/path_finder.h>

#include "ai/npc_ai.h"
#include "ai/eidolon_ai.h"
#include "ai/mob_ai.h"
#include "ai/quest_mob.h"
#include "ai/quest_npc.h"

MACRO_ALLOC_POOL_ID_IMPL(CCharacterBotAI, MAX_CLIENTS* ENGINE_MAX_WORLDS + MAX_CLIENTS)

CCharacterBotAI::CCharacterBotAI(CGameWorld* pWorld) : CCharacter(pWorld)
{
	m_aListDmgPlayers.reserve(MAX_CLIENTS);
}
CCharacterBotAI::~CCharacterBotAI()
{
	delete m_pAI;
	m_pAI = nullptr;
}

bool CCharacterBotAI::Spawn(CPlayer* pPlayer, vec2 Pos)
{
	m_pBotPlayer = static_cast<CPlayerBot*>(pPlayer);

	const int Bottype = m_pBotPlayer->GetBotType();
	if(Bottype == TYPE_BOT_NPC)
	{
		const int MobID = m_pBotPlayer->GetBotMobID();
		m_pAI = new CNpcAI(&NpcBotInfo::ms_aNpcBot[MobID], m_pBotPlayer, this);
	}
	else if(Bottype == TYPE_BOT_QUEST)
	{
		const int MobID = m_pBotPlayer->GetBotMobID();
		m_pAI = new CQuestNpcAI(&QuestBotInfo::ms_aQuestBot[MobID], m_pBotPlayer, this);
	}
	else if(Bottype == TYPE_BOT_QUEST_MOB)
	{
		m_pAI = new CQuestMobAI(&m_pBotPlayer->GetQuestBotMobInfo(), m_pBotPlayer, this);
	}
	else if(Bottype == TYPE_BOT_MOB)
	{
		const int MobID = m_pBotPlayer->GetBotMobID();
		m_pAI = new CMobAI(&MobBotInfo::ms_aMobBot[MobID], m_pBotPlayer, this);
	}
	else if(Bottype == TYPE_BOT_EIDOLON)
	{
		m_pAI = new CEidolonAI(m_pBotPlayer, this);
	}

	if(CCharacter::Spawn(m_pBotPlayer, Pos))
	{
		m_pAI->OnSpawn();
		return true;
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

bool CCharacterBotAI::TakeDamage(vec2 Force, int Dmg, int From, int Weapon)
{
	if(!m_pBotPlayer->IsActive() || m_pBotPlayer->IsDisabledBotDamage())
		return false;

	CPlayer* pFrom = GS()->GetPlayer(From, false, true);
	if(!pFrom)
		return false;

	// Between player damage disabled
	if(GS()->Collision()->IntersectLineColFlag(m_Core.m_Pos, pFrom->GetCharacter()->m_Core.m_Pos, nullptr, nullptr, CCollision::COLFLAG_DISALLOW_MOVE))
		return false;

	CCharacter::TakeDamage(Force, Dmg, From, Weapon);

	// Update for from eidolon damage
	if(pFrom->GetBotType() == TYPE_BOT_EIDOLON)
	{
		if(const auto* pFromBot = dynamic_cast<CPlayerBot*>(pFrom))
		{
			From = pFromBot->GetEidolonOwner()->GetCID();
		}
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

			if(!GS()->IsPlayerInWorld(ClientID, m_pBotPlayer->GetPlayerWorldID()))
				continue;

			if(distance(pPlayer->m_ViewPos, m_Core.m_Pos) > 1000.0f)
				continue;

			RewardPlayer(pPlayer);
		}
	}

	// Die
	m_aListDmgPlayers.clear();
	AI()->GetTarget()->Reset();
	CCharacter::Die(Killer, Weapon);
}

void CCharacterBotAI::RewardPlayer(CPlayer* pPlayer) const
{
	m_pAI->OnRewardPlayer(pPlayer, m_DieForce);
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

	// If the interval has succesful, change the weapon
	const int changeInterval = Server()->TickSpeed() * (1 + rand() % 3);
	if(Server()->Tick() % changeInterval == 0)
	{
		const int RandomWeapon = rand() % (WEAPON_LASER + 1);

		if(RandomWeapon == WEAPON_HAMMER || m_pBotPlayer->GetEquippedItemID((ItemFunctional)RandomWeapon).has_value())
		{
			m_Core.m_ActiveWeapon = clamp(RandomWeapon, (int)WEAPON_HAMMER, (int)WEAPON_LASER);
		}
	}
}

bool CCharacterBotAI::GiveWeapon(int Weapon, int GiveAmmo)
{
	if(Weapon < WEAPON_HAMMER || Weapon > WEAPON_NINJA)
		return false;

	m_Core.m_aWeapons[Weapon].m_Got = true;
	m_Core.m_aWeapons[Weapon].m_Ammo = GiveAmmo;
	return true;
}

void CCharacterBotAI::Tick()
{
	// Check if this bot is alive
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

	// handle safe flags
	HandleSafeFlags();

	// engine bots
	ProcessBot();
	HandleTiles();
	HandleTuning();

	// core
	m_Core.m_Input = m_Input;
	m_Core.Tick(true, &m_pBotPlayer->m_NextTuningParams);
	m_pBotPlayer->UpdateTempData(m_Health, m_Mana);

	// game clipped
	if(GameLayerClipped(m_Pos))
	{
		Die(m_pBotPlayer->GetCID(), WEAPON_SELF);
	}

	// door
	if(length(m_NormalDoorHit) < 0.1f)
	{
		m_OlderPos = m_OldPos;
		m_OldPos = m_Core.m_Pos;
	}
}

void CCharacterBotAI::TickDeferred()
{
	// check active this bot
	if(!m_pBotPlayer->IsActive() || !IsAlive())
		return;

	// door reset
	if(length(m_NormalDoorHit) >= 0.1f)
	{
		HandleDoorHit();
		m_NormalDoorHit = vec2(0, 0);
	}

	CCharacterCore::CParams PlayerTune(&m_pBotPlayer->m_NextTuningParams);
	m_Core.Move(&PlayerTune);
	m_Core.Quantize();
	m_Pos = m_Core.m_Pos;
}

void CCharacterBotAI::Snap(int SnappingClient)
{
	int ID = m_pBotPlayer->GetCID();

	// check active this bot
	if(!m_pBotPlayer->IsActive() || !m_pBotPlayer->IsActiveForClient(SnappingClient))
		return;

	// check network clipped and translate state
	if(NetworkClipped(SnappingClient) || !Server()->Translate(ID, SnappingClient))
		return;

	CNetObj_Character* pCharacter = static_cast<CNetObj_Character*>(Server()->SnapNewItem(NETOBJTYPE_CHARACTER, ID, sizeof(CNetObj_Character)));
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
	m_pAI->OnSnapDDNetCharacter(SnappingClient, pDDNetCharacter);
}

void CCharacterBotAI::ProcessBot()
{
	const int Bottype = m_pBotPlayer->GetBotType();

	AI()->GetTarget()->Tick();
	m_pAI->Process();

	m_PrevPos = m_Pos;
	if(m_Input.m_Direction)
	{
		m_PrevDirection = m_Input.m_Direction;
	}
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
	m_pBotPlayer->m_PathHandle.TryGetPath();
	if(m_pBotPlayer->m_PathHandle.vPath.empty())
		return;

	// Update the aim of the bot player by calculating the direction vector from the current position to the target position
	SetAim(m_pBotPlayer->m_TargetPos - m_Pos);

	// Initialize variables
	int Index = -1;
	int ActiveWayPoints = 4;
	vec2 WayDir = m_pBotPlayer->m_TargetPos;
	for(int i = 0; i < (int)m_pBotPlayer->m_PathHandle.vPath.size() && i < 30 && !GS()->Collision()->IntersectLineWithInvisible(m_pBotPlayer->m_PathHandle.vPath[i], m_Pos, nullptr, nullptr); i++)
	{
		Index = i;
		ActiveWayPoints = i;
	}

	// If the given index is valid
	if(Index > -1)
		WayDir = m_pBotPlayer->m_PathHandle.vPath[Index];

	// Accuracy
	WayDir = normalize(WayDir - m_Core.m_Pos);

	// If the x component of WayDir is negative and there are more than 3 active waypoints
	if(WayDir.x < 0 && ActiveWayPoints > 3)
	{
		// Set the direction input to -1 (left)
		m_Input.m_Direction = -1;
	}
	// If the x component of WayDir is positive and there are more than 3 active waypoints
	else if(WayDir.x > 0 && ActiveWayPoints > 3)
	{
		// Set the direction input to 1 (right)
		m_Input.m_Direction = 1;
	}
	// Otherwise
	else
	{
		// Set the direction input to the previous direction
		m_Input.m_Direction = m_PrevDirection;
	}

	// check dissalow move
	int BotType = m_pBotPlayer->GetBotType();
	if(BotType != TYPE_BOT_EIDOLON && BotType != TYPE_BOT_QUEST_MOB && BotType != TYPE_BOT_NPC)
	{
		if(IsCollisionFlag(CCollision::COLFLAG_DISALLOW_MOVE))
		{
			AI()->GetTarget()->SetType(TargetType::LOST);
			m_Input.m_Direction = -m_Input.m_Direction;
			m_NormalDoorHit = vec2(1.f, 1.f);
		}
	}

	// jumping
	const bool IsGround = IsGrounded();
	if((IsGround && WayDir.y < -0.5) || (!IsGround && WayDir.y < -0.5 && m_Core.m_Vel.y > 0))
		m_Input.m_Jump = 1;

	if(GS()->Collision()->IntersectLineWithInvisible(m_Pos, m_Pos + vec2(m_Input.m_Direction, 0) * 150, &m_WallPos, nullptr))
	{
		if(IsGround && GS()->Collision()->IntersectLine(m_WallPos, m_WallPos + vec2(0, -1) * 210, nullptr, nullptr))
			m_Input.m_Jump = 1;

		if(!IsGround && GS()->Collision()->IntersectLine(m_WallPos, m_WallPos + vec2(0, -1) * 125, nullptr, nullptr))
			m_Input.m_Jump = 1;
	}

	// if way points down dont jump
	if(m_Input.m_Jump == 1 && (WayDir.y >= 0 || ActiveWayPoints < 3))
		m_Input.m_Jump = 0;

	// jump over character
	vec2 IntersectPos;
	CCharacter* pChar = GameWorld()->IntersectCharacter(m_Core.m_Pos, m_Core.m_Pos + vec2(m_Input.m_Direction, 0) * 128, 16.0f, IntersectPos, (CCharacter*)this);
	if(pChar && (IntersectPos.x < m_Core.m_Pos.x || !pChar->GetPlayer()->IsBot()))
		m_Input.m_Jump = 1;

	if(ActiveWayPoints > 2 && !m_Input.m_Hook && (WayDir.x != 0 || WayDir.y != 0))
	{
		if(m_Core.m_HookState == HOOK_GRABBED && m_Core.m_HookedPlayer == -1)
		{
			vec2 HookVel = normalize(m_Core.m_HookPos - m_Core.m_Pos) * GS()->Tuning()->m_HookDragAccel;
			HookVel.y *= 0.3f;
			HookVel.x *= (m_Input.m_Direction == 0 || (m_Input.m_Direction < 0 && HookVel.x > 0) || (m_Input.m_Direction > 0 && HookVel.x < 0)) ? 0.95f : 0.75f;
			HookVel += vec2(0, 1) * GS()->Tuning()->m_Gravity;

			float ps = dot(WayDir, HookVel);
			if(ps > 0 || (WayDir.y < 0 && m_Core.m_Vel.y > 0.f && m_Core.m_HookTick < 3 * SERVER_TICK_SPEED))
				m_Input.m_Hook = 1;
			if(m_Core.m_HookTick > 4 * SERVER_TICK_SPEED || length(m_Core.m_HookPos - GetPos()) < 20.0f)
				m_Input.m_Hook = 0;
		}
		else if(m_Core.m_HookState == HOOK_FLYING)
			m_Input.m_Hook = 1;
		else if(m_LatestInput.m_Hook == 0 && m_Core.m_HookState == HOOK_IDLE && rand() % 3 == 0)
		{
			int NumDir = 45;
			vec2 HookDir(0.0f, 0.0f);
			float MaxForce = 0;

			for(int i = 0; i < NumDir; i++)
			{
				float a = 2 * i * pi / NumDir;
				vec2 dir = direction(a);
				vec2 Pos = GetPos() + dir * GS()->Tuning()->m_HookLength;

				if((GS()->Collision()->IntersectLine(GetPos(), Pos, &Pos, nullptr) & (CCollision::COLFLAG_SOLID | CCollision::COLFLAG_NOHOOK)) == CCollision::COLFLAG_SOLID)
				{
					vec2 HookVel = dir * GS()->Tuning()->m_HookDragAccel;
					HookVel.y *= 0.3f;
					HookVel.x *= (HookVel.x < 0 && m_Input.m_Direction < 0) || (HookVel.x > 0 && m_Input.m_Direction > 0) ? 0.95f : 0.75f;
					HookVel += vec2(0, 1) * GS()->Tuning()->m_Gravity;

					float ps = dot(WayDir, HookVel);
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

	// in case the bot stucks
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
	CPlayer* pPlayer = GS()->GetPlayer(AI()->GetTarget()->GetCID(), false, true);
	if(AI()->GetTarget()->IsEmpty() || AI()->GetTarget()->IsCollised() || !pPlayer)
		return;

	if((m_Input.m_Hook && m_Core.m_HookState == HOOK_IDLE) || m_ReloadTimer != 0)
		return;

	if(m_Core.m_ActiveWeapon == WEAPON_HAMMER && distance(pPlayer->GetCharacter()->GetPos(), GetPos()) > 128.0f)
		return;

	// fire
	if((m_Input.m_Fire & 1) != 0)
	{
		m_Input.m_Fire++;
		m_LatestInput.m_Fire++;
		return;
	}

	if(!(m_Input.m_Fire & 1))
	{
		m_LatestInput.m_Fire++;
		m_Input.m_Fire++;
	}
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
	if(!AI()->GetTarget()->IsEmpty())
	{
		const auto* pTargetChar = GS()->GetPlayerChar(AI()->GetTarget()->GetCID());

		if(!pTargetChar || !GS()->IsPlayerInWorld(AI()->GetTarget()->GetCID()) ||
			distance(pTargetChar->GetPos(), m_Pos) > 800.0f)
		{
			AI()->GetTarget()->Reset();
		}

		if(pTargetChar && pTargetChar->m_Core.m_DamageDisabled)
		{
			if(AI()->GetTarget()->SetType(TargetType::LOST))
			{
				GS()->SendEmoticon(m_pBotPlayer->GetCID(), EMOTICON_QUESTION);
			}
		}
	}

	m_pAI->OnTargetRules(Radius);
}