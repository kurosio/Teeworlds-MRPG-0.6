/* (c) Alexandre Díaz. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>
#include <game/server/playerbot.h>
#include "character_bot_ai.h"

#include <game/collision.h>

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
	const int SubBotID = m_pBotPlayer->GetBotSub();
	if(m_pBotPlayer->GetBotType() == BotsTypes::TYPE_BOT_MOB && MobBotInfo::ms_aMobBot[SubBotID].m_Boss)
	{
		for(int i = 0; i < 3; i++)
		{
			CreateSnapProj(GetSnapFullID(), 1, PICKUP_HEALTH, true, false);
			CreateSnapProj(GetSnapFullID(), 1, WEAPON_HAMMER, false, true);
		}
		if (!GS()->IsDungeon())
			GS()->ChatWorldID(MobBotInfo::ms_aMobBot[SubBotID].m_WorldID, "", "In your zone emerging {STR}!", MobBotInfo::ms_aMobBot[SubBotID].GetName());
	}
	else if(m_pBotPlayer->GetBotType() == BotsTypes::TYPE_BOT_QUEST)
	{
		m_Core.m_NoCollision = true;
		GS()->SendEquipments(m_pBotPlayer->GetCID(), -1);
		CreateSnapProj(GetSnapFullID(), 2, PICKUP_HEALTH, true, false);
		CreateSnapProj(GetSnapFullID(), 2, PICKUP_ARMOR, true, false);
	}
	else if(m_pBotPlayer->GetBotType() == BotsTypes::TYPE_BOT_NPC)
	{
		m_Core.m_NoCollision = true;
		const int Function = NpcBotInfo::ms_aNpcBot[SubBotID].m_Function;
		if(Function == FunctionsNPC::FUNCTION_NPC_GIVE_QUEST)
			CreateSnapProj(GetSnapFullID(), 3, PICKUP_ARMOR, false, false);
	}
	return true;
}

void CCharacterBotAI::ShowProgressHealth()
{
	for(const auto & pPlayerDamage : m_aListDmgPlayers)
	{
		CPlayer *pPlayer = GS()->GetPlayer(pPlayerDamage.first, true);
		if(pPlayer)
		{
			const int BotID = m_pBotPlayer->GetBotID();
			const int Health = m_pBotPlayer->GetHealth();
			const int StartHealth = m_pBotPlayer->GetStartHealth();
			const float Percent = (Health * 100.0) / StartHealth;
			std::unique_ptr<char[]> Progress = std::move(GS()->LevelString(100, Percent, 10, ':', ' '));
			GS()->Broadcast(pPlayerDamage.first, BroadcastPriority::GAME_PRIORITY, 100, "{STR} {STR}({INT}/{INT})",
				DataBotInfo::ms_aDataBot[BotID].m_aNameBot, Progress.get(), Health, StartHealth);
		}
	}
}

void CCharacterBotAI::GiveRandomEffects(int To)
{
	CPlayer* pPlayerTo = GS()->GetPlayer(To);
	if(!pPlayerTo && To != m_pBotPlayer->GetCID())
		return;

	const int SubID = m_pBotPlayer->GetBotSub();
	if(m_pBotPlayer->GetBotType() == BotsTypes::TYPE_BOT_MOB && MobBotInfo::ms_aMobBot[SubID].m_aEffect[0] != '\0')
		pPlayerTo->GiveEffect(MobBotInfo::ms_aMobBot[SubID].m_aEffect, 3 + random_int() % 3, 5.0f);
}

bool CCharacterBotAI::TakeDamage(vec2 Force, int Dmg, int From, int Weapon)
{
	CPlayer* pFrom = GS()->GetPlayer(From, true);
	if (!pFrom || !m_BotActive)
		return false;

	if(m_pBotPlayer->GetBotType() != BotsTypes::TYPE_BOT_MOB || pFrom->IsBot())
		return false;

	// damage receive
	CCharacter::TakeDamage(Force, Dmg, From, Weapon);

	// if the bot doesn't have target player, set to from
	if(IsBotTargetEmpty())
		SetTarget(From);

	// add (from player) to the list of those who caused damage
	if(m_aListDmgPlayers.find(From) == m_aListDmgPlayers.end())
		m_aListDmgPlayers[From] = true;

	// verify death
	if(m_Health <= 0)
	{
		if(Weapon != WEAPON_SELF && Weapon != WEAPON_WORLD)
		{
			for(const auto& ld : m_aListDmgPlayers)
			{
				const int ClientID = ld.first;
				CPlayer* pPlayer = GS()->GetPlayer(ClientID, true, true);
				if(!pPlayer || !GS()->IsPlayerEqualWorldID(ClientID, m_pBotPlayer->GetPlayerWorldID())
					|| distance(pPlayer->m_ViewPos, m_Core.m_Pos) > 1000.0f)
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
	if(m_pBotPlayer->GetBotType() != BotsTypes::TYPE_BOT_MOB)
		return;

	CCharacter::Die(Killer, Weapon);
}

void CCharacterBotAI::RewardPlayer(CPlayer* pPlayer, vec2 Force) const
{
	const int ClientID = pPlayer->GetCID();
	const int BotID = m_pBotPlayer->GetBotID();
	const int SubID = m_pBotPlayer->GetBotSub();

	// quest mob progress
	if(m_pBotPlayer->GetBotType() == BotsTypes::TYPE_BOT_MOB)
		GS()->Mmo()->Quest()->AddMobProgressQuests(pPlayer, BotID);

	// golds
	const int Golds = max(MobBotInfo::ms_aMobBot[SubID].m_Power / g_Config.m_SvStrongGold, 1);
	pPlayer->AddMoney(Golds);

	// experience
	const int ExperienceMob = computeExperience(MobBotInfo::ms_aMobBot[SubID].m_Level) / g_Config.m_SvKillmobsIncreaseLevel;
	const int ExperienceWithMultiplier = max(1, GS()->GetExperienceMultiplier(ExperienceMob));
	GS()->CreateParticleExperience(m_Core.m_Pos, ClientID, ExperienceWithMultiplier, Force);

	// drop experience
	const int ExperienceDrop = max(ExperienceWithMultiplier / 2, 1);
	GS()->CreateDropBonuses(m_Core.m_Pos, 1, ExperienceDrop, (1 + random_int() % 2), Force);

	// drop item's
	const float ActiveLuckyDrop = clamp((float)pPlayer->GetAttributeCount(Stats::StLuckyDropItem, true) / 100.0f, 0.01f, 10.0f);
	for(int i = 0; i < 5; i++)
	{
		CItemData DropItem;
		DropItem.m_ItemID = MobBotInfo::ms_aMobBot[SubID].m_aDropItem[i];
		DropItem.m_Value = MobBotInfo::ms_aMobBot[SubID].m_aValueItem[i];
		if(DropItem.m_ItemID <= 0 || DropItem.m_Value <= 0)
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
		CItemData &pItemSkillPlayer = pPlayer->GetItem(itSkillPoint);
		pItemSkillPlayer.Add(1);
		GS()->Chat(ClientID, "Skill points increased. Now ({INT}SP)", pItemSkillPlayer.m_Value);
	}
}

void CCharacterBotAI::ChangeWeapons()
{
	const int RandomSec = 1+random_int()%3;
	if(Server()->Tick() % (Server()->TickSpeed()*RandomSec) == 0)
	{
		const int RandomWeapon = random_int()%4;
		m_ActiveWeapon = clamp(RandomWeapon, (int)WEAPON_HAMMER, (int)WEAPON_LASER);
	}
}

bool CCharacterBotAI::GiveWeapon(int Weapon, int GiveAmmo)
{
	const int WeaponID = clamp(Weapon, (int)WEAPON_HAMMER, (int)WEAPON_NINJA);
	m_aWeapons[WeaponID].m_Got = true;
	m_aWeapons[WeaponID].m_Ammo = GiveAmmo;
	return true;
}

void CCharacterBotAI::Tick()
{
	m_BotActive = GS()->CheckingPlayersDistance(m_Core.m_Pos, 1000.0f);
	if(!m_BotActive || !IsAlive())
		return;

	EngineBots();
	HandleEvents();
	HandleTilesets();
	CCharacter::Tick();
}

void CCharacterBotAI::TickDefered()
{
	if(!m_BotActive || !IsAlive())
		return;

	CCharacter::TickDefered();
}

// interactive bots
void CCharacterBotAI::EngineBots()
{
	if(m_pBotPlayer->GetBotType() == BotsTypes::TYPE_BOT_NPC)
	{
		const int tx = m_Pos.x + m_Input.m_Direction * 45.0f;
		if(tx < 0)
			m_Input.m_Direction = 1;
		else if(tx >= GS()->Collision()->GetWidth() * 32.0f)
			m_Input.m_Direction = -1;

		m_LatestPrevInput = m_LatestInput;
		m_LatestInput = m_Input;

		EngineNPC();
	}
	else if(m_pBotPlayer->GetBotType() == BotsTypes::TYPE_BOT_MOB)
		EngineMobs();
	else if(m_pBotPlayer->GetBotType() == BotsTypes::TYPE_BOT_QUEST)
		EngineQuestMob();
}

// interactive of NPC
void CCharacterBotAI::EngineNPC()
{
	const int MobID = m_pBotPlayer->GetBotSub();
	const int EmoteBot = NpcBotInfo::ms_aNpcBot[MobID].m_Emote;
	EmotesAction(EmoteBot);

	// direction eyes
	if(Server()->Tick() % Server()->TickSpeed() == 0)
		m_Input.m_TargetY = random_int()%4- random_int()%8;
	m_Input.m_TargetX = (m_Input.m_Direction*10+1);

	bool PlayerFinding;
	if(NpcBotInfo::ms_aNpcBot[MobID].m_Function == FunctionsNPC::FUNCTION_NPC_NURSE)
		PlayerFinding = FunctionNurseNPC();
	else
		PlayerFinding = BaseFunctionNPC();

	// walking for npc
	if(!PlayerFinding && !NpcBotInfo::ms_aNpcBot[MobID].m_Static && random_int() % 50 == 0)
	{
		const int RandomDirection = random_int() % 6;
		if(RandomDirection == 0 || RandomDirection == 2)
			m_Input.m_Direction = -1 + RandomDirection;
		else m_Input.m_Direction = 0;
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

// interactive of Mobs
void CCharacterBotAI::EngineMobs()
{
	ResetInput();
	CPlayer* pPlayer = SearchTenacityPlayer(1000.0f);
	if(pPlayer && pPlayer->GetCharacter())
	{
		m_pBotPlayer->m_TargetPos = pPlayer->GetCharacter()->GetPos();
		Action();
	}
	else if(Server()->Tick() > m_pBotPlayer->m_LastPosTick)
		m_pBotPlayer->m_TargetPos = vec2(0, 0);

	// behavior sleppy
	const int MobID = m_pBotPlayer->GetBotSub();
	if(IsBotTargetEmpty() && str_comp(MobBotInfo::ms_aMobBot[MobID].m_aBehavior, "Sleepy") == 0)
	{
		if(Server()->Tick() % (Server()->TickSpeed() / 2) == 0)
		{
			GS()->SendEmoticon(m_pBotPlayer->GetCID(), EMOTICON_ZZZ);
			SetEmote(EMOTE_BLINK, 1);
		}
		return;
	}

	if(MobBotInfo::ms_aMobBot[MobID].m_Boss)
		ShowProgressHealth();

	const bool WeaponedBot = (MobBotInfo::ms_aMobBot[MobID].m_Spread >= 1);
	if(WeaponedBot)
		ChangeWeapons();

	Move();

	m_PrevPos = m_Pos;
	if(m_Input.m_Direction)
		m_PrevDirection = m_Input.m_Direction;

	EmotesAction(m_EmotionsStyle);
}

void CCharacterBotAI::Move()
{
	bool Status = false;
	if(!m_pBotPlayer->m_ThreadReadNow.compare_exchange_strong(Status, true, std::memory_order::memory_order_acquire, std::memory_order::memory_order_relaxed))
		return;

	SetAim(m_pBotPlayer->m_TargetPos - m_Pos);

	int Index = -1;
	int ActiveWayPoints = 0;
	for(int i = 0; i < m_pBotPlayer->m_PathSize && i < 30 && !GS()->Collision()->IntersectLineWithInvisible(m_pBotPlayer->GetWayPoint(i), m_Pos, 0, 0); i++)
	{
		Index = i;
		ActiveWayPoints = i;
	}

	vec2 WayDir = vec2(0, 0);
	if(Index > -1)
		WayDir = normalize(m_pBotPlayer->GetWayPoint(Index) - GetPos());

	// set the direction
	if(WayDir.x < 0 && ActiveWayPoints > 3)
		m_Input.m_Direction = -1;
	else if(WayDir.x > 0 && ActiveWayPoints > 3)
		m_Input.m_Direction = 1;
	else
		m_Input.m_Direction = m_PrevDirection;

	// jumping
	const bool IsGround = IsGrounded();
	if((IsGround && WayDir.y < -0.5) || (!IsGround && WayDir.y < -0.5 && m_Core.m_Vel.y > 0))
		m_Input.m_Jump = 1;

	const bool IsCollide = GS()->Collision()->IntersectLine(m_Pos, m_Pos + vec2(m_Input.m_Direction, 0) * 150, &m_WallPos, 0x0);
	if(IsCollide)
	{
		if(IsGround && GS()->Collision()->IntersectLine(m_WallPos, m_WallPos + vec2(0, -1) * 210, 0x0, 0x0))
			m_Input.m_Jump = 1;

		if(!IsGround && GS()->Collision()->IntersectLine(m_WallPos, m_WallPos + vec2(0, -1) * 125, 0x0, 0x0))
			m_Input.m_Jump = 1;
	}

	// if way points down dont jump
	if(m_Input.m_Jump == 1 && (WayDir.y >= 0 || ActiveWayPoints < 3))
		m_Input.m_Jump = 0;

	// jump over character
	vec2 IntersectPos;
	CCharacter* pChar = GameWorld()->IntersectCharacter(GetPos(), GetPos() + vec2(m_Input.m_Direction, 0) * 128, 16.0f, IntersectPos, this);
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

			float ps = dot(WayDir, HookVel);
			if(ps > 0 || (WayDir.y < 0 && m_Core.m_Vel.y > 0.f && m_Core.m_HookTick < SERVER_TICK_SPEED + SERVER_TICK_SPEED / 2))
				m_Input.m_Hook = 1;
			if(m_Core.m_HookTick > 4 * SERVER_TICK_SPEED || length(m_Core.m_HookPos - GetPos()) < 20.0f)
				m_Input.m_Hook = 0;
		}
		else if(m_Core.m_HookState == HOOK_FLYING)
			m_Input.m_Hook = 1;
		else if(m_LatestInput.m_Hook == 0 && m_Core.m_HookState == HOOK_IDLE)
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

void CCharacterBotAI::Action()
{
	CPlayer* pPlayer = GS()->GetPlayer(m_BotTargetID, true, true);
	if(IsBotTargetEmpty() || !pPlayer || m_BotTargetCollised)
		return;

	if((m_Input.m_Hook && m_Core.m_HookState == HOOK_IDLE) || m_ReloadTimer != 0)
		return;

	if(m_ActiveWeapon == WEAPON_HAMMER && distance(pPlayer->GetCharacter()->GetPos(), GetPos()) > 128.0f)
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
			|| GS()->Collision()->IntersectLineWithInvisible(GS()->m_apPlayers[i]->GetCharacter()->m_Core.m_Pos, m_Pos, 0, 0)
			|| !GS()->IsPlayerEqualWorldID(i))
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
		|| (pPlayer && (distance(pPlayer->GetCharacter()->GetPos(), m_Pos) > 800.0f || !GS()->IsPlayerEqualWorldID(m_BotTargetID)))))
		ClearTarget();

	// non-hostile mobs
	if (IsBotTargetEmpty() || !pPlayer)
		return nullptr;

	// throw off the lifetime of a target
	m_BotTargetCollised = GS()->Collision()->IntersectLineWithInvisible(pPlayer->GetCharacter()->GetPos(), m_Pos, 0, 0);
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
		if (!pFinderHard || distance(pFinderHard->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos) > 800.0f)
			continue;

		// check if the player is tastier for the bot
		const bool FinderCollised = GS()->Collision()->IntersectLineWithInvisible(pFinderHard->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos, 0, 0);
		if (!FinderCollised && ((m_BotTargetLife <= 10 && m_BotTargetCollised)
			|| pFinderHard->GetAttributeCount(Stats::StHardness, true) > pPlayer->GetAttributeCount(Stats::StHardness, true)))
			SetTarget(i);
	}

	return pPlayer;
}

bool CCharacterBotAI::SearchTalkedPlayer()
{
	bool PlayerFinding = false;
	const int MobID = m_pBotPlayer->GetBotSub();
	const bool DialoguesNotEmpty = ((m_pBotPlayer->GetBotType() == BotsTypes::TYPE_BOT_QUEST && !QuestBotInfo::ms_aQuestBot[MobID].m_aDialog.empty())
				|| (m_pBotPlayer->GetBotType() == BotsTypes::TYPE_BOT_NPC && !(NpcBotInfo::ms_aNpcBot[MobID].m_aDialog).empty()));
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		CPlayer* pFindPlayer = GS()->GetPlayer(i, true, true);
		if(pFindPlayer && distance(pFindPlayer->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos) < 128.0f &&
			!GS()->Collision()->IntersectLine(pFindPlayer->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos, 0, 0) && m_pBotPlayer->IsVisibleForClient(i))
		{
			if (DialoguesNotEmpty)
				GS()->Broadcast(i, BroadcastPriority::GAME_INFORMATION, 10, "Begin dialog: \"hammer hit\"");

			pFindPlayer->GetCharacter()->m_Core.m_NoCollision = true;
			m_Input.m_TargetX = static_cast<int>(pFindPlayer->GetCharacter()->m_Core.m_Pos.x - m_Pos.x);
			m_Input.m_TargetY = static_cast<int>(pFindPlayer->GetCharacter()->m_Core.m_Pos.y - m_Pos.y);
			m_Input.m_Direction = 0;
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
			GS()->Collision()->IntersectLine(pFindPlayer->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos, 0, 0))
			continue;

		// disable collision with players
		if(distance(pFindPlayer->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos) < 128.0f)
			pFindPlayer->GetCharacter()->m_Core.m_NoCollision = true;

		// skip full health
		if(pFindPlayer->GetHealth() >= pFindPlayer->GetStartHealth())
			continue;

		// set target to player
		m_Input.m_TargetX = static_cast<int>(pFindPlayer->GetCharacter()->m_Core.m_Pos.x - m_Pos.x);
		m_Input.m_TargetY = static_cast<int>(pFindPlayer->GetCharacter()->m_Core.m_Pos.y - m_Pos.y);
		m_LatestInput.m_TargetX = m_Input.m_TargetX;
		m_LatestInput.m_TargetY = m_Input.m_TargetX;

		// health every sec
		if(Server()->Tick() % Server()->TickSpeed() != 0)
			continue;

		// increase health for player
		const int Health = max(pFindPlayer->GetStartHealth() / 20, 1);
		vec2 DrawPosition = vec2(pFindPlayer->GetCharacter()->m_Core.m_Pos.x, pFindPlayer->GetCharacter()->m_Core.m_Pos.y - 90.0f);
		str_format(aBuf, sizeof(aBuf), "%dHP", Health);
		GS()->CreateText(NULL, false, DrawPosition, vec2(0, 0), 40, aBuf);
		new CHearth(&GS()->m_World, m_Pos, pFindPlayer, Health, pFindPlayer->GetCharacter()->m_Core.m_Vel);

		m_Input.m_Direction = 0;
		PlayerFinding = true;
	}
	return PlayerFinding;
}
