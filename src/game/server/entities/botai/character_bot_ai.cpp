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
#include <game/server/mmocore/Components/Quests/QuestManager.h>

#include "game/server/mmocore/PathFinder.h"

#include "nurse_heart.h"

MACRO_ALLOC_POOL_ID_IMPL(CCharacterBotAI, MAX_CLIENTS* ENGINE_MAX_WORLDS + MAX_CLIENTS)

CCharacterBotAI::CCharacterBotAI(CGameWorld* pWorld) : CCharacter(pWorld)
{
	m_pAI = new CAIController(this);
}
CCharacterBotAI::~CCharacterBotAI()
{
	delete m_pAI;
	m_pAI = nullptr;
}

int CCharacterBotAI::GetSnapFullID() const { return m_pBotPlayer->GetCID() * SNAPBOTS; }

bool CCharacterBotAI::Spawn(class CPlayer* pPlayer, vec2 Pos)
{
	m_pBotPlayer = static_cast<CPlayerBot*>(pPlayer);
	if(!CCharacter::Spawn(m_pBotPlayer, Pos))
		return false;

	// bot types init
	OnSpawnInitBotTypes();

	return true;
}

void CCharacterBotAI::OnSpawnInitBotTypes()
{
	// mob information
	const int ClientID = m_pBotPlayer->GetCID();
	const int MobID = m_pBotPlayer->GetBotMobID();
	switch(m_pBotPlayer->GetBotType())
	{
		//
		// Check if the type of bot is TYPE_BOT_MOB
		case TYPE_BOT_MOB:
		{
			// Get a pointer to the MobBotInfo object for the given MobID
			MobBotInfo* pMobBot = &MobBotInfo::ms_aMobBot[MobID];

			// Check if the mob bot is a boss
			if(pMobBot->m_Boss)
			{
				// Create 3 snap projectiles with type POWERUP_HEALTH and initial snap ID of 1
				CreateSnapProj(GetSnapFullID(), 1, POWERUP_HEALTH, true, false);

				// Create 3 snap projectiles with type WEAPON_HAMMER and initial snap ID of 1
				CreateSnapProj(GetSnapFullID(), 1, WEAPON_HAMMER, false, true);

				// Check if the game state is not a dungeon
				if(!GS()->IsDungeon())
				{
					// Display a chat message in the world with the mob bot's name
					GS()->ChatWorldID(pMobBot->m_WorldID, nullptr, "In your zone emerging {STR}!", pMobBot->GetName());
				}
			}
		} break;
		//
		// Case for bot type TYPE_BOT_QUEST
		case TYPE_BOT_QUEST:
		{
			// Get the quest bot info for the given MobID
			QuestBotInfo* pQuestBot = &QuestBotInfo::ms_aQuestBot[MobID];

			// Check if the quest bot has an action and create a laser orbite
			if(pQuestBot->m_HasAction)
			{
				GS()->CreateLaserOrbite(ClientID, 5, EntLaserOrbiteType::MOVE_RIGHT, 0.15f, 64.f, LASERTYPE_SHOTGUN);
			}
		} break;
		//
		// Case for bot type TYPE_BOT_NPC
		case TYPE_BOT_NPC:
		{
			// Get the function of the NPC bot
			const int Function = NpcBotInfo::ms_aNpcBot[MobID].m_Function;

			// Check the function of the NPC bot and perform the corresponding action
			if(Function == FUNCTION_NPC_GIVE_QUEST)
			{
				// Create a snapshot projectile with the given SnapFullID, type 3 (POWERUP_ARMOR), and without explosion and collision
				CreateSnapProj(GetSnapFullID(), 3, POWERUP_ARMOR, false, false);
			}
			else if(Function == FUNCTION_NPC_NURSE)
			{
				// Create a new instance of CNurseHeart with the GameWorld and ClientID parameters
				new CNurseHeart(GameWorld(), ClientID);
			}
			else if(Function == FUNCTION_NPC_GUARDIAN)
			{
				// Create a snapshot projectile with the given SnapFullID, type 2 (POWERUP_NINJA), and without explosion and collision
				CreateSnapProj(GetSnapFullID(), 2, POWERUP_NINJA, false, false);
			}
		} break;
		//
		// Case for bot type TYPE_BOT_EIDOLON
		case TYPE_BOT_EIDOLON:
		{
			// Set the m_Solo flag to true
			m_Core.m_Solo = true;

			// Get the CID of the owner of the bot
			const int OwnerCID = m_pBotPlayer->GetEidolonOwner()->GetCID();

			// Create a new CEidolon object with the specified parameters
			new CEidolon(&GS()->m_World, m_Core.m_Pos, 0, ClientID, OwnerCID);
		} break;
		//
		// Case for bot type TYPE_BOT_QUEST_MOB
		{

		} break;
		default: break;
	}
}

void CCharacterBotAI::GiveRandomEffects(int ClientID)
{
	if(m_pBotPlayer->GetBotType() == TYPE_BOT_MOB)
	{
		if(CPlayer* pPlayer = GS()->GetPlayer(ClientID); pPlayer && ClientID != m_pBotPlayer->GetCID())
		{
			const int MobID = m_pBotPlayer->GetBotMobID();
			if(const CMobBuffDebuff* pBuff = MobBotInfo::ms_aMobBot[MobID].GetRandomEffect())
				pPlayer->GiveEffect(pBuff->getEffect(), pBuff->getTime(), pBuff->getChance());
		}
	}
}

bool CCharacterBotAI::TakeDamage(vec2 Force, int Dmg, int From, int Weapon)
{
	CPlayer* pFrom = GS()->GetPlayer(From, false, true);
	if(!pFrom || !m_pBotPlayer->IsActive())
		return false;

	// allow damage for mob's
	if(m_pBotPlayer->GetBotType() == TYPE_BOT_MOB || m_pBotPlayer->GetBotType() == TYPE_BOT_QUEST_MOB ||
		(m_pBotPlayer->GetBotType() == TYPE_BOT_NPC && NpcBotInfo::ms_aNpcBot[m_pBotPlayer->GetBotMobID()].m_Function == FUNCTION_NPC_GUARDIAN))
	{
		// Check for collision between the current character and another character
		if(GS()->Collision()->IntersectLineColFlag(m_Core.m_Pos, pFrom->GetCharacter()->m_Core.m_Pos, nullptr, nullptr, CCollision::COLFLAG_DISALLOW_MOVE))
			return false;

		// Function to handle character taking damage
		CCharacter::TakeDamage(Force, Dmg, From, Weapon);

		// Check if the type of pFrom is TYPE_BOT_EIDOLON
		if(pFrom->GetBotType() == TYPE_BOT_EIDOLON)
		{
			// Cast pFrom to CPlayerBot* and get the Eidolon Owner's CID
			From = dynamic_cast<CPlayerBot*>(pFrom)->GetEidolonOwner()->GetCID();
		}

		// Check if the bot player is of type NPC and the player interacting with the bot is not a bot itself
		if(m_pBotPlayer->GetBotType() == TYPE_BOT_NPC && !pFrom->IsBot())
		{
			SetEmote(EMOTE_ANGRY, 1, true);
			pFrom->IncreaseRelations(1 + random_int() % 8);
			GS()->CreateDropBonuses(m_Core.m_Pos, 1, 1, (1 + random_int() % 2), Force);
		}

		// Check if the bot type of pFrom is TYPE_BOT_MOB and if the target of AI is empty
		if(pFrom->GetBotType() == TYPE_BOT_MOB && AI()->GetTarget()->IsEmpty())
		{
			// Set the target of AI to the "From" bot with an aggression level of 200 units
			AI()->GetTarget()->Set(From, 200);
		}

		// Check if the element "From" is already present in the vector m_aListDmgPlayers
		if(!std::any_of(m_aListDmgPlayers.begin(), m_aListDmgPlayers.end(), [From](int ClientID){ return ClientID == From; }))
		{
			// If the element is not present, append it to the end of the vector
			m_aListDmgPlayers.push_back(From);
		}

		// verify death
		if(m_Health <= 0)
		{
			// Reward players with damage
			// Check if the weapon is not self or world
			if(Weapon != WEAPON_SELF && Weapon != WEAPON_WORLD)
			{
				for(const auto& ClientID : m_aListDmgPlayers)
				{
					// Check if the player object exists and is in the same world as the bot player
					CPlayer* pPlayer = GS()->GetPlayer(ClientID, true, true);
					if(!pPlayer || !GS()->IsPlayerEqualWorld(ClientID, m_pBotPlayer->GetPlayerWorldID()) || distance(pPlayer->m_ViewPos, m_Core.m_Pos) > 1000.0f)
						continue;

					// Reward the player with damage applied
					RewardPlayer(pPlayer, Force);
				}
			}

			// Clear the list of damaged players
			m_aListDmgPlayers.clear();

			// Reset the target of the AI
			AI()->GetTarget()->Reset();

			// Die
			Die(From, Weapon);

			// Return false
			return false;
		}

		return true;
	}

	return false;
}

void CCharacterBotAI::Die(int Killer, int Weapon)
{
	if(Killer < 0 || Killer > MAX_CLIENTS || !GS()->m_apPlayers[Killer])
		return;

	// only for mob's
	int BotType = m_pBotPlayer->GetBotType();
	int MobID = m_pBotPlayer->GetBotMobID();
	if(BotType == TYPE_BOT_MOB || BotType == TYPE_BOT_QUEST_MOB || (BotType == TYPE_BOT_NPC && NpcBotInfo::ms_aNpcBot[MobID].m_Function == FUNCTION_NPC_GUARDIAN))
	{
		m_Alive = false;

		// Check if the bot player's type is TYPE_BOT_NPC and increase relations
		if(m_pBotPlayer->GetBotType() == TYPE_BOT_NPC)
			GS()->m_apPlayers[Killer]->IncreaseRelations(1 + (random_int() % 5));

		// a nice sound
		int ClientID = m_pBotPlayer->GetCID();
		CPlayer* pPlayerKiller = GS()->m_apPlayers[Killer];
		GS()->m_pController->OnCharacterDeath(this, pPlayerKiller, Weapon);
		GS()->CreateSound(m_Pos, SOUND_PLAYER_DIE);

		// send the kill message
		CNetMsg_Sv_KillMsg Msg;
		Msg.m_Killer = Killer;
		Msg.m_Victim = ClientID;
		Msg.m_Weapon = Weapon;
		Msg.m_ModeSpecial = 0;
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1, -1, pPlayerKiller->GetPlayerWorldID());

		// respawn
		m_pBotPlayer->ResetRespawnTick();
		m_pBotPlayer->m_aPlayerTick[TickState::Die] = Server()->Tick() / 2;
		m_pBotPlayer->m_Spawned = true;
		GS()->CreateDeath(m_Pos, ClientID);

		GS()->m_World.RemoveEntity(this);
		GS()->m_World.m_Core.m_apCharacters[ClientID] = nullptr;
	}
}

void CCharacterBotAI::RewardPlayer(CPlayer* pPlayer, vec2 Force) const
{
	const int ClientID = pPlayer->GetCID();
	const int BotID = m_pBotPlayer->GetBotID();
	const int SubID = m_pBotPlayer->GetBotMobID();

	// Check if the bot type is TYPE_BOT_QUEST_MOB and the killer is active for the client
	if(m_pBotPlayer->GetBotType() == TYPE_BOT_QUEST_MOB)
	{
		if(m_pBotPlayer->GetQuestBotMobInfo().m_ActiveForClient[ClientID])
		{
			// Set the completion status for the killer to true
			m_pBotPlayer->GetQuestBotMobInfo().m_CompleteClient[ClientID] = true;
		}
	}

	// Check if the bot is a mob type
	if(m_pBotPlayer->GetBotType() == TYPE_BOT_MOB)
	{
		// Append defeat progress for the quest mob
		GS()->Mmo()->Quest()->AppendDefeatProgress(pPlayer, BotID);
	}

	// reduce afk farming
	if(pPlayer->IsAfk())
	{
		GS()->Broadcast(ClientID, BroadcastPriority::GAME_PRIORITY, 100, "You get reduced rewards, due to farming mobs afk.");
		pPlayer->AddMoney(1);
		GS()->CreateParticleExperience(m_Core.m_Pos, ClientID, 1, Force);
		return;
	}

	// grinding gold
	const int Gold = max(MobBotInfo::ms_aMobBot[SubID].m_Level / g_Config.m_SvStrongGold, 1);
	pPlayer->AddMoney(Gold);

	// grinding experience
	const int ExperienceMob = max(1, (int)computeExperience(MobBotInfo::ms_aMobBot[SubID].m_Level) / g_Config.m_SvKillmobsIncreaseLevel);
	const int ExperienceWithMultiplier = GS()->GetExperienceMultiplier(ExperienceMob);
	GS()->CreateParticleExperience(m_Core.m_Pos, ClientID, ExperienceWithMultiplier, Force);

	// drop experience
	const int ExperienceDrop = max(ExperienceWithMultiplier / 2, 1);
	GS()->CreateDropBonuses(m_Core.m_Pos, 1, ExperienceDrop, (1 + random_int() % 2), Force);

	// drop item's
	const float ActiveLuckyDrop = clamp((float)pPlayer->GetAttributeSize(AttributeIdentifier::LuckyDropItem) / 100.0f, 0.01f, 10.0f);
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
	if(Server()->Tick() % (Server()->TickSpeed() * (1 + random_int() % 3)) == 0)
	{
		int RandomWeapon = clamp(random_int() % 4, (int)WEAPON_HAMMER, (int)WEAPON_LASER);
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
	// check active this bot
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

	// update door state
	if(!m_DoorHit)
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

	// reset door
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
		CPlayer* pOwner = m_pBotPlayer->GetEidolonOwner();
		DDNetFlag(CHARACTERFLAG_SOLO, !(pOwner && pOwner->GetCID() == SnappingClient))
			DDNetFlag(CHARACTERFLAG_COLLISION_DISABLED, true)
	}
	else if(m_pBotPlayer->GetBotType() == TYPE_BOT_QUEST_MOB)
	{
		bool IsActiveForSnappingClient = m_pBotPlayer->GetQuestBotMobInfo().m_ActiveForClient[SnappingClient];
		DDNetFlag(CHARACTERFLAG_SOLO, !IsActiveForSnappingClient)
			DDNetFlag(CHARACTERFLAG_COLLISION_DISABLED, !IsActiveForSnappingClient)
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
	// bot-like behavior
	switch(m_pBotPlayer->GetBotType())
	{
		// default bot mob
		case TYPE_BOT_MOB:
		{
			AI()->GetTarget()->Tick();
			EngineMobs();
		} break;
		// bot quest mob
		case TYPE_BOT_QUEST_MOB:
		{
			AI()->GetTarget()->Tick();
			EngineQuestMob();
		} break;
		// player eidolon
		case TYPE_BOT_EIDOLON:
		{
			AI()->GetTarget()->Tick();
			EngineEidolons();
		} break;
		// quest bot
		case TYPE_BOT_QUEST:
		{
			SetSafe();
			EngineQuestNPC();
		} break;
		// npc bot
		case TYPE_BOT_NPC:
		{
			const int MobID = m_pBotPlayer->GetBotMobID();
			NpcBotInfo& pNpcBot = NpcBotInfo::ms_aNpcBot[MobID];

			if(pNpcBot.m_Function == FUNCTION_NPC_GUARDIAN)
			{
				AI()->GetTarget()->Tick();
				EngineNPC();
			}
			else
			{
				const float tx = m_Pos.x + m_Input.m_Direction * 45.0f;
				const float collisionWidth = GS()->Collision()->GetWidth() * 32.0f;

				if(tx < 0)
				{
					m_Input.m_Direction = 1;
				}
				else if(tx >= collisionWidth)
				{
					m_Input.m_Direction = -1;
				}

				m_LatestPrevInput = m_LatestInput;
				m_LatestInput = m_Input;

				SetSafe();
				EngineNPC();
			}
		} break;
		// unknown bot
		default: break;
	}

	// handle
	HandleEvent();
}

// interactive of NPC
void CCharacterBotAI::EngineNPC()
{
	// Get variables
	const int MobID = m_pBotPlayer->GetBotMobID();
	NpcBotInfo* pNpcBot = &NpcBotInfo::ms_aNpcBot[MobID];
	bool PlayerFinding = false;
	int NpcFunction = pNpcBot->m_Function;

	// check if the bot's function is set to NPC_GUARDIAN
	if(NpcFunction == FUNCTION_NPC_GUARDIAN)
	{
		// call the FunctionGuardian to execute the corresponding actions
		FunctionGuardian();
		return;
	}
	// check if the bot's function is set to NPC_NURSE
	else if(NpcFunction == FUNCTION_NPC_NURSE)
	{
		// call FunctionNurseNPC to find players
		PlayerFinding = FunctionNurseNPC();
	}
	else
	{
		// call BaseFunctionNPC to find players
		PlayerFinding = BaseFunctionNPC();
	}

	// Emote actions
	EmotesAction(pNpcBot->m_Emote);

	// Direction eyes
	// Every tick (once per second), change the vertical target of the NPC's eyes randomly between -4 and 4.
	if(Server()->Tick() % Server()->TickSpeed() == 0)
		m_Input.m_TargetY = random_int() % 8 - random_int() % 4;

	// Set the horizontal target of the NPC's eyes to the direction the NPC is facing multiplied by 10, plus 1.
	m_Input.m_TargetX = (m_Input.m_Direction * 10 + 1);

	// Walking for NPC
	// If the NPC is not currently searching for a player and is not static (not stationary), then randomly change direction every 50 ticks.
	if(!PlayerFinding && !pNpcBot->m_Static && random_int() % 50 == 0)
	{
		m_Input.m_Direction = -1 + (random_int() % 3);
	}
}

// interactive of Quest bots
void CCharacterBotAI::EngineQuestNPC()
{
	if(Server()->Tick() % Server()->TickSpeed() == 0)
		m_Input.m_TargetY = random_int() % 4 - random_int() % 8;
	m_Input.m_TargetX = (m_Input.m_Direction * 10 + 1);

	// emote actions
	EmotesAction(EMOTE_BLINK);

	// functional
	SearchPlayersToTalk();
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
		if(MobBotInfo::ms_aMobBot[MobID].IsIncludedBehavior("Slower"))
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
		if(AI()->GetTarget()->IsEmpty() && pMobBot->IsIncludedBehavior("Sleepy"))
		{
			if(Server()->Tick() % (Server()->TickSpeed() / 2) == 0)
			{
				GS()->SendEmoticon(m_pBotPlayer->GetCID(), EMOTICON_ZZZ);
				SetEmote(EMOTE_BLINK, 1, false);
			}
		}
	}
}

// interactive of Mobs
void CCharacterBotAI::EngineMobs()
{
	const int MobID = m_pBotPlayer->GetBotMobID();
	MobBotInfo* pMobBot = &MobBotInfo::ms_aMobBot[MobID];

	// reset input
	ResetInput();

	// search hardress player
	CPlayer* pPlayer = SearchTankPlayer(1000.0f);
	if(pPlayer && pPlayer->GetCharacter())
	{
		m_pBotPlayer->m_TargetPos = pPlayer->GetCharacter()->GetPos();
		Fire();
	}
	else if(Server()->Tick() > m_pBotPlayer->m_LastPosTick)
		m_pBotPlayer->m_TargetPos = vec2(0, 0);

	// behavior
	BehaviorTick();

	// show health for boss mobs
	if(pMobBot->m_Boss)
	{
		for(const auto& ClientID : m_aListDmgPlayers)
		{
			if(GS()->GetPlayer(ClientID, true))
			{
				char aBufProgress[32];
				const int BotID = m_pBotPlayer->GetBotID();
				const int Health = m_pBotPlayer->GetHealth();
				const int StartHealth = m_pBotPlayer->GetStartHealth();
				const float Percent = translate_to_percent((float)StartHealth, (float)Health);

				str_format_progress_bar(aBufProgress, sizeof(aBufProgress), 100, (int)Percent, 10, ':', ' ');
				GS()->Broadcast(ClientID, BroadcastPriority::GAME_PRIORITY, 100, "{STR} {STR}({INT}/{INT})",
					DataBotInfo::ms_aDataBot[BotID].m_aNameBot, aBufProgress, Health, StartHealth);
			}
		}
	}

	// bot with weapons since it has spread.
	if(pMobBot->m_Spread >= 1)
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

	// reset input
	ResetInput();

	if(const CPlayer* pOwner = m_pBotPlayer->GetEidolonOwner(); pOwner && pOwner->GetCharacter())
	{
		// search target
		if(const CPlayerBot* pEmenyPlayer = SearchMob(400.0f); pEmenyPlayer && pEmenyPlayer->GetCharacter())
		{
			AI()->GetTarget()->Set(pEmenyPlayer->GetCID(), 100);
			m_pBotPlayer->m_TargetPos = pEmenyPlayer->GetCharacter()->GetPos();
		}

		// check player distance with active target
		float Distance = distance(pOwner->m_ViewPos, m_Pos);
		if(Distance > 400.0f && !AI()->GetTarget()->IsEmpty())
		{
			AI()->GetTarget()->Reset();
		}

		// side empty target
		if(AI()->GetTarget()->IsEmpty())
		{
			if(Distance < 128.0f)
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

			// always with empty target / target pos it's owner
			m_pBotPlayer->m_TargetPos = pOwner->GetCharacter()->GetPos();
		}
	}

	// bot with weapons since it has spread.
	if(MobMove)
	{
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

// interactive of Quest mobs
void CCharacterBotAI::EngineQuestMob()
{
	bool MobMove = true;

	// reset input
	ResetInput();

	// search target
	CPlayer* pPlayer = SearchTankPlayer(1000.0f);
	if(pPlayer && pPlayer->GetCharacter())
	{
		AI()->GetTarget()->Set(pPlayer->GetCID(), 100);
		m_pBotPlayer->m_TargetPos = pPlayer->GetCharacter()->GetPos();
	}

	// check player distance with active target
	float Distance = distance(m_SpawnPoint, m_Pos);
	if(Distance > 800.0f)
	{
		ChangePosition(m_SpawnPoint);
	}
	else if(Distance > 400.0f && !AI()->GetTarget()->IsEmpty())
	{
		AI()->GetTarget()->Reset();
	}

	// side empty target
	if(AI()->GetTarget()->IsEmpty())
	{
		if(Distance < 128.0f)
		{
			if(Server()->Tick() % Server()->TickSpeed() == 0)
				m_Input.m_TargetY = random_int() % 4 - random_int() % 8;

			m_pBotPlayer->m_TargetPos = vec2(0, 0);
			m_Input.m_TargetX = (m_Input.m_Direction * 10 + 1);
			m_Input.m_Direction = 0;
			MobMove = false;
		}

		// always with empty target / target pos it's owner
		m_pBotPlayer->m_TargetPos = m_SpawnPoint;
	}

	// bot with weapons since it has spread.
	if(MobMove)
	{
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
	// try get path finder updated data
	GS()->PathFinder()->SyncHandler()->TryMarkAndUpdatePreparedData(&m_pBotPlayer->m_PathFinderData, &m_pBotPlayer->m_TargetPos, &m_pBotPlayer->m_OldTargetPos);

	CPathFinderPrepared::CData& pData = m_pBotPlayer->m_PathFinderData.Get();

	// update aim
	SetAim(m_pBotPlayer->m_TargetPos - m_Pos);

	// parse path finder data
	int Index = -1;
	int ActiveWayPoints = 0;
	for(int i = 0; i < (int)pData.m_Points.size() && i < 30 && !GS()->Collision()->IntersectLineWithInvisible(pData.m_Points[i], m_Pos, nullptr, nullptr); i++)
	{
		Index = i;
		ActiveWayPoints = i;
	}

	vec2 WayDir = vec2(0, 0);
	if(Index > -1)
		WayDir = normalize(pData.m_Points[Index] - GetPos());

	if(WayDir.x < 0 && ActiveWayPoints > 3)
		m_Input.m_Direction = -1;
	else if(WayDir.x > 0 && ActiveWayPoints > 3)
		m_Input.m_Direction = 1;
	else
		m_Input.m_Direction = m_PrevDirection;

	// check dissalow move
	if(m_pBotPlayer->GetBotType() != TYPE_BOT_EIDOLON && m_pBotPlayer->GetBotType() != TYPE_BOT_QUEST_MOB)
	{
		if(IsCollisionFlag(CCollision::COLFLAG_DISALLOW_MOVE))
		{
			AI()->GetTarget()->SetType(TARGET_TYPE::LOST);
			m_Input.m_Direction = -m_Input.m_Direction;
			m_DoorHit = true;
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
	CCharacter* pChar = GameWorld()->IntersectCharacter(GetPos(), GetPos() + vec2(m_Input.m_Direction, 0) * 128, 16.0f, IntersectPos, (CCharacter*)this);
	if(pChar && (pChar->GetPos().x < GetPos().x || !pChar->GetPlayer()->IsBot()))
		m_Input.m_Jump = 1;

	if(ActiveWayPoints > 2 && !m_Input.m_Hook && (WayDir.x != 0 || WayDir.y != 0))
	{
		if(m_Core.m_HookState == HOOK_GRABBED && m_Core.m_HookedPlayer == -1)
		{
			vec2 HookVel = normalize(m_Core.m_HookPos - GetPos()) * GS()->Tuning()->m_HookDragAccel;
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
					HookVel.y *= 0.3f;
					HookVel.x *= (HookVel.x < 0 && m_Input.m_Direction < 0) || (HookVel.x > 0 && m_Input.m_Direction > 0) ? 0.95f : 0.75f;
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

// searching for a player among people
CPlayer* CCharacterBotAI::SearchPlayer(float Distance) const
{
	// Loop through each player in the game
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		// Skip the iteration if the player or their character is not available
		if(!GS()->m_apPlayers[i] || !GS()->m_apPlayers[i]->GetCharacter())
			continue;

		// Skip the iteration if the bot is a quest mob type and the player is not active for the bot
		if(m_pBotPlayer->GetBotType() == TYPE_BOT_QUEST_MOB && !m_pBotPlayer->GetQuestBotMobInfo().m_ActiveForClient[i])
			continue;

		// Check if the bot is a npc type
		if(m_pBotPlayer->GetBotType() == TYPE_BOT_NPC)
		{
			// Check if the bot is a guardian NPC and the player is not active for the bot
			if(NpcBotInfo::ms_aNpcBot[m_pBotPlayer->GetBotMobID()].m_Function == FUNCTION_NPC_GUARDIAN && !GS()->m_apPlayers[i]->Acc().IsRelationshipsDeterioratedToMax())
			{
				continue;
			}
		}

		// Skip the iteration if the distance between the bot and the player's character is greater than the specified distance
		if(distance(m_Core.m_Pos, GS()->m_apPlayers[i]->GetCharacter()->m_Core.m_Pos) > Distance)
			continue;

		// Skip the iteration if there is a collision between the bot and the player's character
		if(GS()->Collision()->IntersectLineWithInvisible(GS()->m_apPlayers[i]->GetCharacter()->m_Core.m_Pos, m_Pos, nullptr, nullptr))
			continue;

		// Skip the iteration if the player is not in the same world as the bot
		if(!GS()->IsPlayerEqualWorld(i))
			continue;

		// Return the player if all conditions are met
		return GS()->m_apPlayers[i];
	}

	// Return null if no player is found that meets all the conditions
	return nullptr;
}

// finding a player among people who have the highest fury
CPlayer* CCharacterBotAI::SearchTankPlayer(float Distance)
{
	if(AI()->GetTarget()->IsEmpty() && (GS()->IsDungeon() || random_int() % 30 == 0))
	{
		CPlayer* pPlayer = SearchPlayer(Distance);
		if(pPlayer && pPlayer->GetCharacter())
		{
			AI()->GetTarget()->Set(pPlayer->GetCID(), 100);
		}

		return pPlayer;
	}

	// strong warnings for reset target (position and world zone)
	CPlayer* pPlayer = GS()->GetPlayer(AI()->GetTarget()->GetCID(), true, true);
	if(!AI()->GetTarget()->IsEmpty() && (!pPlayer || (pPlayer && (distance(pPlayer->GetCharacter()->GetPos(), m_Pos) > 800.0f || !GS()->IsPlayerEqualWorld(AI()->GetTarget()->GetCID())))))
		AI()->GetTarget()->Reset();

	// non-hostile mobs
	if(AI()->GetTarget()->IsEmpty() || !pPlayer)
		return nullptr;

	// throw off the lifetime of a target
	AI()->GetTarget()->UpdateCollised(GS()->Collision()->IntersectLineWithInvisible(pPlayer->GetCharacter()->GetPos(), m_Pos, nullptr, nullptr));

	// looking for a stronger
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		// Skip the iteration if the bot is a quest mob type and the player is not active for the bot
		if(m_pBotPlayer->GetBotType() == TYPE_BOT_QUEST_MOB && !m_pBotPlayer->GetQuestBotMobInfo().m_ActiveForClient[i])
			continue;

		// check the distance of the player
		CPlayer* pFinderHard = GS()->GetPlayer(i, true, true);
		if(AI()->GetTarget()->GetCID() == i || !pFinderHard || distance(pFinderHard->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos) > 800.0f)
			continue;

		// check if the player is tastier for the bot
		const bool FinderCollised = GS()->Collision()->IntersectLineWithInvisible(pFinderHard->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos, nullptr, nullptr);
		if(!FinderCollised && pFinderHard->GetAttributeSize(AttributeIdentifier::HP) > pPlayer->GetAttributeSize(AttributeIdentifier::HP))
		{
			AI()->GetTarget()->Set(i, 100);
			pPlayer = pFinderHard;
		}
	}

	return pPlayer;
}

// search mob
CPlayerBot* CCharacterBotAI::SearchMob(float Distance) const
{
	CPlayerBot* pBotPlayer = nullptr;

	// Looking for a stronger
	for(int i = MAX_PLAYERS; i < MAX_CLIENTS; i++)
	{
		// Check active player bot
		CPlayerBot* pSearchBotPlayer = dynamic_cast<CPlayerBot*>(GS()->m_apPlayers[i]);
		if(!pSearchBotPlayer || !pSearchBotPlayer->GetCharacter())
			continue;

		if(pSearchBotPlayer->GetBotType() == TYPE_BOT_QUEST_MOB || pSearchBotPlayer->GetBotType() == TYPE_BOT_MOB)
		{
			// Check if the bot player is an eidolon owner
			if(m_pBotPlayer->GetEidolonOwner())
			{
				// Check if the bot player is of type "TYPE_BOT_QUEST_MOB" and if the quest bot mob is active for the client at index i
				if(pSearchBotPlayer->GetBotType() == TYPE_BOT_QUEST_MOB && !pSearchBotPlayer->GetQuestBotMobInfo().m_ActiveForClient[m_pBotPlayer->GetEidolonOwner()->GetCID()])
					continue;
			}

			// Check distance
			if(distance(pSearchBotPlayer->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos) > Distance)
				continue;

			// Check walls and closed lines
			if(GS()->Collision()->IntersectLineWithInvisible(pSearchBotPlayer->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos, nullptr, nullptr))
				continue;

			pBotPlayer = dynamic_cast<CPlayerBot*>(GS()->m_apPlayers[i]);
			break;
		}
	}

	return pBotPlayer;
}

bool CCharacterBotAI::SearchPlayersToTalk()
{
	bool PlayerFinding = false;
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		// check for visible client
		if(!m_pBotPlayer->IsVisibleForClient(i))
			continue;

		// check active player near bot
		CPlayer* pPlayer = GS()->GetPlayer(i, true, true);
		if(!pPlayer || distance(pPlayer->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos) > 128.0f)
			continue;

		// check walls and closed lines
		if(GS()->Collision()->IntersectLineWithInvisible(pPlayer->GetCharacter()->m_Core.m_Pos, m_Core.m_Pos, nullptr, nullptr))
			continue;

		pPlayer->GetCharacter()->m_SafeAreaForTick = true;
		m_Input.m_TargetX = static_cast<int>(pPlayer->GetCharacter()->m_Core.m_Pos.x - m_Pos.x);
		m_Input.m_TargetY = static_cast<int>(pPlayer->GetCharacter()->m_Core.m_Pos.y - m_Pos.y);
		m_Input.m_Direction = 0;

		GS()->Broadcast(i, BroadcastPriority::GAME_INFORMATION, 10, "Begin dialogue: \"hammer hit\"");
		PlayerFinding = true;
	}

	return PlayerFinding;
}

void CCharacterBotAI::EmotesAction(int EmotionStyle)
{
	if(EmotionStyle < EMOTE_PAIN || EmotionStyle > EMOTE_BLINK)
		return;

	if(Server()->Tick() % (Server()->TickSpeed() * 3 + random_int() % 10) == 0)
	{
		switch(EmotionStyle)
		{
			case EMOTE_BLINK:
			SetEmote(EMOTE_BLINK, 1 + random_int() % 2, true);
			break;
			case EMOTE_HAPPY:
			SetEmote(EMOTE_HAPPY, 1 + random_int() % 2, true);
			break;
			case EMOTE_ANGRY:
			SetEmote(EMOTE_ANGRY, 1 + random_int() % 2, true);
			break;
			case EMOTE_PAIN:
			SetEmote(EMOTE_PAIN, 1 + random_int() % 2, true);
			break;
			default: break;
		}
	}
}

// - - - - - - - - - - - - - - - - - - - - - Npc functions
bool CCharacterBotAI::BaseFunctionNPC()
{
	return SearchPlayersToTalk();
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
			pPlayer->GetCharacter()->m_SafeAreaForTick = true;
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
			int Health = max(pPlayer->GetStartHealth() / 20, 1);
			new CHearth(&GS()->m_World, m_Pos, pPlayer, Health, pPlayer->GetCharacter()->m_Core.m_Vel);
			m_Input.m_Direction = 0;

			// information
			vec2 DrawPosition = vec2(pPlayer->GetCharacter()->m_Core.m_Pos.x, pPlayer->GetCharacter()->m_Core.m_Pos.y - 90.0f);
			str_format(aBuf, sizeof(aBuf), "%dHP", Health);
			GS()->CreateText(nullptr, false, DrawPosition, vec2(0, 0), 40, aBuf);
		}
	}
	return PlayerFinding;
}

bool CCharacterBotAI::FunctionGuardian()
{
	bool MobMove = true;

	// reset input
	ResetInput();

	// Search for a tank player within a 1000 unit range
	CPlayer* pPlayer = SearchTankPlayer(1000.0f);

	// If a tank player is found and their revelation is less than 1000, or no player is found at all
	if(!pPlayer || !pPlayer->GetCharacter())
	{
		// Search for a mob within a 600 unit range
		pPlayer = SearchMob(600.f);
	}

	// Check if pPlayer exists and if pPlayer's character
	if(pPlayer && pPlayer->GetCharacter())
	{
		// Set the target of the AI to pPlayer's CID with a priority of 100
		AI()->GetTarget()->Set(pPlayer->GetCID(), 100);
		m_pBotPlayer->m_TargetPos = pPlayer->GetCharacter()->GetPos();
	}

	// Calculate the distance between the spawn point and the current position
	float Distance = distance(m_SpawnPoint, m_Pos);
	if(AI()->GetTarget()->IsEmpty())
	{
		// If the active target is empty and the distance is greater than 800.0f
		if(Distance > 800.0f)
		{
			// Change the position of the player to m_SpawnPoint
			ChangePosition(m_SpawnPoint);
			AI()->GetTarget()->Reset();
		}

		// Check if the distance is less than 128.0f 
		if(Distance < 256.0f)
		{
			if(Server()->Tick() % Server()->TickSpeed() == 0)
				m_Input.m_TargetY = random_int() % 4 - random_int() % 8;

			m_Input.m_TargetX = (m_Input.m_Direction * 10 + 1);
			m_Input.m_Direction = 0;
			MobMove = false;
		}

		// always with empty target / target pos it's owner
		m_pBotPlayer->m_TargetPos = m_SpawnPoint;
	}

	// bot with weapons since it has spread.
	if(MobMove)
	{
		Fire();
		Move();
	}

	m_PrevPos = m_Pos;
	if(m_Input.m_Direction)
		m_PrevDirection = m_Input.m_Direction;

	EmotesAction(m_EmotionsStyle);
	HandleWeapons();
	return true;
}
