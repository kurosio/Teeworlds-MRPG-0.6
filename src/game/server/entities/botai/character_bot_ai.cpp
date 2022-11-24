/* (c) Alexandre Díaz. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>
#include <game/server/playerbot.h>
#include "character_bot_ai.h"

#include <game/collision.h>

#include <game/server/mmocore/GameEntities/Eidolons/base.h>
#include <game/server/mmocore/Components/Skills/Entities/HealthTurret/hearth.h> // for nurse

#include <game/server/mmocore/Components/Bots/BotData.h>
#include <game/server/mmocore/Components/Quests/QuestCore.h>

MACRO_ALLOC_POOL_ID_IMPL(CCharacterBotAI, MAX_CLIENTS * ENGINE_MAX_WORLDS + MAX_CLIENTS)

CCharacterBotAI::CCharacterBotAI(CGameWorld *pWorld) : CCharacter(pWorld) {}
CCharacterBotAI::~CCharacterBotAI() = default;

int CCharacterBotAI::GetSnapFullID() const { return m_pBotPlayer->GetCID() * SNAPBOTS; }

bool CCharacterBotAI::Spawn(class CPlayer *pPlayer, vec2 Pos)
{
	m_pBotPlayer = static_cast<CPlayerBot*>(pPlayer);
	if(!CCharacter::Spawn(m_pBotPlayer, Pos))
		return false;

	ClearTarget();

	// mob information
	const int MobID = m_pBotPlayer->GetBotMobID();
	if(m_pBotPlayer->GetBotType() == TYPE_BOT_MOB && MobBotInfo::ms_aMobBot[MobID].m_Boss)
	{
		for(int i = 0; i < 3; i++)
		{
			CreateSnapProj(GetSnapFullID(), 1, POWERUP_HEALTH, true, false);
			CreateSnapProj(GetSnapFullID(), 1, WEAPON_HAMMER, false, true);
		}

		if(!GS()->IsDungeon())
			GS()->ChatWorldID(MobBotInfo::ms_aMobBot[MobID].m_WorldID, "", "In your zone emerging {STR}!", MobBotInfo::ms_aMobBot[MobID].GetName());
	}
	else if(m_pBotPlayer->GetBotType() == TYPE_BOT_QUEST && QuestBotInfo::ms_aQuestBot[MobID].m_HasAction)
	{
		CreateSnapProj(GetSnapFullID(), 2, POWERUP_HEALTH, true, false);
		CreateSnapProj(GetSnapFullID(), 2, POWERUP_ARMOR, true, false);
	}
	else if(m_pBotPlayer->GetBotType() == TYPE_BOT_NPC)
	{
		const int Function = NpcBotInfo::ms_aNpcBot[MobID].m_Function;
		if(Function == FUNCTION_NPC_GIVE_QUEST)
			CreateSnapProj(GetSnapFullID(), 3, POWERUP_ARMOR, false, false);
	}
	else if(m_pBotPlayer->GetBotType() == TYPE_BOT_EIDOLON)
	{
		m_Core.m_Solo = true;
		int ClientID = m_pBotPlayer->GetCID();
		int OwnerCID = m_pBotPlayer->GetEidolonOwner()->GetCID();
		new CEidolon(&GS()->m_World, Pos, 0, ClientID, OwnerCID);
	}
	return true;
}

void CCharacterBotAI::GiveRandomEffects(int To)
{
	if(m_pBotPlayer->GetBotType() == TYPE_BOT_MOB)
	{
		CPlayer* pPlayerTo = GS()->GetPlayer(To);
		if(pPlayerTo && To != m_pBotPlayer->GetCID())
		{
			int MobID = m_pBotPlayer->GetBotMobID();
			if(const CMobBuffDebuff* pBuff = MobBotInfo::ms_aMobBot[MobID].GetRandomEffect())
				pPlayerTo->GiveEffect(pBuff->getEffect(), pBuff->getTime(), pBuff->getChance());
		}
	}
}

bool CCharacterBotAI::TakeDamage(vec2 Force, int Dmg, int From, int Weapon)
{
	CPlayer* pFrom = GS()->GetPlayer(From, false);
	if (!pFrom || !m_pBotPlayer->IsActive())
		return false;

	// dissalow for self damage except type mobs
	if(m_pBotPlayer->GetBotType() != TYPE_BOT_MOB)
		return false;

	// dissalow entered line damage
	if(GS()->Collision()->IntersectLineColFlag(m_Core.m_Pos, pFrom->GetCharacter()->m_Core.m_Pos, nullptr, nullptr, CCollision::COLFLAG_DISALLOW_MOVE))
		return false;

	// dissalow damage from bot to bot except type eidolon
	if(pFrom->IsBot() && pFrom->GetBotType() != TYPE_BOT_EIDOLON)
		return false;

	// damage receive
	CCharacter::TakeDamage(Force, Dmg, From, Weapon);

	// for eidolon change from owner to owner eidolon
	if(pFrom->IsBot() && pFrom->GetBotType() == TYPE_BOT_EIDOLON)
		From = dynamic_cast<CPlayerBot*>(pFrom)->GetEidolonOwner()->GetCID();

	// if the bot doesn't have target player, set to from
	if(IsBotTargetEmpty())
		SetTarget(From);

	// add (from player) to the list of those who caused damage
	if(std::find_if(m_aListDmgPlayers.begin(), m_aListDmgPlayers.end(), [From](int ClientID){ return ClientID == From; }) == m_aListDmgPlayers.end())
		m_aListDmgPlayers.push_back(From);

	// verify death
	if(m_Health <= 0)
	{
		if(Weapon != WEAPON_SELF && Weapon != WEAPON_WORLD)
		{
			for(const auto& ClientID : m_aListDmgPlayers)
			{
				CPlayer* pPlayer = GS()->GetPlayer(ClientID, true, true);
				if(!pPlayer || !GS()->IsPlayerEqualWorld(ClientID, m_pBotPlayer->GetPlayerWorldID()) || distance(pPlayer->m_ViewPos, m_Core.m_Pos) > 1000.0f)
					continue;

				RewardPlayer(pPlayer, Force);
			}
		}
		m_aListDmgPlayers.clear();
		ClearTarget();

		Die(From, Weapon);
		return false;
	}

	return true;
}

void CCharacterBotAI::Die(int Killer, int Weapon)
{
	if(m_pBotPlayer->GetBotType() != TYPE_BOT_MOB)
		return;

	CCharacter::Die(Killer, Weapon);
}

void CCharacterBotAI::RewardPlayer(CPlayer* pPlayer, vec2 Force) const
{
	const int ClientID = pPlayer->GetCID();
	const int BotID = m_pBotPlayer->GetBotID();
	const int SubID = m_pBotPlayer->GetBotMobID();

	// quest mob progress
	if(m_pBotPlayer->GetBotType() == TYPE_BOT_MOB)
		GS()->Mmo()->Quest()->AddMobProgressQuests(pPlayer, BotID);

	// golds
	const int Golds = max(MobBotInfo::ms_aMobBot[SubID].m_Power / g_Config.m_SvStrongGold, 1);
	pPlayer->AddMoney(Golds);

	// experience
	const int ExperienceMob = max(1, (int)computeExperience(MobBotInfo::ms_aMobBot[SubID].m_Level) / g_Config.m_SvKillmobsIncreaseLevel);
	const int ExperienceWithMultiplier = GS()->GetExperienceMultiplier(ExperienceMob);
	GS()->CreateParticleExperience(m_Core.m_Pos, ClientID, ExperienceWithMultiplier, Force);

	// drop experience
	const int ExperienceDrop = max(ExperienceWithMultiplier / 2, 1);
	GS()->CreateDropBonuses(m_Core.m_Pos, 1, ExperienceDrop, (1 + random_int() % 2), Force);

	// drop item's
	const float ActiveLuckyDrop = clamp((float)pPlayer->GetAttributeSize(AttributeIdentifier::LuckyDropItem, true) / 100.0f, 0.01f, 10.0f);
	for(int i = 0; i < 5; i++)
	{
		CItem DropItem;
		DropItem.SetID(MobBotInfo::ms_aMobBot[SubID].m_aDropItem[i]);
		DropItem.SetValue(MobBotInfo::ms_aMobBot[SubID].m_aValueItem[i]);
		if(DropItem.GetID() <= 0 || DropItem.GetValue() <= 0)
			continue;

		const float RandomDrop = clamp(MobBotInfo::ms_aMobBot[SubID].m_aRandomItem[i] + ActiveLuckyDrop, 0.0f, 100.0f);
		const vec2 ForceRandom(centrelized_frandom(Force.x, Force.x / 4.0f), centrelized_frandom(Force.y, Force.y / 8.0f));
		GS()->CreateRandomDropItem(m_Core.m_Pos, ClientID, RandomDrop, DropItem, ForceRandom);
	}

	// skill point
	// TODO: balance depending on the difficulty, not just the level
	const int CalculateSP = (pPlayer->Acc().m_Level > MobBotInfo::ms_aMobBot[SubID].m_Level ? 40 + min(40, (pPlayer->Acc().m_Level - MobBotInfo::ms_aMobBot[SubID].m_Level) * 2) : 40);
	if(random_int() % CalculateSP == 0)
	{
		CPlayerItem* pPlayerItem = pPlayer->GetItem(itSkillPoint);
		pPlayerItem->Add(1);
		GS()->Chat(ClientID, "Skill points increased. Now ({INT}SP)", pPlayerItem->GetValue());
	}
}

void CCharacterBotAI::ChangeWeapons()
{
	const int RandomSec = 1 + random_int() % 3;
	if(Server()->Tick() % (Server()->TickSpeed() * RandomSec) == 0)
	{
		const int RandomWeapon = clamp(random_int()%4, (int)WEAPON_HAMMER, (int)WEAPON_LASER);
		if(RandomWeapon == WEAPON_HAMMER || m_pBotPlayer->GetEquippedItemID((ItemFunctional)RandomWeapon))
			m_Core.m_ActiveWeapon = RandomWeapon;
	}
}

bool CCharacterBotAI::GiveWeapon(int Weapon, int GiveAmmo)
{
	const int WeaponID = clamp(Weapon, (int)WEAPON_HAMMER, (int)WEAPON_NINJA);
	m_Core.m_aWeapons[WeaponID].m_Got = true;
	m_Core.m_aWeapons[WeaponID].m_Ammo = GiveAmmo;
	return true;
}

void CCharacterBotAI::Tick()
{
	if(!m_pBotPlayer->IsActive() || !IsAlive())
		return;

	// check safe area
	ResetSafe();
	if(GS()->Collision()->CheckPoint(m_Core.m_Pos, CCollision::COLFLAG_SAFE_AREA) || m_pBotPlayer->GetBotType() == TYPE_BOT_EIDOLON)
		SetSafe();

	// engine bots
	HandleBot();
	HandleTilesets();
	HandleTuning();

	// core
	m_Core.m_Input = m_Input;
	m_Core.Tick(true, &m_pBotPlayer->m_NextTuningParams);
	m_pBotPlayer->UpdateTempData(m_Health, m_Mana);

	// game clipped
	if(GameLayerClipped(m_Pos))
		Die(m_pBotPlayer->GetCID(), WEAPON_SELF);

	// door
	if (!m_DoorHit)
	{
		m_OlderPos = m_OldPos;
		m_OldPos = m_Core.m_Pos;
	}
}

void CCharacterBotAI::TickDeferred()
{
	if(!m_pBotPlayer->IsActive() || !IsAlive())
		return;
	
	if(m_DoorHit)
	{
		ResetDoorPos();
		m_DoorHit = false;
	}

	CCharacterCore::CParams PlayerTune(&m_pBotPlayer->m_NextTuningParams);
	m_Core.Move(&PlayerTune);
	m_Core.Quantize();
	m_Pos = m_Core.m_Pos;
}

void CCharacterBotAI::Snap(int SnappingClient)
{
	int ID = m_pBotPlayer->GetCID();
	if(NetworkClipped(SnappingClient) || !Server()->Translate(ID, SnappingClient) || !m_pBotPlayer->IsVisibleForClient(SnappingClient))
		return;

	// Character
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
	if(250 - ((Server()->Tick() - m_LastAction) % (250)) < 5)
		pCharacter->m_Emote = EMOTE_BLINK;

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
#define DDNetFlag(flag, check) if(check) { pDDNetCharacter->m_Flags |= (flag); }
	if(m_pBotPlayer->GetBotType() == TYPE_BOT_EIDOLON)
	{
		DDNetFlag(CHARACTERFLAG_SOLO, false)
		DDNetFlag(CHARACTERFLAG_COLLISION_DISABLED, true)
	}
#undef DDNetFlag

	pDDNetCharacter->m_Jumps = m_Core.m_Jumps;
	pDDNetCharacter->m_TeleCheckpoint = 0;
	pDDNetCharacter->m_StrongWeakID = 0; // ???

	// Display Informations
	pDDNetCharacter->m_TargetX = m_Core.m_Input.m_TargetX;
	pDDNetCharacter->m_TargetY = m_Core.m_Input.m_TargetY;
}

// interactive bots
void CCharacterBotAI::HandleBot()
{
	if(m_pBotPlayer->GetBotType() == TYPE_BOT_MOB)
	{
		EngineMobs();
	}
	else if(m_pBotPlayer->GetBotType() == TYPE_BOT_QUEST)
	{
		SetSafe();
		EngineQuestMob();
	}
	else if(m_pBotPlayer->GetBotType() == TYPE_BOT_NPC)
	{
		const int tx = m_Pos.x + m_Input.m_Direction * 45.0f;
		if(tx < 0)
			m_Input.m_Direction = 1;
		else if(tx >= GS()->Collision()->GetWidth() * 32.0f)
			m_Input.m_Direction = -1;

		m_LatestPrevInput = m_LatestInput;
		m_LatestInput = m_Input;

		SetSafe();
		EngineNPC();
	}

	else if(m_pBotPlayer->GetBotType() == TYPE_BOT_EIDOLON)
		EngineEidolons();

	HandleEvent();
}

// interactive of NPC
void CCharacterBotAI::EngineNPC()
{
	const int MobID = m_pBotPlayer->GetBotMobID();
	const int EmoteBot = NpcBotInfo::ms_aNpcBot[MobID].m_Emote;
	EmotesAction(EmoteBot);

	// direction eyes
	if(Server()->Tick() % Server()->TickSpeed() == 0)
		m_Input.m_TargetY = random_int()%4- random_int()%8;
	m_Input.m_TargetX = (m_Input.m_Direction*10+1);

	bool PlayerFinding;
	if(NpcBotInfo::ms_aNpcBot[MobID].m_Function == FUNCTION_NPC_NURSE)
		PlayerFinding = FunctionNurseNPC();
	else
		PlayerFinding = BaseFunctionNPC();

	// walking for npc
	if(!PlayerFinding && !NpcBotInfo::ms_aNpcBot[MobID].m_Static && random_int() % 50 == 0)
	{
		const int RandomDirection = random_int() % 6;
		if(RandomDirection == 0 || RandomDirection == 2)
			m_Input.m_Direction = -1 + RandomDirection;
		else 
			m_Input.m_Direction = 0;
	}
}

// interactive of Quest bots
void CCharacterBotAI::EngineQuestMob()
{
	if(Server()->Tick() % Server()->TickSpeed() == 0)
		m_Input.m_TargetY = random_int()%4- random_int()%8;
	m_Input.m_TargetX = (m_Input.m_Direction*10+1);
	EmotesAction(EMOTE_BLINK);
	SearchTalkedPlayer();
}

void CCharacterBotAI::HandleTuning()
{
	CTuningParams* pTuningParams = &m_pBotPlayer->m_NextTuningParams;

	// behavior mobs
	if(m_pBotPlayer->GetBotType() == TYPE_BOT_NPC || m_pBotPlayer->GetBotType() == TYPE_BOT_QUEST)
	{
		// walk effect
		pTuningParams->m_GroundControlSpeed = 5.0f;
		pTuningParams->m_GroundControlAccel = 1.0f;
	}
	else if(m_pBotPlayer->GetBotType() == TYPE_BOT_MOB)
	{
		// effect slower
		const int MobID = m_pBotPlayer->GetBotMobID();
		if(MobBotInfo::ms_aMobBot[MobID].IsEnabledBehavior("Slower"))
		{
			pTuningParams->m_Gravity = 0.25f;
			pTuningParams->m_GroundJumpImpulse = 8.0f;

			pTuningParams->m_AirFriction = 0.75f;
			pTuningParams->m_AirControlAccel = 1.0f;
			pTuningParams->m_AirControlSpeed = 3.75f;
			pTuningParams->m_AirJumpImpulse = 8.0f;

			pTuningParams->m_HookDragAccel = 1.5f;
			pTuningParams->m_HookDragSpeed = 8.0f;
		}

	}

	// eidolon boost
	if(m_pBotPlayer->GetBotType() == TYPE_BOT_EIDOLON)
	{
		pTuningParams->m_HookFireSpeed = 320.0f;
	}

	HandleIndependentTuning();
}

void CCharacterBotAI::BehaviorTick()
{
	const int MobID = m_pBotPlayer->GetBotMobID();

	// sleepy behavior
	if(IsBotTargetEmpty() && MobBotInfo::ms_aMobBot[MobID].IsEnabledBehavior("Sleepy"))
	{
		if(Server()->Tick() % (Server()->TickSpeed() / 2) == 0)
		{
			GS()->SendEmoticon(m_pBotPlayer->GetCID(), EMOTICON_ZZZ);
			SetEmote(EMOTE_BLINK, 1);
		}
	}
}

// interactive of Mobs
void CCharacterBotAI::EngineMobs()
{
	ResetInput();
	CPlayer* pPlayer = SearchTenacityPlayer(1000.0f);
	if(pPlayer && pPlayer->GetCharacter())
	{
		m_pBotPlayer->m_TargetPos = pPlayer->GetCharacter()->GetPos();
		Fire();
	}
	else if(Server()->Tick() > m_pBotPlayer->m_LastPosTick)
	{
		m_pBotPlayer->m_TargetPos = vec2(0, 0);
	}

	// behavior
	BehaviorTick();

	// show health for boss mobs
	const int MobID = m_pBotPlayer->GetBotMobID();
	if(MobBotInfo::ms_aMobBot[MobID].m_Boss)
	{
		for(const auto & ClientID : m_aListDmgPlayers)
		{
			if(GS()->GetPlayer(ClientID, true))
			{
				const int BotID = m_pBotPlayer->GetBotID();
				const int Health = m_pBotPlayer->GetHealth();
				const int StartHealth = m_pBotPlayer->GetStartHealth();
				const float Percent = (Health * 100.0) / StartHealth;
				std::unique_ptr<char[]> Progress = std::move(GS()->LevelString(100, Percent, 10, ':', ' '));
				GS()->Broadcast(ClientID, BroadcastPriority::GAME_PRIORITY, 100, "{STR} {STR}({INT}/{INT})",
					DataBotInfo::ms_aDataBot[BotID].m_aNameBot, Progress.get(), Health, StartHealth);
			}
		}
	}

	// bot with weapons since it has spread.
	if(MobBotInfo::ms_aMobBot[MobID].m_Spread >= 1)
		ChangeWeapons();

	Move();

	m_PrevPos = m_Pos;
	if(m_Input.m_Direction)
		m_PrevDirection = m_Input.m_Direction;

	EmotesAction(m_EmotionsStyle);
	HandleWeapons();
}

// interactive of Eidolons
void CCharacterBotAI::EngineEidolons()
{
	bool MobMove = true;
	if(const CPlayer* pOwner = m_pBotPlayer->GetEidolonOwner(); pOwner && pOwner->GetCharacter())
	{
		float Distance = distance(pOwner->m_ViewPos, m_Pos);
		if(Distance > 400.0f)
		{
			ClearTarget();
			m_pBotPlayer->m_TargetPos = pOwner->GetCharacter()->GetPos();
		}
		else if(const CPlayerBot* pEmenyPlayer = SearchMob(400.0f); pEmenyPlayer && pEmenyPlayer->GetCharacter())
		{
			m_pBotPlayer->m_TargetPos = pEmenyPlayer->GetCharacter()->GetPos();
			SetTarget(pEmenyPlayer->GetCID());
		}
		else if(Distance < 128.0f && IsBotTargetEmpty())
		{
			if(pOwner->GetCharacter()->m_Core.m_HookState != HOOK_GRABBED)
				ResetHook();

			if(Server()->Tick() % Server()->TickSpeed() == 0)
				m_Input.m_TargetY = random_int() % 4 - random_int() % 8;

			m_pBotPlayer->m_TargetPos = vec2(0, 0);
			m_Input.m_TargetX = (m_Input.m_Direction * 10 + 1);
			m_Input.m_Direction = 0;
			MobMove = false;
		}
		else
		{
			ClearTarget();
			m_pBotPlayer->m_TargetPos = pOwner->GetCharacter()->GetPos();
		}
	}

	// bot with weapons since it has spread.
	if(MobMove)
	{
		ResetInput();
		Fire();
		ChangeWeapons();
		Move();
	}

	m_PrevPos = m_Pos;
	if(m_Input.m_Direction)
		m_PrevDirection = m_Input.m_Direction;

	EmotesAction(m_EmotionsStyle);
	HandleWeapons();
}

void CCharacterBotAI::Move()
{
	bool Status = false;
	if(!m_pBotPlayer->m_ThreadReadNow.compare_exchange_strong(Status, true, std::memory_order::memory_order_acquire, std::memory_order::memory_order_relaxed))
		return;

	SetAim(m_pBotPlayer->m_TargetPos - m_Pos);

	int Index = -1;
	int ActiveWayPoints = 0;
	for(int i = 0; i < m_pBotPlayer->m_PathSize && i < 30 && !GS()->Collision()->IntersectLineWithInvisible(m_pBotPlayer->GetWayPoint(i), m_Pos, nullptr, nullptr); i++)
	{
		Index = i;
		ActiveWayPoints = i;
	}

	vec2 WayDir = vec2(0, 0);
	if(Index > -1)
		WayDir = normalize(m_pBotPlayer->GetWayPoint(Index) - GetPos());


	if(WayDir.x < 0 && ActiveWayPoints > 3)
		m_Input.m_Direction = -1;
	else if(WayDir.x > 0 && ActiveWayPoints > 3)
		m_Input.m_Direction = 1;
	else
		m_Input.m_Direction = m_PrevDirection;

	// check dissalow move
	if(m_pBotPlayer->GetBotType() != TYPE_BOT_EIDOLON && IsCollisionFlag(CCollision::COLFLAG_DISALLOW_MOVE))
	{
		m_Input.m_Direction = -m_Input.m_Direction;
		m_DoorHit = true;
		ClearTarget();
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
	CCharacter* pChar = GameWorld()->IntersectCharacter(GetPos(), GetPos() + vec2(m_Input.m_Direction, 0) * 128, 16.0f, IntersectPos, (CCharacter*)this);
	if (pChar && (pChar->GetPos().x < GetPos().x || !pChar->GetPlayer()->IsBot()))
		m_Input.m_Jump = 1;

	if(ActiveWayPoints > 2 && !m_Input.m_Hook && (WayDir.x != 0 || WayDir.y != 0))
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

			HookVel += vec2(0, 1) * GS()->Tuning()->m_Gravity;

			float ps = dot(WayDir, HookVel);
			if(ps > 0 || (WayDir.y < 0 && m_Core.m_Vel.y > 0.f && m_Core.m_HookTick < SERVER_TICK_SPEED + SERVER_TICK_SPEED / 2))
				m_Input.m_Hook = 1;
			if(m_Core.m_HookTick > 4 * SERVER_TICK_SPEED || length(m_Core.m_HookPos - GetPos()) < 20.0f)
				m_Input.m_Hook = 0;
		}
		else if(m_Core.m_HookState == HOOK_FLYING)
			m_Input.m_Hook = 1;
		else if(m_LatestInput.m_Hook == 0 && m_Core.m_HookState == HOOK_IDLE && random_int() % 3 == 0)
		{
			int NumDir = 45;
			vec2 HookDir(0.0f, 0.0f);
			float MaxForce = 0;

			for(int i = 0; i < NumDir; i++)
			{
				float a = 2 * i * pi / NumDir;
				vec2 dir = direction(a);
				vec2 Pos = GetPos() + dir * GS()->Tuning()->m_HookLength;

				if(GameWorld()->IntersectCharacter(GetPos(), Pos, 16.0f, IntersectPos, (CCharacter*)this))
					continue;

				if((GS()->Collision()->IntersectLine(GetPos(), Pos, &Pos, nullptr) & (CCollision::COLFLAG_SOLID | CCollision::COLFLAG_NOHOOK)) == CCollision::COLFLAG_SOLID)
				{
					vec2 HookVel = dir * GS()->Tuning()->m_HookDragAccel;
					if(HookVel.y > 0)
						HookVel.y *= 0.3f;
					if((HookVel.x < 0 && m_Input.m_Direction < 0) || (HookVel.x > 0 && m_Input.m_Direction > 0))
						HookVel.x *= 0.95f;
					else
						HookVel.x *= 0.75f;

					HookVel += vec2(0, 1) * GS()->Tuning()->m_Gravity;

					float ps = dot(WayDir, HookVel);
					if(ps > MaxForce)
					{
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
		m_MoveTick = Server()->Tick();

	if(m_Pos.x == m_PrevPos.x && Server()->Tick() - m_MoveTick > Server()->TickSpeed() / 2)
	{
		m_Input.m_Direction = -m_Input.m_Direction;
		m_Input.m_Jump = 1;
		m_MoveTick = Server()->Tick();
	}

	m_pBotPlayer->m_ThreadReadNow.store(false, std::memory_order::memory_order_release);
}


void CCharacterBotAI::Fire()
{
	CPlayer* pPlayer = GS()->GetPlayer(m_BotTargetID, false, true);
	if(IsBotTargetEmpty() || !pPlayer || m_BotTargetCollised)
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

// searching for a player among people
CPlayer* CCharacterBotAI::SearchPlayer(float Distance) const
{
	for(int i = 0 ; i < MAX_PLAYERS; i ++)
	{
		if(!GS()->m_apPlayers[i]
			|| !GS()->m_apPlayers[i]->GetCharacter()
			|| distance(m_Core.m_Pos, GS()->m_apPlayers[i]->GetCharacter()->m_Core.m_Pos) > Distance
			|| GS()->Collision()->IntersectLineWithInvisible(GS()->m_apPlayers[i]->GetCharacter()->m_Core.m_Pos, m_Pos, nullptr, nullptr)
			|| !GS()->IsPlayerEqualWorld(i))
			continue;
		return GS()->m_apPlayers[i];
	}
	return nullptr;
}

// finding a player among people who have the highest fury
CPlayer *CCharacterBotAI::SearchTenacityPlayer(float Distance)
{
	if(IsBotTargetEmpty() && (GS()->IsDungeon() || random_int() % 30 == 0))
	{
		CPlayer *pPlayer = SearchPlayer(Distance);
		if(pPlayer && pPlayer->GetCharacter())
			SetTarget(pPlayer->GetCID());
		return pPlayer;
	}

	// throw off aggression if the player is far away
	CPlayer* pPlayer = GS()->GetPlayer(m_BotTargetID, true, true);
	if (!IsBotTargetEmpty() && (!pPlayer
		|| (pPlayer && (distance(pPlayer->GetCharacter()->GetPos(), m_Pos) > 800.0f || !GS()->IsPlayerEqualWorld(m_BotTargetID)))))
		ClearTarget();

	// non-hostile mobs
	if (IsBotTargetEmpty() || !pPlayer)
		return nullptr;

	// throw off the lifetime of a target
	m_BotTargetCollised = GS()->Collision()->IntersectLineWithInvisible(pPlayer->GetCharacter()->GetPos(), m_Pos, nullptr, nullptr);
	if (m_BotTargetLife && m_BotTargetCollised)
	{
		m_BotTargetLife--;
		if (!m_BotTargetLife)
			ClearTarget();
	}
	else
	{
		m_BotTargetLife = Server()->TickSpeed() * 3;
	}

	// looking for a stronger
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		// check the distance of the player
		CPlayer* pFinderHard = GS()->GetPlayer(i, true, true);
		if (m_BotTargetID == i || !pFinderHard || distance(pFinderHard->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos) > 800.0f)
			continue;

		// check if the player is tastier for the bot
		const bool FinderCollised = GS()->Collision()->IntersectLineWithInvisible(pFinderHard->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos, nullptr, nullptr);
		if (!FinderCollised && ((m_BotTargetLife <= 10 && m_BotTargetCollised)
			|| pFinderHard->GetAttributeSize(AttributeIdentifier::Hardness, true) > pPlayer->GetAttributeSize(AttributeIdentifier::Hardness, true)))
			SetTarget(i);
	}

	return pPlayer;
}

// search mob
CPlayerBot* CCharacterBotAI::SearchMob(float Distance) const
{
	CPlayerBot* pBotPlayer = nullptr;

	// looking for a stronger
	for(int i = MAX_PLAYERS; i < MAX_CLIENTS; i++)
	{
		// check the distance of the player
		CPlayerBot* pCheckedPlayer = dynamic_cast<CPlayerBot*>(GS()->m_apPlayers[i]);
		if(!pCheckedPlayer || !pCheckedPlayer->GetCharacter() || distance(pCheckedPlayer->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos) > Distance || pCheckedPlayer->GetBotType() !=
			TYPE_BOT_MOB)
			continue;

		// check if the player is tastier for the bot
		const bool FinderCollised = GS()->Collision()->IntersectLineWithInvisible(pCheckedPlayer->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos, nullptr, nullptr);
		if(!FinderCollised)
		{
			pBotPlayer = dynamic_cast<CPlayerBot*>(GS()->m_apPlayers[i]);
			break;
		}
	}

	return pBotPlayer;
}

bool CCharacterBotAI::SearchTalkedPlayer()
{
	bool PlayerFinding = false;
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		CPlayer* pFindPlayer = GS()->GetPlayer(i, true, true);
		if(pFindPlayer && distance(pFindPlayer->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos) < 128.0f &&
			!GS()->Collision()->IntersectLineWithInvisible(pFindPlayer->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos, nullptr, nullptr) && m_pBotPlayer->IsVisibleForClient(i))
		{
			pFindPlayer->GetCharacter()->m_SafeAreaForTick = true;

			m_Input.m_TargetX = static_cast<int>(pFindPlayer->GetCharacter()->m_Core.m_Pos.x - m_Pos.x);
			m_Input.m_TargetY = static_cast<int>(pFindPlayer->GetCharacter()->m_Core.m_Pos.y - m_Pos.y);
			m_Input.m_Direction = 0;

			GS()->Broadcast(i, BroadcastPriority::GAME_INFORMATION, 10, "Begin dialog: \"hammer hit\"");
			PlayerFinding = true;
		}
	}
	return PlayerFinding;
}

void CCharacterBotAI::EmotesAction(int EmotionStyle)
{
	if (EmotionStyle < EMOTE_PAIN || EmotionStyle > EMOTE_BLINK)
		return;

	if ((Server()->Tick() % (Server()->TickSpeed() * 3 + random_int() % 10)) == 0)
	{
		if (EmotionStyle == EMOTE_BLINK)
		{
			SetEmote(EMOTE_BLINK, 1 + random_int() % 2);
			GS()->SendEmoticon(m_pBotPlayer->GetCID(), EMOTICON_DOTDOT);
		}
		else if (EmotionStyle == EMOTE_HAPPY)
		{
			SetEmote(EMOTE_HAPPY, 1 + random_int() % 2);
			GS()->SendEmoticon(m_pBotPlayer->GetCID(), (random_int() % 2 == 0 ? (int)EMOTICON_HEARTS : (int)EMOTICON_EYES));
		}
		else if (EmotionStyle == EMOTE_ANGRY)
		{
			SetEmote(EMOTE_ANGRY, 1 + random_int() % 2);
			GS()->SendEmoticon(m_pBotPlayer->GetCID(), (EMOTICON_SPLATTEE + random_int() % 3));
		}
		else if (EmotionStyle == EMOTE_PAIN)
		{
			SetEmote(EMOTE_PAIN, 1 + random_int() % 2);
			GS()->SendEmoticon(m_pBotPlayer->GetCID(), EMOTICON_DROP);
		}
	}
}

// - - - - - - - - - - - - - - - - - - - - - Target bot system
bool CCharacterBotAI::IsBotTargetEmpty() const
{
	return m_BotTargetID == m_pBotPlayer->GetCID();
}

void CCharacterBotAI::ClearTarget()
{
	const int FromID = m_pBotPlayer->GetCID();
	if(m_BotTargetID != FromID)
	{
		m_pBotPlayer->m_TargetPos = vec2(0, 0);
		m_EmotionsStyle = 1 + random_int() % 5;
		m_BotTargetID = FromID;
		m_BotTargetLife = 0;
		m_BotTargetCollised = 0;
	}
}

void CCharacterBotAI::SetTarget(int ClientID)
{
	m_EmotionsStyle = EMOTE_ANGRY;
	m_BotTargetID = ClientID;
}

// - - - - - - - - - - - - - - - - - - - - - Npc functions
bool CCharacterBotAI::BaseFunctionNPC()
{
	const bool PlayerFinding = SearchTalkedPlayer();
	return PlayerFinding;
}

bool CCharacterBotAI::FunctionNurseNPC()
{
	char aBuf[16];
	bool PlayerFinding = false;
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		CPlayer* pFindPlayer = GS()->GetPlayer(i, true, true);
		if(!pFindPlayer || distance(pFindPlayer->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos) >= 256.0f ||
			GS()->Collision()->IntersectLine(pFindPlayer->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos, nullptr, nullptr))
			continue;

		// disable collision and hook with players
		float Distance = distance(pFindPlayer->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos);
		if(Distance < 128.0f)
			pFindPlayer->GetCharacter()->m_SafeAreaForTick = true;
		else if(Distance < 64.0f)
			m_Input.m_Direction = 0;

		// skip full health
		if(pFindPlayer->GetHealth() >= pFindPlayer->GetStartHealth())
			continue;

		// set target to player
		m_Input.m_TargetX = static_cast<int>(pFindPlayer->GetCharacter()->m_Core.m_Pos.x - m_Pos.x);
		m_Input.m_TargetY = static_cast<int>(pFindPlayer->GetCharacter()->m_Core.m_Pos.y - m_Pos.y);
		m_LatestInput.m_TargetX = m_Input.m_TargetX;
		m_LatestInput.m_TargetY = m_Input.m_TargetX;
		PlayerFinding = true;

		// health every sec
		if(Server()->Tick() % Server()->TickSpeed() != 0)
			continue;

		// increase health for player
		const int Health = max(pFindPlayer->GetStartHealth() / 20, 1);
		vec2 DrawPosition = vec2(pFindPlayer->GetCharacter()->m_Core.m_Pos.x, pFindPlayer->GetCharacter()->m_Core.m_Pos.y - 90.0f);
		str_format(aBuf, sizeof(aBuf), "%dHP", Health);
		GS()->CreateText(nullptr, false, DrawPosition, vec2(0, 0), 40, aBuf);
		new CHearth(&GS()->m_World, m_Pos, pFindPlayer, Health, pFindPlayer->GetCharacter()->m_Core.m_Vel);

		m_Input.m_Direction = 0;
	}
	return PlayerFinding;
}
