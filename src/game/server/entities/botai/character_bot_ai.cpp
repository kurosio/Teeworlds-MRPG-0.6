/* (c) Alexandre Díaz. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/server/entity_manager.h>
#include <game/server/gamecontext.h>
#include <game/server/playerbot.h>
#include "character_bot_ai.h"

#include <game/collision.h>

#include <game/server/core/components/skills/entities/heart_healer.h> // for nurse
#include <game/server/core/components/Bots/BotData.h>
#include <game/server/core/components/quests/quest_manager.h>

#include <game/server/core/tools/path_finder.h>

#include <game/server/core/scenarios/scenario_eidolon.h>

#include "nurse_heart.h"

MACRO_ALLOC_POOL_ID_IMPL(CCharacterBotAI, MAX_CLIENTS* ENGINE_MAX_WORLDS + MAX_CLIENTS)

CCharacterBotAI::CCharacterBotAI(CGameWorld* pWorld) : CCharacter(pWorld)
{
	m_pAI = new CAIController(this);
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
	if(CCharacter::Spawn(m_pBotPlayer, Pos))
	{
		InitBot();
		return true;
	}

	return false;
}

void CCharacterBotAI::InitBot()
{
	const int ClientID = m_pBotPlayer->GetCID();
	const int MobID = m_pBotPlayer->GetBotMobID();
	const int Bottype = m_pBotPlayer->GetBotType();

	if(Bottype == TYPE_BOT_MOB)
	{
		const auto* pMobBot = &MobBotInfo::ms_aMobBot[MobID];

		if(pMobBot->m_Boss)
		{
			AddMultipleOrbite(1, POWERUP_HEALTH, 0);
			AddMultipleOrbite(1, POWERUP_ARMOR, 0);

			if(!GS()->IsWorldType(WorldType::Dungeon))
			{
				GS()->ChatWorld(pMobBot->m_WorldID, nullptr, "In your zone emerging {}!", pMobBot->GetName());
			}
		}
	}
	else if(Bottype == TYPE_BOT_QUEST)
	{
		const auto* pQuestBot = &QuestBotInfo::ms_aQuestBot[MobID];

		if(pQuestBot->m_HasAction)
		{
			GS()->EntityManager()->EffectCircleDamage(ClientID, Server()->TickSpeed() / 2, Server()->TickSpeed());
		}
	}
	else if(Bottype == TYPE_BOT_NPC)
	{
		const auto* pNpcBot = &NpcBotInfo::ms_aNpcBot[MobID];
		const int Function = pNpcBot->m_Function;

		if(Function == FUNCTION_NPC_GIVE_QUEST)
		{
			AddMultipleOrbite(3, POWERUP_ARMOR, 0);
		}
		else if(Function == FUNCTION_NPC_NURSE)
		{
			new CNurseHeart(GameWorld(), ClientID);
		}
		else if(Function == FUNCTION_NPC_GUARDIAN)
		{
			AddMultipleOrbite(2, POWERUP_NINJA, POWERUP_WEAPON);
		}
	}
	else if(Bottype == TYPE_BOT_EIDOLON)
	{
		m_pBotPlayer->Scenarios().Start(std::make_unique<CEidolonScenario>());
		m_Core.m_Solo = true;
	}
}

void CCharacterBotAI::GiveRandomEffects(int ClientID)
{
	if(m_pBotPlayer->GetBotType() == TYPE_BOT_MOB)
	{
		CPlayer* pPlayer = GS()->GetPlayer(ClientID);

		if(pPlayer && ClientID != m_pBotPlayer->GetCID())
		{
			const int MobID = m_pBotPlayer->GetBotMobID();
			auto* pMobBot = &MobBotInfo::ms_aMobBot[MobID];

			if(const auto* pBuff = pMobBot->GetRandomEffect())
			{
				pPlayer->GiveEffect(pBuff->getEffect(), pBuff->getTime(), pBuff->getChance());
			}
		}
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

	// Update ai target
	if(AI()->GetTarget()->IsEmpty())
	{
		if(!pFrom->IsBot())
		{
			AI()->GetTarget()->Set(From, 200);
		}
		else if(pFrom->GetBotType() == TYPE_BOT_EIDOLON)
		{
			AI()->GetTarget()->Set(From, 150);
		}
	}

	// Is player bot npc
	if(m_pBotPlayer->GetBotType() == TYPE_BOT_NPC)
	{
		// increase crime score for damage to friendly NPCs
		if(!pFrom->IsBot())
		{
			pFrom->Account()->IncreaseCrimeScore(1 + rand() % 8);
			SetEmote(EMOTE_ANGRY, 1, true);
		}
	}

	// Random create experience point's
	if(rand() % 10 == 0)
	{
		GS()->EntityManager()->DropBonus(m_Core.m_Pos, POWERUP_ARMOR, 0, 1, 1, Force);
	}

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
	// is quest bot mob
	if(m_pBotPlayer->GetBotType() == TYPE_BOT_QUEST_MOB)
	{
		HandleQuestMobReward(pPlayer);
	}

	// is mob bot
	if(m_pBotPlayer->GetBotType() == TYPE_BOT_MOB)
	{
		HandleMobReward(pPlayer);
	}
}

void CCharacterBotAI::HandleQuestMobReward(CPlayer* pPlayer) const
{
	const int ClientID = pPlayer->GetCID();
	const int BotID = m_pBotPlayer->GetBotID();
	auto& questBotMobData = m_pBotPlayer->GetQuestBotMobInfo();

	if(questBotMobData.m_ActiveForClient[ClientID])
	{
		questBotMobData.m_CompleteClient[ClientID] = true;
	}

	// append defeat mob progress
	GS()->Core()->QuestManager()->TryAppendDefeatProgress(pPlayer, BotID);
}

void CCharacterBotAI::HandleMobReward(CPlayer* pPlayer) const
{
	const int ClientID = pPlayer->GetCID();
	const int BotID = m_pBotPlayer->GetBotID();
	const int SubID = m_pBotPlayer->GetBotMobID();
	const int MobLevel = MobBotInfo::ms_aMobBot[SubID].m_Level;
	const int PlayerLevel = pPlayer->Account()->GetLevel();
	const vec2& Force = m_DieForce;
	const bool showMessages = pPlayer->GetItem(itShowDetailGainMessages)->IsEquipped();

	// grinding gold
	{
		if(pPlayer->Account()->GetGold() < pPlayer->Account()->GetGoldCapacity())
		{
			const int goldGain = calculate_gold_gain(g_Config.m_SvKillmobsGoldFactor, PlayerLevel, MobLevel, true);

			if(showMessages)
			{
				GS()->Chat(ClientID, "You gained {} gold.", goldGain);
			}

			pPlayer->Account()->AddGold(goldGain, false, true);
		}
	}

	// grinding experience
	{
		int expGain = calculate_exp_gain(g_Config.m_SvKillmobsExpFactor, PlayerLevel, MobLevel);
		const int expBonusDrop = maximum(expGain / 3, 1);

		GS()->ApplyExperienceMultiplier(&expGain);

		if(showMessages)
		{
			GS()->Chat(ClientID, "You gained {} exp.", expGain);
		}

		GS()->EntityManager()->ExpFlyingPoint(m_Core.m_Pos, ClientID, expGain, Force);
		GS()->EntityManager()->DropBonus(m_Core.m_Pos, POWERUP_ARMOR, 0, expBonusDrop, (1 + rand() % 2), Force);
	}

	// drop item's
	{
		const float ActiveLuckyDrop = clamp((float)pPlayer->GetTotalAttributeValue(AttributeIdentifier::LuckyDropItem) / 100.0f, 0.01f, 10.0f);

		for(int i = 0; i < MAX_DROPPED_FROM_MOBS; i++)
		{
			const int DropID = MobBotInfo::ms_aMobBot[SubID].m_aDropItem[i];
			const int DropValue = MobBotInfo::ms_aMobBot[SubID].m_aValueItem[i];

			if(DropID <= 0 || DropValue <= 0)
				continue;

			const float RandomDrop = clamp(MobBotInfo::ms_aMobBot[SubID].m_aRandomItem[i] + ActiveLuckyDrop, 0.0f, 100.0f);
			const vec2 ForceRandom = random_range_pos(Force, 4.f);

			CItem DropItem;
			DropItem.SetID(DropID);
			DropItem.SetValue(DropValue);
			GS()->EntityManager()->RandomDropItem(m_Core.m_Pos, ClientID, RandomDrop, DropItem, ForceRandom);
		}
	}

	// skill points
	{
		const bool isRareMob = MobBotInfo::ms_aMobBot[SubID].m_Boss;
		float BaseChance = isRareMob ? g_Config.m_SvSPChanceDropRareMob : g_Config.m_SvSPChanceDropMob;
		const int LevelDifference = PlayerLevel - MobLevel;

		// check leveling difference
		if(LevelDifference > 0)
		{
			const int AdjustmentSteps = LevelDifference / 5;
			BaseChance -= BaseChance * (0.10f * AdjustmentSteps);
		}
		BaseChance = maximum(1.0f, BaseChance);

		// try generate chance
		if(random_float(100.f) <= BaseChance)
		{
			CPlayerItem* pPlayerItem = pPlayer->GetItem(itSkillPoint);
			pPlayerItem->Add(1);
			GS()->Chat(ClientID, "Skill points increased. Now you have {} SP!", pPlayerItem->GetValue());
		}
	}

	// append defeat mob progress
	GS()->Core()->QuestManager()->TryAppendDefeatProgress(pPlayer, BotID);
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

void CCharacterBotAI::SelectEmoteAtRandomInterval(int EmotionStyle)
{
	if(EmotionStyle < EMOTE_PAIN || EmotionStyle > EMOTE_BLINK)
		return;

	const int emoteInterval = Server()->TickSpeed() * 3 + rand() % 10;
	if(Server()->Tick() % emoteInterval == 0)
	{
		const int duration = 1 + rand() % 2;
		SetEmote(EMOTE_BLINK, duration, true);
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
	ProcessBotBehavior();
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

	auto DDNetFlag = [&](int flag, bool check) { if(check) pDDNetCharacter->m_Flags |= flag; };
	if(m_pBotPlayer->GetBotType() == TYPE_BOT_EIDOLON)
	{
		CPlayer* pOwner = m_pBotPlayer->GetEidolonOwner();
		DDNetFlag(CHARACTERFLAG_SOLO, !(pOwner && pOwner->GetCID() == SnappingClient));
		DDNetFlag(CHARACTERFLAG_COLLISION_DISABLED, true);
	}
	else if(m_pBotPlayer->GetBotType() == TYPE_BOT_QUEST_MOB)
	{
		bool IsActiveForSnappingClient = m_pBotPlayer->GetQuestBotMobInfo().m_ActiveForClient[SnappingClient];
		DDNetFlag(CHARACTERFLAG_SOLO, !IsActiveForSnappingClient);
		DDNetFlag(CHARACTERFLAG_COLLISION_DISABLED, !IsActiveForSnappingClient);
	}

	pDDNetCharacter->m_Jumps = m_Core.m_Jumps;
	pDDNetCharacter->m_TeleCheckpoint = 0;
	pDDNetCharacter->m_StrongWeakId = 0;
	pDDNetCharacter->m_TargetX = m_Core.m_Input.m_TargetX;
	pDDNetCharacter->m_TargetY = m_Core.m_Input.m_TargetY;
}

void CCharacterBotAI::ProcessBotBehavior()
{
	const int Bottype = m_pBotPlayer->GetBotType();

	AI()->GetTarget()->Tick();

	// bevavior mobs
	if(Bottype == TYPE_BOT_MOB)
	{
		ProcessMobs();
		return;
	}

	// bevavior eidolon
	if(Bottype == TYPE_BOT_EIDOLON)
	{
		SetSafeFlags(SAFEFLAG_COLLISION_DISABLED);
		ProcessEidolons();
		return;
	}

	// bevavior quest mob
	if(Bottype == TYPE_BOT_QUEST_MOB)
	{
		ProcessQuestMob();
		return;
	}

	// bevavior quest bot
	if(Bottype == TYPE_BOT_QUEST)
	{
		SetSafeFlags();
		ProcessQuestNPC();
		return;
	}

	// bevavior npc
	if(Bottype == TYPE_BOT_NPC)
	{
		const int MobID = m_pBotPlayer->GetBotMobID();
		const auto* pNpcBot = &NpcBotInfo::ms_aNpcBot[MobID];

		// has functional guardian
		if(pNpcBot->m_Function == FUNCTION_NPC_GUARDIAN)
		{
			ProcessGuardianNPC();
			return;
		}

		// has functional nurse
		if(pNpcBot->m_Function == FUNCTION_NPC_NURSE)
		{
			//SetSafeFlags(SAFEFLAG_COLLISION_DISABLED);
			//ProcessNPC();
			return;
		}

		SetSafeFlags();
		ProcessNPC();
		return;
	}
}

// interactive of NPC
void CCharacterBotAI::ProcessNPC()
{
	const int MobID = m_pBotPlayer->GetBotMobID();
	const auto* pNpcBot = &NpcBotInfo::ms_aNpcBot[MobID];
	const float collisionWidth = GS()->Collision()->GetWidth() * 32.0f;
	const float npcPosX = m_Pos.x + m_Input.m_Direction * 45.0f;

	m_Input.m_Direction = (npcPosX < 0) ? 1 : (npcPosX >= collisionWidth) ? -1 : m_Input.m_Direction;
	m_LatestPrevInput = m_LatestInput;
	m_LatestInput = m_Input;

	// behavior
	bool isPlayerNearby = BaseFunctionNPC();

	// emote actions
	SelectEmoteAtRandomInterval(pNpcBot->m_Emote);

	// random direction target
	if(Server()->Tick() % Server()->TickSpeed() == 0)
	{
		m_Input.m_TargetY = (rand() % 9) - 4;
	}
	m_Input.m_TargetX = m_Input.m_Direction * 10 + 1;

	// random direction moving
	if(!isPlayerNearby && !pNpcBot->m_Static && rand() % 50 == 0)
	{
		m_Input.m_Direction = -1 + rand() % 3;
	}
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
		MobBotInfo* pMobBot = &MobBotInfo::ms_aMobBot[MobID];
		if(pMobBot->m_BehaviorSets & MOBFLAG_BEHAVIOR_SLOWER)
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
	if(m_pBotPlayer->GetBotType() == TYPE_BOT_MOB)
	{
		const int MobID = m_pBotPlayer->GetBotMobID();
		MobBotInfo* pMobBot = &MobBotInfo::ms_aMobBot[MobID];

		// sleepy behavior
		if(AI()->GetTarget()->IsEmpty() && pMobBot->m_BehaviorSets & MOBFLAG_BEHAVIOR_SLEEPY)
		{
			if(Server()->Tick() % (Server()->TickSpeed() / 2) == 0)
			{
				GS()->SendEmoticon(m_pBotPlayer->GetCID(), EMOTICON_ZZZ);
				SetEmote(EMOTE_BLINK, 1, false);
			}
		}
	}
}

void CCharacterBotAI::ProcessGuardianNPC()
{
	const float DistanceBetweenSpawnpoint = distance(m_SpawnPoint, m_Pos);
	if(DistanceBetweenSpawnpoint > 800.0f && AI()->GetTarget()->IsEmpty())
	{
		ChangePosition(m_SpawnPoint);
	}
	else
	{
		UpdateTarget(800.0f);
	}
	ResetInput();

	bool MobMove = true;
	if(const auto* pTargetChar = GS()->GetPlayerChar(AI()->GetTarget()->GetCID()))
	{
		m_pBotPlayer->m_TargetPos = pTargetChar->GetPos();
		Fire();
	}
	else
	{
		if(DistanceBetweenSpawnpoint < 256.0f)
		{
			if(Server()->Tick() % Server()->TickSpeed() == 0)
			{
				m_Input.m_TargetY = rand() % 4 - rand() % 8;
			}

			m_Input.m_TargetX = (m_Input.m_Direction * 10 + 1);
			m_Input.m_Direction = 0;
			MobMove = false;
		}
		else
		{
			m_pBotPlayer->m_TargetPos = m_SpawnPoint;
		}
	}

	if(MobMove)
	{
		Move();
	}

	m_PrevPos = m_Pos;
	if(m_Input.m_Direction)
	{
		m_PrevDirection = m_Input.m_Direction;
	}

	SelectEmoteAtRandomInterval(m_EmotionsStyle);
	HandleWeapons();
}

void CCharacterBotAI::ProcessQuestNPC()
{
	// random direction target
	if(Server()->Tick() % Server()->TickSpeed() == 0)
	{
		m_Input.m_TargetY = (rand() % 9) - 4;
	}
	m_Input.m_TargetX = m_Input.m_Direction * 10 + 1;

	// emote actions
	SelectEmoteAtRandomInterval(EMOTE_BLINK);

	// functional
	SearchPlayersForDialogue();
}

void CCharacterBotAI::ProcessMobs()
{
	UpdateTarget(1000.f);
	ResetInput();

	const int MobID = m_pBotPlayer->GetBotMobID();
	const auto* pMobBot = &MobBotInfo::ms_aMobBot[MobID];

	if(const auto* pTargetChar = GS()->GetPlayerChar(AI()->GetTarget()->GetCID()))
	{
		m_pBotPlayer->m_TargetPos = pTargetChar->GetPos();
		Fire();
	}
	else if(Server()->Tick() > m_pBotPlayer->m_LastPosTick)
	{
		m_pBotPlayer->m_TargetPos = {};
	}

	// behavior
	BehaviorTick();

	// has spread for weapons
	if(pMobBot->m_Spread >= 1)
	{
		SelectWeaponAtRandomInterval();
	}

	Move();

	m_PrevPos = m_Pos;
	if(m_Input.m_Direction)
	{
		m_PrevDirection = m_Input.m_Direction;
	}

	SelectEmoteAtRandomInterval(m_EmotionsStyle);
	HandleWeapons();

	// show health for boss mobs
	if(pMobBot->m_Boss)
	{
		for(const auto& ClientID : m_aListDmgPlayers)
		{
			if(GS()->GetPlayer(ClientID, true))
			{
				const int BotID = m_pBotPlayer->GetBotID();
				const int Health = m_pBotPlayer->GetHealth();
				const int StartHealth = m_pBotPlayer->GetStartHealth();
				const float Percent = translate_to_percent((float)StartHealth, (float)Health);

				std::string ProgressBar = mystd::string::progressBar(100, (int)Percent, 10, "\u25B0", "\u25B1");
				GS()->Broadcast(ClientID, BroadcastPriority::GAME_PRIORITY, 100, "{} {}({}/{})",
					DataBotInfo::ms_aDataBot[BotID].m_aNameBot, ProgressBar.c_str(), Health, StartHealth);
			}
		}
	}
}

void CCharacterBotAI::ProcessEidolons()
{
	const auto* pOwner = m_pBotPlayer->GetEidolonOwner();
	if(!pOwner || !pOwner->GetCharacter())
		return;

	const auto* pOwnerChar = pOwner->GetCharacter();
	const float Distance = distance(pOwnerChar->GetPos(), m_Pos);
	if(Distance > 400.f && !AI()->GetTarget()->IsEmpty())
	{
		AI()->GetTarget()->Reset();
	}
	else
	{
		UpdateTarget(400.f);
	}
	ResetInput();

	bool CanMove = true;
	if(const auto* pTargetChar = GS()->GetPlayerChar(AI()->GetTarget()->GetCID()))
	{
		m_pBotPlayer->m_TargetPos = pTargetChar->GetPos();
		Fire();
	}
	else
	{
		if(Distance < 128.0f)
		{
			if(pOwnerChar->m_Core.m_HookState != HOOK_GRABBED)
			{
				ResetHook();
			}

			if(Server()->Tick() % Server()->TickSpeed() == 0)
			{
				m_Input.m_TargetY = rand() % 4 - rand() % 8;
			}

			m_pBotPlayer->m_TargetPos = {};
			m_Input.m_TargetX = (m_Input.m_Direction * 10 + 1);
			m_Input.m_Direction = 0;
			CanMove = false;
		}
		else
		{
			m_pBotPlayer->m_TargetPos = pOwnerChar->GetPos();
		}
	}

	if(CanMove)
	{
		SelectWeaponAtRandomInterval();
		Move();
	}

	m_PrevPos = m_Pos;
	if(m_Input.m_Direction)
	{
		m_PrevDirection = m_Input.m_Direction;
	}

	SelectEmoteAtRandomInterval(m_EmotionsStyle);
	HandleWeapons();
}

// interactive of Quest mobs
void CCharacterBotAI::ProcessQuestMob()
{
	float DistanceBetweenSpawnpoint = distance(m_SpawnPoint, m_Pos);
	if(DistanceBetweenSpawnpoint > 800.0f)
	{
		ChangePosition(m_SpawnPoint);
	}
	else if(DistanceBetweenSpawnpoint > 400.0f && !AI()->GetTarget()->IsEmpty())
	{
		AI()->GetTarget()->Reset();
	}
	else
	{
		UpdateTarget(800.0f);
	}
	ResetInput();


	bool MobMove = true;
	if(const auto* pTargetChar = GS()->GetPlayerChar(AI()->GetTarget()->GetCID()))
	{
		m_pBotPlayer->m_TargetPos = pTargetChar->GetPos();
		Fire();
	}
	else
	{
		if(DistanceBetweenSpawnpoint < 128.0f)
		{
			m_pBotPlayer->m_TargetPos = {};
			m_Input.m_Direction = 0;
			MobMove = false;
		}
		else
		{
			m_pBotPlayer->m_TargetPos = m_SpawnPoint;
		}
	}

	if(MobMove)
	{
		SelectWeaponAtRandomInterval();
		Move();
	}

	m_PrevPos = m_Pos;
	if(m_Input.m_Direction)
	{
		m_PrevDirection = m_Input.m_Direction;
	}

	SelectEmoteAtRandomInterval(m_EmotionsStyle);
	HandleWeapons();
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
			AI()->GetTarget()->SetType(TARGET_TYPE::LOST);
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

void CCharacterBotAI::UpdateTarget(float Radius)
{
	const int Bottype = m_pBotPlayer->GetBotType();
	const auto IsBetterTarget = [](const CPlayer* pCurrentTarget, const CPlayer* pCandidatePlayer)
	{
		if(pCurrentTarget)
		{
			const int CurrentTotalAttHP = pCurrentTarget->GetTotalAttributeValue(AttributeIdentifier::HP);
			const int CandidateTotalAttHP = pCandidatePlayer->GetTotalAttributeValue(AttributeIdentifier::HP);
			return CandidateTotalAttHP > CurrentTotalAttHP;
		}
		return false;
	};

	// conditions for reset target
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
			if(AI()->GetTarget()->SetType(TARGET_TYPE::LOST))
			{
				GS()->SendEmoticon(m_pBotPlayer->GetCID(), EMOTICON_QUESTION);
			}
		}
	}

	// is mob type
	if(Bottype == TYPE_BOT_MOB)
	{
		const auto* pTarget = GS()->GetPlayer(AI()->GetTarget()->GetCID(), false, true);
		const auto* pPlayer = SearchPlayerCondition(Radius, [&](const CPlayer* pCandidatePlayer)
		{
			const bool DamageDisabled = pCandidatePlayer->GetCharacter()->m_Core.m_DamageDisabled;
			const bool AgressionFactor = GS()->IsWorldType(WorldType::Dungeon) || rand() % 30 == 0;

			return !DamageDisabled && ((!pTarget && AgressionFactor) || IsBetterTarget(pTarget, pCandidatePlayer));
		});

		if(pPlayer)
		{
			AI()->GetTarget()->Set(pPlayer->GetCID(), 100);
		}

		return;
	}

	// is quest mob type
	if(Bottype == TYPE_BOT_QUEST_MOB)
	{
		const auto& questBotMobInfo = m_pBotPlayer->GetQuestBotMobInfo();
		const auto* pTarget = GS()->GetPlayer(AI()->GetTarget()->GetCID(), false, true);
		const auto* pPlayer = SearchPlayerCondition(Radius, [&](const CPlayer* pCandidatePlayer)
		{
			const bool DamageDisabled = pCandidatePlayer->GetCharacter()->m_Core.m_DamageDisabled;
			const bool IsActiveForClient = questBotMobInfo.m_ActiveForClient[pCandidatePlayer->GetCID()];

			return !DamageDisabled && IsActiveForClient && (!pTarget || IsBetterTarget(pTarget, pCandidatePlayer));
		});

		if(pPlayer)
		{
			AI()->GetTarget()->Set(pPlayer->GetCID(), 100);
		}

		return;
	}

	// is eidolon type
	if(Bottype == TYPE_BOT_EIDOLON)
	{
		auto* pOwner = m_pBotPlayer->GetEidolonOwner();
		if(!pOwner || !pOwner->GetCharacter())
			return;

		// find from players
		const auto* pTarget = GS()->GetPlayer(AI()->GetTarget()->GetCID(), false, true);
		const auto* pPlayer = SearchPlayerCondition(Radius, [&](const CPlayer* pCandidatePlayer)
		{
			const bool DamageDisabled = pCandidatePlayer->GetCharacter()->m_Core.m_DamageDisabled;
			const bool AllowedPVP = pOwner->GetCharacter()->IsAllowedPVP(pCandidatePlayer->GetCID());

			return !DamageDisabled && AllowedPVP && (!pTarget || IsBetterTarget(pTarget, pCandidatePlayer));
		});

		// try find from bots
		if(!pPlayer)
		{
			pPlayer = SearchPlayerBotCondition(Radius, [&](CPlayerBot* pCandidatePlayer)
			{
				const bool DamageDisabled = pOwner->GetCharacter()->m_Core.m_DamageDisabled || pCandidatePlayer->IsDisabledBotDamage();
				const int MobIDCandidate = pCandidatePlayer->GetBotMobID();
				const int BottypeCandidate = pCandidatePlayer->GetBotType();

				if(BottypeCandidate == TYPE_BOT_MOB)
				{
					return !DamageDisabled;
				}

				if(BottypeCandidate == TYPE_BOT_QUEST_MOB)
				{
					const auto& questBotMobInfo = pCandidatePlayer->GetQuestBotMobInfo();
					const bool IsActiveForClient = questBotMobInfo.m_ActiveForClient[pOwner->GetCID()];

					return !DamageDisabled && IsActiveForClient;
				}

				if(BottypeCandidate == TYPE_BOT_EIDOLON)
				{
					// TODO
					return !DamageDisabled;
				}

				if(BottypeCandidate == TYPE_BOT_NPC)
				{
					const auto* pNpcBot = &NpcBotInfo::ms_aNpcBot[MobIDCandidate];

					if(pNpcBot->m_Function == FUNCTION_NPC_GUARDIAN)
					{
						bool IsCrimeScoreMax = pOwner->Account()->IsCrimeScoreMaxedOut();
						return !DamageDisabled && IsCrimeScoreMax;
					}
				}

				return false;
			});
		}

		if(pPlayer)
		{
			AI()->GetTarget()->Set(pPlayer->GetCID(), 100);
		}
	}
}

CPlayer* CCharacterBotAI::SearchPlayerCondition(float Distance, const std::function<bool(CPlayer*)>& Condition) const
{
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		CPlayer* pCandidatePlayer = GS()->GetPlayer(i);

		if(!pCandidatePlayer || !pCandidatePlayer->GetCharacter())
			continue;

		if(!GS()->IsPlayerInWorld(i))
			continue;

		const vec2& CandidatePos = pCandidatePlayer->GetCharacter()->m_Core.m_Pos;
		if(distance(m_Core.m_Pos, CandidatePos) > Distance)
			continue;

		const bool IntersectedWithInvisibleLine = GS()->Collision()->IntersectLineWithInvisible(CandidatePos, m_Pos, nullptr, nullptr);
		AI()->GetTarget()->UpdateCollised(IntersectedWithInvisibleLine);
		if(GS()->Collision()->IntersectLineWithInvisible(CandidatePos, m_Pos, nullptr, nullptr))
			continue;

		if(!Condition(pCandidatePlayer))
			continue;

		return pCandidatePlayer;
	}

	return nullptr;
}

CPlayerBot* CCharacterBotAI::SearchPlayerBotCondition(float Distance, const std::function<bool(CPlayerBot*)>& Condition) const
{
	for(int i = MAX_PLAYERS; i < MAX_CLIENTS; i++)
	{
		if(m_pBotPlayer->GetCID() == i)
			continue;

		CPlayerBot* pCandidatePlayer = dynamic_cast<CPlayerBot*>(GS()->GetPlayer(i));

		if(!pCandidatePlayer || !pCandidatePlayer->GetCharacter())
			continue;

		if(pCandidatePlayer->IsDisabledBotDamage())
			continue;

		const vec2& CandidatePos = pCandidatePlayer->GetCharacter()->m_Core.m_Pos;
		if(distance(m_Core.m_Pos, CandidatePos) > Distance)
			continue;

		const bool IntersectedWithInvisibleLine = GS()->Collision()->IntersectLineWithInvisible(CandidatePos, m_Pos, nullptr, nullptr);
		AI()->GetTarget()->UpdateCollised(IntersectedWithInvisibleLine);
		if(GS()->Collision()->IntersectLineWithInvisible(CandidatePos, m_Pos, nullptr, nullptr))
			continue;

		if(!Condition(pCandidatePlayer))
			continue;

		return pCandidatePlayer;
	}

	return nullptr;
}

bool CCharacterBotAI::SearchPlayersForDialogue()
{
	bool PlayerFinding = false;
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		// check for visible client
		if(!m_pBotPlayer->IsActive() || !m_pBotPlayer->IsActiveForClient(i))
			continue;

		// check active player near bot
		CPlayer* pPlayer = GS()->GetPlayer(i, true, true);
		if(!pPlayer || distance(pPlayer->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos) > 128.0f)
			continue;

		// check walls and closed lines
		if(GS()->Collision()->IntersectLineWithInvisible(pPlayer->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos, nullptr, nullptr))
			continue;

		pPlayer->GetCharacter()->SetSafeFlags();
		m_Input.m_TargetX = static_cast<int>(pPlayer->GetCharacter()->m_Core.m_Pos.x - m_Pos.x);
		m_Input.m_TargetY = static_cast<int>(pPlayer->GetCharacter()->m_Core.m_Pos.y - m_Pos.y);
		m_Input.m_Direction = 0;

		GS()->Broadcast(i, BroadcastPriority::GAME_INFORMATION, 10, "Begin dialogue: \"hammer hit\"");
		PlayerFinding = true;
	}

	return PlayerFinding;
}

// - - - - - - - - - - - - - - - - - - - - - Npc functions
bool CCharacterBotAI::BaseFunctionNPC()
{
	return SearchPlayersForDialogue();
}

bool CCharacterBotAI::FunctionNurseNPC()
{
	char aBuf[16];
	bool PlayerFinding = false;
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		// check active player near bot
		CPlayer* pPlayer = GS()->GetPlayer(i, true, true);
		if(!pPlayer || distance(pPlayer->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos) >= 256.0f)
			continue;

		// check walls and closed lines
		if(GS()->Collision()->IntersectLine(pPlayer->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos, nullptr, nullptr))
			continue;

		// disable collision and hook with players
		float Distance = distance(pPlayer->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos);
		if(Distance < 128.0f)
		{
			pPlayer->GetCharacter()->SetSafeFlags();
			if(Distance < 64.0f)
				m_Input.m_Direction = 0;
		}

		// skip full health
		if(pPlayer->GetHealth() >= pPlayer->GetStartHealth())
			continue;

		// set target to player
		m_Input.m_TargetX = static_cast<int>(pPlayer->GetCharacter()->m_Core.m_Pos.x - m_Pos.x);
		m_Input.m_TargetY = static_cast<int>(pPlayer->GetCharacter()->m_Core.m_Pos.y - m_Pos.y);
		m_LatestInput.m_TargetX = m_Input.m_TargetX;
		m_LatestInput.m_TargetY = m_Input.m_TargetX;
		PlayerFinding = true;

		// health every sec
		if(Server()->Tick() % Server()->TickSpeed() == 0)
		{
			// increase health for player
			int Health = maximum(pPlayer->GetStartHealth() / 20, 1);
			new CHeartHealer(&GS()->m_World, m_Pos, pPlayer, Health, pPlayer->GetCharacter()->m_Core.m_Vel);
			m_Input.m_Direction = 0;

			// information
			vec2 DrawPosition = vec2(pPlayer->GetCharacter()->m_Core.m_Pos.x, pPlayer->GetCharacter()->m_Core.m_Pos.y - 90.0f);
			str_format(aBuf, sizeof(aBuf), "%dHP", Health);
			GS()->EntityManager()->Text(DrawPosition, 40, aBuf);
		}
	}
	return PlayerFinding;
}