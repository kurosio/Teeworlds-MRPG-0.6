/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "playerbot.h"
#include "gamecontext.h"

#include "entities/botai/character_bot_ai.h"
#include "worldmodes/dungeon.h"

#include "core/components/Bots/BotManager.h"
#include "core/utilities/pathfinder.h"

MACRO_ALLOC_POOL_ID_IMPL(CPlayerBot, MAX_CLIENTS* ENGINE_MAX_WORLDS + MAX_CLIENTS)

CPlayerBot::CPlayerBot(CGS* pGS, int ClientID, int BotID, int MobID, int SpawnPoint)
	: CPlayer(pGS, ClientID), m_BotType(SpawnPoint), m_BotID(BotID), m_MobID(MobID), m_BotHealth(0), m_LastPosTick(0)
{
	m_EidolonCID = -1;
	m_OldTargetPos = vec2(0, 0);
	m_DungeonAllowedSpawn = false;
	m_BotStartHealth = CPlayerBot::GetAttributeSize(AttributeIdentifier::HP);
	m_Items.reserve(CItemDescription::Data().size());
	m_pPathFinderData = new CPathFinderPrepare;
	PrepareRespawnTick();
}

CPlayerBot::~CPlayerBot()
{
	// Set all elements in the m_aVisibleActive array of the DataBotInfo object at index m_BotID to 0
	std::memset(DataBotInfo::ms_aDataBot[m_BotID].m_aVisibleActive, 0, MAX_PLAYERS * sizeof(bool));

	// free memory
	delete m_pCharacter;
	delete m_pPathFinderData;
}

// This method is used to initialize the quest bot mob info for the player bot
// It takes an instance of CQuestBotMobInfo as a parameter
void CPlayerBot::InitQuestBotMobInfo(CQuestBotMobInfo elem)
{
	// Check if the bot type is TYPE_BOT_QUEST_MOB
	if(m_BotType == TYPE_BOT_QUEST_MOB)
	{
		// Assign the passed CQuestBotMobInfo instance to the member variable m_QuestMobInfo
		m_QuestMobInfo = elem;

		// Set all elements of m_ActiveForClient m_CompleteClient array to false
		std::memset(m_QuestMobInfo.m_ActiveForClient, 0, MAX_PLAYERS * sizeof(bool));
		std::memset(m_QuestMobInfo.m_CompleteClient, 0, MAX_PLAYERS * sizeof(bool));

		// Update the attribute size of the player bot for the attribute identifier HP
		m_BotStartHealth = CPlayerBot::GetAttributeSize(AttributeIdentifier::HP);
	}
}

void CPlayerBot::Tick()
{
	// Check if the character is valid
	if(m_pCharacter)
	{
		// Check if the character is alive
		if(m_pCharacter->IsAlive())
		{
			// Check if the bot type is TYPE_BOT_EIDOLON
			if(m_BotType == TYPE_BOT_EIDOLON)
			{
				// Get the eidolon owner
				if(const CPlayer* pOwner = GetEidolonOwner(); pOwner && pOwner->GetCharacter() && distance(pOwner->m_ViewPos, m_ViewPos) > 1000.f)
				{
					// Teleport the bot to the owner's position
					vec2 OwnerPos = pOwner->GetCharacter()->GetPos();
					m_pCharacter->m_DoorHit = false;
					m_pCharacter->ChangePosition(OwnerPos);

					// Copy the owner's velocity, direction, and input to the bot
					m_pCharacter->m_Core.m_Vel = pOwner->GetCharacter()->m_Core.m_Vel;
					m_pCharacter->m_Core.m_Direction = pOwner->GetCharacter()->m_Core.m_Direction;
					m_pCharacter->m_Core.m_Input = pOwner->GetCharacter()->m_Core.m_Input;
				}
			}

			// Check if the bot is active
			if(IsActive())
			{
				m_ViewPos = m_pCharacter->GetPos();
				HandlePathFinder();
			}
		}
		else
		{
			// Delete the character and set it to nullptr
			delete m_pCharacter;
			m_pCharacter = nullptr;
		}
	}
	// If the bot is spawned and the respawn tick has passed
	else if(m_WantSpawn && m_aPlayerTick[Respawn] <= Server()->Tick())
	{
		// Try to respawn the bot
		TryRespawn();
	}
}

void CPlayerBot::PostTick()
{
	// update playerbot tick
	HandleTuningParams();
	HandleEffects();
}

CPlayer* CPlayerBot::GetEidolonOwner() const
{
	if(m_BotType != TYPE_BOT_EIDOLON || m_MobID < 0 || m_MobID >= MAX_PLAYERS)
		return nullptr;
	return GS()->m_apPlayers[m_MobID];
}

CPlayerItem* CPlayerBot::GetItem(ItemIdentifier ID)
{
	dbg_assert(CItemDescription::Data().find(ID) != CItemDescription::Data().end(), "invalid referring to the CPlayerItem (from playerbot.h)");

	auto it = m_Items.find(ID);
	if(it == m_Items.end())
	{
		if(DataBotInfo::ms_aDataBot[m_BotID].m_EquippedModules.hasSet(std::to_string(ID)))
			it = m_Items.emplace(ID, std::make_unique<CPlayerItem>(ID, m_ClientID, 1, 0, 100, 1)).first;
		else
			it = m_Items.emplace(ID, std::make_unique<CPlayerItem>(ID, m_ClientID, 0, 0, 0, 0)).first;
	}

	return it->second.get();
}

void CPlayerBot::HandleEffects()
{
	if(Server()->Tick() % Server()->TickSpeed() != 0 || m_aEffects.empty())
		return;

	for(auto pEffect = m_aEffects.begin(); pEffect != m_aEffects.end();)
	{
		pEffect->second--;
		if(pEffect->second <= 0)
		{
			pEffect = m_aEffects.erase(pEffect);
			continue;
		}
		++pEffect;
	}
}

bool CPlayerBot::IsActive() const
{
	return GS()->m_World.IsBotActive(m_ClientID);
}

bool CPlayerBot::IsConversational() const
{
	return ((m_BotType == TYPE_BOT_NPC && NpcBotInfo::ms_aNpcBot[m_MobID].m_Function != FUNCTION_NPC_GUARDIAN) || 
		(m_BotType == TYPE_BOT_QUEST && QuestBotInfo::ms_aQuestBot[m_MobID].m_HasAction)) && IsActive();
}

void CPlayerBot::PrepareRespawnTick()
{
	if(m_BotType == TYPE_BOT_MOB)
	{
		m_DisabledBotDamage = false;
		m_aPlayerTick[Respawn] = Server()->Tick() + Server()->TickSpeed() * MobBotInfo::ms_aMobBot[m_MobID].m_RespawnTick;
	}
	else if(m_BotType == TYPE_BOT_QUEST_MOB)
	{
		m_DisabledBotDamage = false;
		m_aPlayerTick[Respawn] = Server()->Tick() + Server()->TickSpeed();
	}
	else if(m_BotType == TYPE_BOT_NPC && NpcBotInfo::ms_aNpcBot[m_MobID].m_Function == FUNCTION_NPC_GUARDIAN)
	{
		m_DisabledBotDamage = false;
		m_aPlayerTick[Respawn] = Server()->Tick() + Server()->TickSpeed() * 30;
	}
	else if(m_BotType == TYPE_BOT_QUEST || m_BotType == TYPE_BOT_NPC || m_BotType == TYPE_BOT_EIDOLON)
	{
		m_DisabledBotDamage = true;
		m_aPlayerTick[Respawn] = Server()->Tick();
	}
	else
	{
		m_DisabledBotDamage = true;
		m_aPlayerTick[Respawn] = Server()->Tick() + Server()->TickSpeed() * 3;
	}

	// Set the want spawn variable to true
	m_WantSpawn = true;
}

int CPlayerBot::GetAttributeSize(AttributeIdentifier ID) const
{
	if(m_BotType == TYPE_BOT_MOB || m_BotType == TYPE_BOT_EIDOLON || m_BotType == TYPE_BOT_QUEST_MOB ||
		(m_BotType == TYPE_BOT_NPC && NpcBotInfo::ms_aNpcBot[m_MobID].m_Function == FUNCTION_NPC_GUARDIAN))
	{
		auto CalculateAttribute = [ID, this](int Power, int Spread, bool Boss) -> int
		{
			// get stats from the bot's equipment
			int Size = Power;
			for(unsigned i = 0; i < NUM_EQUIPPED; i++)
			{
				const int ItemID = GetEquippedItemID((ItemFunctional)i);
				if(ItemID > 0)
					Size += GS()->GetItemInfo(ItemID)->GetInfoEnchantStats(ID);
			}

			// sync power mobs
			float Percent = 100.0f;
			CAttributeDescription* pAttribute = GS()->GetAttributeInfo(ID);
			if(ID == AttributeIdentifier::SpreadShotgun || ID == AttributeIdentifier::SpreadGrenade || ID == AttributeIdentifier::SpreadRifle)
				Size = Spread;
			else if(pAttribute->IsGroup(AttributeGroup::Healer))
				Percent = 15.0f;
			else if(pAttribute->IsGroup(AttributeGroup::Hardtype))
				Percent = 5.0f;

			if(Boss && ID != AttributeIdentifier::HP)
				Percent /= 10.0f;

			const int SyncPercentSize = maximum(1, translate_to_percent_rest(Size, Percent));
			return SyncPercentSize;
		};

		// Initialize Size variable to 0
		int Size = 0;

		// Check if bot type is TYPE_BOT_EIDOLON
		if(m_BotType == TYPE_BOT_EIDOLON)
		{
			// Check if game is in Dungeon mode
			if(GS()->IsWorldType(WorldType::Dungeon))
			{
				// Calculate Size based on sync factor
				// Translate the sync factor to percent and then calculate the attribute
				Size = CalculateAttribute(translate_to_percent_rest(maximum(1, dynamic_cast<CGameControllerDungeon*>(GS()->m_pController)->GetSyncFactor()), 5), 1, false);
			}
			else
			{
				// Calculate Size based on Eidolon item ID
				Size = CalculateAttribute(m_EidolonItemID, 1, false);
			}
		}
		// Check if bot type is TYPE_BOT_MOB
		else if(m_BotType == TYPE_BOT_MOB)
		{
			// Calculate Size based on Mob bot info
			Size = CalculateAttribute(MobBotInfo::ms_aMobBot[m_MobID].m_Power, MobBotInfo::ms_aMobBot[m_MobID].m_Spread, MobBotInfo::ms_aMobBot[m_MobID].m_Boss);
		}
		// Check if bot type is TYPE_BOT_QUEST_MOB
		else if(m_BotType == TYPE_BOT_QUEST_MOB)
		{
			// Calculate Size based on Quest Mob info
			Size = CalculateAttribute(m_QuestMobInfo.m_AttributePower, m_QuestMobInfo.m_AttributeSpread, true);
		}
		// Check if bot type is TYPE_BOT_NPC
		else if(m_BotType == TYPE_BOT_NPC)
		{
			// Calculate Size based on Npc info
			Size = CalculateAttribute(10, 0, true);
		}
		return Size;
	}

	return 10;
}

bool CPlayerBot::GiveEffect(const char* Potion, int Sec, float Chance)
{
	if(!m_pCharacter || !m_pCharacter->IsAlive())
		return false;

	const float RandomChance = random_float(100.0f);
	if(RandomChance < Chance)
	{
		m_aEffects[Potion] = Sec;
		return true;
	}

	return false;
}

bool CPlayerBot::IsActiveEffect(const char* Potion) const
{
	return m_aEffects.find(Potion) != m_aEffects.end();
}

void CPlayerBot::ClearEffects()
{
	m_aEffects.clear();
}

void CPlayerBot::TryRespawn()
{
	// select spawn point
	vec2 SpawnPos;
	if(m_BotType == TYPE_BOT_MOB)
	{
		// close spawn mobs on non allowed spawn dungeon
		if(GS()->IsWorldType(WorldType::Dungeon) && !m_DungeonAllowedSpawn)
			return;

		const vec2 MobRespawnPosition = MobBotInfo::ms_aMobBot[m_MobID].m_Position;
		if(!GS()->m_pController->CanSpawn(m_BotType, &SpawnPos, std::make_pair(MobRespawnPosition, 800.f)))
			return;

		// reset spawn mobs on non allowed spawn dungeon
		if(GS()->IsWorldType(WorldType::Dungeon) && m_DungeonAllowedSpawn)
			m_DungeonAllowedSpawn = false;
	}
	else if(m_BotType == TYPE_BOT_NPC)
	{
		SpawnPos = NpcBotInfo::ms_aNpcBot[m_MobID].m_Position;
	}
	else if(m_BotType == TYPE_BOT_QUEST)
	{
		SpawnPos = QuestBotInfo::ms_aQuestBot[m_MobID].m_Position;
	}
	else if(m_BotType == TYPE_BOT_EIDOLON)
	{
		CPlayer* pOwner = GetEidolonOwner();
		if(!pOwner || !pOwner->GetCharacter())
			return;

		SpawnPos = pOwner->GetCharacter()->GetPos();
	}
	else if(m_BotType == TYPE_BOT_QUEST_MOB)
	{
		SpawnPos = GetQuestBotMobInfo().m_Position;
	}

	// create character
	const int AllocMemoryCell = m_ClientID + GS()->GetWorldID() * MAX_CLIENTS;
	m_pCharacter = new(AllocMemoryCell) CCharacterBotAI(&GS()->m_World);
	m_pCharacter->Spawn(this, SpawnPos);
	GS()->CreatePlayerSpawn(SpawnPos, GetMaskVisibleForClients());
}

int64_t CPlayerBot::GetMaskVisibleForClients() const
{
	// Initialize the mask with the client ID
	int64_t Mask = 0;

	// Loop through all players
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		// Check if the player is active for the client
		if(IsActiveForClient(i))
		{
			// Add the player's ID to the mask
			Mask |= CmaskOne(i);
		}
	}

	// Return the final mask
	return Mask;
}

StateSnapping CPlayerBot::IsActiveForClient(int ClientID) const
{
	// Get the snapping player
	CPlayer* pSnappingPlayer = GS()->m_apPlayers[ClientID];

	// Check if the client ID is valid and if the snapping player exists
	if(ClientID < 0 || ClientID >= MAX_PLAYERS || !pSnappingPlayer)
		return STATE_SNAPPING_NONE;

	// Check if the bot type is quest bot
	if(m_BotType == TYPE_BOT_QUEST)
	{
		// Check if the quest state for the snapping player is not ACCEPT
		const QuestIdentifier QuestID = QuestBotInfo::ms_aQuestBot[m_MobID].m_QuestID;
		if(pSnappingPlayer->GetQuest(QuestID)->GetState() != QuestState::ACCEPT)
			return STATE_SNAPPING_NONE;

		// Check if the current step of the quest bot is not the same as the current step of the snapping player's quest
		if((QuestBotInfo::ms_aQuestBot[m_MobID].m_StepPos != pSnappingPlayer->GetQuest(QuestID)->GetStepPos()))
			return STATE_SNAPPING_NONE;

		// Check if the step for the bot mob in the snapping player's quest is already completed
		if(pSnappingPlayer->GetQuest(QuestID)->GetStepByMob(GetBotMobID())->m_StepComplete)
			return STATE_SNAPPING_NONE;

		// Set the visible active state for the snapping player to true
		DataBotInfo::ms_aDataBot[m_BotID].m_aVisibleActive[ClientID] = true;
	}

	// Check if the bot type is NPC bot
	if(m_BotType == TYPE_BOT_NPC)
	{
		// Check if the function of the NPC bot is NPC_GUARDIAN
		if(NpcBotInfo::ms_aNpcBot[m_MobID].m_Function == FUNCTION_NPC_GUARDIAN)
			return STATE_SNAPPING_FULL;

		// Check if the visible active state for the snapping player is already true
		if(DataBotInfo::ms_aDataBot[m_BotID].m_aVisibleActive[ClientID])
			return STATE_SNAPPING_NONE;

		// Check if the NPC's function is to give a quest and if the player has already accepted the quest
		const int GivesQuest = GS()->Core()->BotManager()->GetQuestNPC(m_MobID);
		if(NpcBotInfo::ms_aNpcBot[m_MobID].m_Function == FUNCTION_NPC_GIVE_QUEST
			&& pSnappingPlayer->GetQuest(GivesQuest)->GetState() != QuestState::NO_ACCEPT)
			return STATE_SNAPPING_ONLY_CHARACTER;
	}

	// Return full state as the default state
	return STATE_SNAPPING_FULL;
}

void CPlayerBot::HandleTuningParams()
{
	if(!(m_PrevTuningParams == m_NextTuningParams))
		m_PrevTuningParams = m_NextTuningParams;

	m_NextTuningParams = *GS()->Tuning();
}

void CPlayerBot::Snap(int SnappingClient)
{
	// Get the client ID
	int ID = m_ClientID;

	// Translate the client ID to the corresponding snapping client
	if(!Server()->Translate(ID, SnappingClient))
		return;

	// Check if the game is active or if it is active for the snapping client
	if(!IsActive() || !IsActiveForClient(SnappingClient))
		return;

	CNetObj_ClientInfo* pClientInfo = static_cast<CNetObj_ClientInfo*>(Server()->SnapNewItem(NETOBJTYPE_CLIENTINFO, ID, sizeof(CNetObj_ClientInfo)));
	if(!pClientInfo)
		return;

	char aNameBuf[MAX_NAME_LENGTH];
	GetFormatedName(aNameBuf, sizeof(aNameBuf));
	StrToInts(&pClientInfo->m_Name0, 4, aNameBuf);
	if(m_BotType == TYPE_BOT_MOB || m_BotType == TYPE_BOT_QUEST_MOB || (m_BotType == TYPE_BOT_NPC && NpcBotInfo::ms_aNpcBot[m_MobID].m_Function == FUNCTION_NPC_GUARDIAN))
	{
		const float Progress = translate_to_percent((float)GetStartHealth(), (float)GetHealth());

		std::string ProgressBar = Tools::String::progressBar(100, (int)Progress, 33, "\u25B0", "\u25B1");
		StrToInts(&pClientInfo->m_Clan0, 3, ProgressBar.c_str());
	}
	else
	{
		StrToInts(&pClientInfo->m_Clan0, 3, GetStatus());
	}

	pClientInfo->m_Country = 0;
	{
		StrToInts(&pClientInfo->m_Skin0, 6, GetTeeInfo().m_aSkinName);
		pClientInfo->m_UseCustomColor = GetTeeInfo().m_UseCustomColor;
		pClientInfo->m_ColorBody = GetTeeInfo().m_ColorBody;
		pClientInfo->m_ColorFeet = GetTeeInfo().m_ColorFeet;
	}

	CNetObj_PlayerInfo* pPlayerInfo = static_cast<CNetObj_PlayerInfo*>(Server()->SnapNewItem(NETOBJTYPE_PLAYERINFO, ID, sizeof(CNetObj_PlayerInfo)));
	if(!pPlayerInfo)
		return;

	const bool LocalClient = (m_ClientID == SnappingClient);
	pPlayerInfo->m_Latency = 0;
	pPlayerInfo->m_Score = 0;
	pPlayerInfo->m_Local = LocalClient;
	pPlayerInfo->m_ClientID = ID;
	pPlayerInfo->m_Team = TEAM_BLUE;
}

void CPlayerBot::FakeSnap()
{
	CPlayer::FakeSnap();
}

Mood CPlayerBot::GetMoodState() const
{
	CCharacterBotAI* pChar = (CCharacterBotAI*)m_pCharacter;
	if(!pChar)
		return Mood::NORMAL;

	if(GetBotType() == TYPE_BOT_MOB && !pChar->AI()->GetTarget()->IsEmpty())
		return Mood::ANGRY;

	if(GetBotType() == TYPE_BOT_QUEST_MOB && !pChar->AI()->GetTarget()->IsEmpty())
		return Mood::ANGRY;

	if(GetBotType() == TYPE_BOT_EIDOLON)
		return Mood::FRIENDLY;

	if(GetBotType() == TYPE_BOT_QUEST)
		return Mood::QUEST;

	if(GetBotType() == TYPE_BOT_NPC)
	{
		bool IsGuardian = NpcBotInfo::ms_aNpcBot[m_MobID].m_Function == FUNCTION_NPC_GUARDIAN;
		if(IsGuardian && !pChar->AI()->GetTarget()->IsEmpty())
			return Mood::AGRESSED;

		return Mood::FRIENDLY;
	}

	return Mood::NORMAL;
}

int CPlayerBot::GetBotLevel() const
{
	return (m_BotType == TYPE_BOT_MOB ? MobBotInfo::ms_aMobBot[m_MobID].m_Level : 1);
}

void CPlayerBot::GetFormatedName(char* aBuffer, int BufferSize)
{
	if(m_BotType == TYPE_BOT_MOB || m_BotType == TYPE_BOT_QUEST_MOB || (m_BotType == TYPE_BOT_NPC && NpcBotInfo::ms_aNpcBot[m_MobID].m_Function == FUNCTION_NPC_GUARDIAN))
	{
		const int PercentHP = translate_to_percent(GetStartHealth(), GetHealth());
		str_format(aBuffer, BufferSize, "%s:%d%%", DataBotInfo::ms_aDataBot[m_BotID].m_aNameBot, clamp(PercentHP, 1, 100));
	}
	else
	{
		str_format(aBuffer, BufferSize, "%s", DataBotInfo::ms_aDataBot[m_BotID].m_aNameBot);
	}
}

int CPlayerBot::GetEquippedItemID(ItemFunctional EquipID, int SkipItemID) const
{
	if((EquipID >= EQUIP_HAMMER && EquipID <= EQUIP_LASER) || EquipID == EQUIP_ARMOR)
		return DataBotInfo::ms_aDataBot[m_BotID].m_aEquipSlot[EquipID];
	return -1;
}

const char* CPlayerBot::GetStatus() const
{
	if(m_BotType == TYPE_BOT_MOB && MobBotInfo::ms_aMobBot[m_MobID].m_Boss)
	{
		return GS()->IsWorldType(WorldType::Dungeon) ? "Boss" : "Raid";
	}

	if(m_BotType == TYPE_BOT_QUEST_MOB)
	{
		return "Questing mob";
	}

	if(m_BotType == TYPE_BOT_EIDOLON && GetEidolonOwner())
	{
		int OwnerID = GetEidolonOwner()->GetCID();
		return Server()->ClientName(OwnerID);
	}

	switch(GetMoodState())
	{
		default:
		case Mood::NORMAL: return "\0";
		case Mood::FRIENDLY: return "Friendly";
		case Mood::QUEST:
		{
			const int QuestID = QuestBotInfo::ms_aQuestBot[m_MobID].m_QuestID;
			return GS()->GetQuestInfo(QuestID)->GetName();
		}
		case Mood::ANGRY: return "Angry";
		case Mood::AGRESSED: return "Aggressive";
	}
}

int CPlayerBot::GetPlayerWorldID() const
{
	switch(m_BotType)
	{
		case TYPE_BOT_MOB: return MobBotInfo::ms_aMobBot[m_MobID].m_WorldID;
		case TYPE_BOT_QUEST_MOB: return m_QuestMobInfo.m_WorldID;
		case TYPE_BOT_NPC: return NpcBotInfo::ms_aNpcBot[m_MobID].m_WorldID;
		case TYPE_BOT_EIDOLON: return Server()->GetClientWorldID(m_MobID);
		case TYPE_BOT_QUEST: return QuestBotInfo::ms_aQuestBot[m_MobID].m_WorldID;
		default: return -1;
	}
}

CTeeInfo& CPlayerBot::GetTeeInfo() const
{
	dbg_assert(DataBotInfo::IsDataBotValid(m_BotID), "Assert getter TeeInfo from data bot");
	return DataBotInfo::ms_aDataBot[m_BotID].m_TeeInfos;
}

void CPlayerBot::HandlePathFinder()
{
	if(!IsActive() || !m_pCharacter || !m_pCharacter->IsAlive())
		return;

	// Check if the bot type is TYPE_BOT_MOB
	if(GetBotType() == TYPE_BOT_MOB)
	{
		// Check if the target position is not (0, 0) and if the current server tick modulo (3 * m_ClientID) is 0
		if(m_TargetPos != vec2(0, 0))
		{
			// Prepare the path finder data for default path finding
			GS()->PathFinder()->Handle()->Prepare<CPathFinderPrepare::DEFAULT>(m_pPathFinderData, m_pCharacter->m_Core.m_Pos, m_TargetPos);
		}
		// If the target position is (0, 0) or the distance between the view position and the target position is less than 128.0f
		else if(m_TargetPos == vec2(0, 0) || distance(m_pCharacter->m_Core.m_Pos, m_TargetPos) < 128.0f)
		{
			// Set the last position tick to the current server tick plus a random time interval
			m_LastPosTick = Server()->Tick() + (Server()->TickSpeed() * 2 + rand() % 4);
			// Prepare the path finder data for random path finding
			GS()->PathFinder()->Handle()->Prepare<CPathFinderPrepare::RANDOM>(m_pPathFinderData, m_pCharacter->m_Core.m_Pos, m_TargetPos);
		}
	}

	// Check if the bot type is TYPE_BOT_EIDOLON
	else if(GetBotType() == TYPE_BOT_EIDOLON)
	{
		// Get the owner ID of the bot
		int OwnerID = m_MobID;
		// Check if the owner player exists and if the target position is not (0, 0) and if the current server tick modulo (Server()->TickSpeed() / 3) is 0
		if(const CPlayer* pPlayerOwner = GS()->GetPlayer(OwnerID, true, true); pPlayerOwner && m_TargetPos != vec2(0, 0))
		{
			// Prepare the path finder data for default path finding
			GS()->PathFinder()->Handle()->Prepare<CPathFinderPrepare::DEFAULT>(m_pPathFinderData, m_pCharacter->m_Core.m_Pos, m_TargetPos);
		}
	}

	// Check if the bot type is TYPE_BOT_QUEST_MOB
	else if(GetBotType() == TYPE_BOT_QUEST_MOB)
	{
		// Check if the target position is not (0, 0) and if the current server tick modulo (Server()->TickSpeed() / 3) is 0
		if(m_TargetPos != vec2(0, 0))
		{
			m_pPathFinderData->Get().Clear();

			// Prepare the path finder data for default path finding
			GS()->PathFinder()->Handle()->Prepare<CPathFinderPrepare::DEFAULT>(m_pPathFinderData, m_pCharacter->m_Core.m_Pos, m_TargetPos);
		}
	}

	// Check if the bot type is TYPE_BOT_NPC and the function is FUNCTION_NPC_GUARDIAN
	else if(GetBotType() == TYPE_BOT_NPC && NpcBotInfo::ms_aNpcBot[m_MobID].m_Function == FUNCTION_NPC_GUARDIAN)
	{
		// Check if the target position is not (0, 0) and if the current server tick modulo (Server()->TickSpeed() / 3) is 0
		if(m_TargetPos != vec2(0, 0))
		{
			m_pPathFinderData->Get().Clear();

			// Prepare the path finder data for default path finding
			GS()->PathFinder()->Handle()->Prepare<CPathFinderPrepare::DEFAULT>(m_pPathFinderData, m_pCharacter->m_Core.m_Pos, m_TargetPos);
		}
	}
}