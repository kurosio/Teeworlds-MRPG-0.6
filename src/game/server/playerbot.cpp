/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "playerbot.h"

#include "gamecontext.h"

#include "entities/botai/character_bot_ai.h"
#include "worldmodes/dungeon.h"

#include "mmocore/Components/Bots/BotManager.h"
#include "mmocore/PathFinder.h"

MACRO_ALLOC_POOL_ID_IMPL(CPlayerBot, MAX_CLIENTS* ENGINE_MAX_WORLDS + MAX_CLIENTS)

CPlayerBot::CPlayerBot(CGS* pGS, int ClientID, int BotID, int SubBotID, int SpawnPoint)
	: CPlayer(pGS, ClientID), m_BotType(SpawnPoint), m_BotID(BotID), m_MobID(SubBotID), m_BotHealth(0), m_LastPosTick(0)
{
	m_EidolonCID = -1;
	m_OldTargetPos = vec2(0, 0);
	m_DungeonAllowedSpawn = false;
	m_BotStartHealth = CPlayerBot::GetAttributeSize(AttributeIdentifier::HP);
}

CPlayerBot::~CPlayerBot()
{
	// Set all elements in the m_aVisibleActive array of the DataBotInfo object at index m_BotID to 0
	std::memset(DataBotInfo::ms_aDataBot[m_BotID].m_aVisibleActive, 0, MAX_PLAYERS * sizeof(bool));

	// Delete the m_pCharacter object and set it to nullptr
	delete m_pCharacter;
	m_pCharacter = nullptr;
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
	m_BotActive = false;

	if(m_pCharacter && !m_pCharacter->IsAlive())
	{
		delete m_pCharacter;
		m_pCharacter = nullptr;
	}

	if(m_pCharacter)
	{
		m_BotActive = GS()->IsPlayersNearby(m_pCharacter->GetPos(), 1000.0f);

		// update eidolon position
		if(m_BotType == TYPE_BOT_EIDOLON)
		{
			m_BotActive = m_BotActive && GetEidolonOwner();
			if(const CPlayer* pOwner = GetEidolonOwner(); pOwner && pOwner->GetCharacter() && !m_BotActive)
			{
				vec2 OwnerPos = pOwner->GetCharacter()->GetPos();
				m_pCharacter->m_DoorHit = false;
				m_pCharacter->ChangePosition(OwnerPos);
				m_pCharacter->m_Core.m_Vel = pOwner->GetCharacter()->m_Core.m_Vel;
				m_pCharacter->m_Core.m_Direction = pOwner->GetCharacter()->m_Core.m_Direction;
				m_BotActive = true;
			}
		}

		if(m_pCharacter->IsAlive() && m_BotActive)
		{
			m_ViewPos = m_pCharacter->GetPos();
			HandlePathFinder();
		}
	}
	else if(m_Spawned && GetRespawnTick() <= Server()->Tick())
	{
		m_BotActive = true;
		TryRespawn();
	}
}

void CPlayerBot::PostTick()
{
	// update playerbot tick
	HandleTuningParams();
	EffectsTick();
}

CPlayer* CPlayerBot::GetEidolonOwner() const
{
	if(m_BotType != TYPE_BOT_EIDOLON || m_MobID < 0 || m_MobID >= MAX_PLAYERS)
		return nullptr;
	return GS()->m_apPlayers[m_MobID];
}

void CPlayerBot::EffectsTick()
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

int CPlayerBot::GetRespawnTick() const
{
	if(m_BotType == TYPE_BOT_NPC && NpcBotInfo::ms_aNpcBot[m_MobID].m_Function == FUNCTION_NPC_GUARDIAN)
	{
		return m_aPlayerTick[Respawn] * 2;
	}

	if(m_BotType == TYPE_BOT_QUEST || m_BotType == TYPE_BOT_NPC || m_BotType == TYPE_BOT_EIDOLON)
	{
		return m_aPlayerTick[Respawn];
	}

	return m_aPlayerTick[Respawn] + Server()->TickSpeed() * 3;
}

int CPlayerBot::GetAttributeSize(AttributeIdentifier ID)
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
			else if(pAttribute->IsType(AttributeType::Healer))
				Percent = 15.0f;
			else if(pAttribute->IsType(AttributeType::Hardtype))
				Percent = 5.0f;

			if(Boss && ID != AttributeIdentifier::HP)
				Percent /= 10.0f;

			const int SyncPercentSize = max(1, translate_to_percent_rest(Size, Percent));
			return SyncPercentSize;
		};

		// Initialize Size variable to 0
		int Size = 0;

		// Check if bot type is TYPE_BOT_EIDOLON
		if(m_BotType == TYPE_BOT_EIDOLON)
		{
			// Check if game is in Dungeon mode
			if(GS()->IsDungeon())
			{
				// Calculate Size based on sync factor
				// Translate the sync factor to percent and then calculate the attribute
				Size = CalculateAttribute(translate_to_percent_rest(max(1, dynamic_cast<CGameControllerDungeon*>(GS()->m_pController)->GetSyncFactor()), 5), 1, false);
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
			Size = CalculateAttribute(1500, 0, true);
		}
		return Size;
	}

	return 10;
}

void CPlayerBot::GiveEffect(const char* Potion, int Sec, float Chance)
{
	if(!m_pCharacter || !m_pCharacter->IsAlive())
		return;

	const float RandomChance = frandom() * 100.0f;
	if(RandomChance < Chance)
		m_aEffects[Potion] = Sec;
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
		if(GS()->IsDungeon() && !m_DungeonAllowedSpawn)
			return;

		const vec2 MobRespawnPosition = MobBotInfo::ms_aMobBot[m_MobID].m_Position;
		if(!GS()->m_pController->CanSpawn(m_BotType, &SpawnPos, std::make_pair(MobRespawnPosition, 800.f)))
			return;

		// reset spawn mobs on non allowed spawn dungeon
		if(GS()->IsDungeon() && m_DungeonAllowedSpawn)
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

int64 CPlayerBot::GetMaskVisibleForClients() const
{
	int64 Mask = CmaskOne(m_ClientID);
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		if(IsVisibleForClient(i))
			Mask |= CmaskOne(i);
	}
	return Mask;
}

/*
	0 - empty
	1 - is active draw only bot
	2 - is active draw bot and entities
*/
int CPlayerBot::IsVisibleForClient(int ClientID) const
{
	CPlayer* pSnappingPlayer = GS()->m_apPlayers[ClientID];
	if(ClientID < 0 || ClientID >= MAX_PLAYERS || !pSnappingPlayer || !m_BotActive)
		return 0;

	if(m_BotType == TYPE_BOT_QUEST)
	{
		const int QuestID = QuestBotInfo::ms_aQuestBot[m_MobID].m_QuestID;
		if(pSnappingPlayer->GetQuest(QuestID)->GetState() != QuestState::ACCEPT)
			return 0;

		if((QuestBotInfo::ms_aQuestBot[m_MobID].m_Step != pSnappingPlayer->GetQuest(QuestID)->GetCurrentStepPos()) || pSnappingPlayer->GetQuest(QuestID)->GetStepByMob(GetBotMobID())->m_StepComplete)
			return 0;

		// [first] quest bot active for player
		DataBotInfo::ms_aDataBot[m_BotID].m_aVisibleActive[ClientID] = true;
	}

	if(m_BotType == TYPE_BOT_NPC)
	{
		// [second] skip snapping for npc already snap on quest state
		if(DataBotInfo::ms_aDataBot[m_BotID].m_aVisibleActive[ClientID])
			return 0;

		if(!IsActiveQuests(ClientID))
			return 1;
	}

	return 2;
}

void CPlayerBot::HandleTuningParams()
{
	if(!(m_PrevTuningParams == m_NextTuningParams))
		m_PrevTuningParams = m_NextTuningParams;

	m_NextTuningParams = *GS()->Tuning();
}

void CPlayerBot::Snap(int SnappingClient)
{
	int ID = m_ClientID;
	if(!Server()->Translate(ID, SnappingClient) || !IsVisibleForClient(SnappingClient))
		return;

	CNetObj_ClientInfo* pClientInfo = static_cast<CNetObj_ClientInfo*>(Server()->SnapNewItem(NETOBJTYPE_CLIENTINFO, ID, sizeof(CNetObj_ClientInfo)));
	if(!pClientInfo)
		return;

	if(m_BotType == TYPE_BOT_MOB || m_BotType == TYPE_BOT_QUEST_MOB || (m_BotType == TYPE_BOT_NPC && NpcBotInfo::ms_aNpcBot[m_MobID].m_Function == FUNCTION_NPC_GUARDIAN))
	{
		const int PercentHP = translate_to_percent(GetStartHealth(), GetHealth());

		char aNameBuf[MAX_NAME_LENGTH];
		str_format(aNameBuf, sizeof(aNameBuf), "%s:%d%%", DataBotInfo::ms_aDataBot[m_BotID].m_aNameBot, clamp(PercentHP, 1, 100));
		StrToInts(&pClientInfo->m_Name0, 4, aNameBuf);
	}
	else
	{
		StrToInts(&pClientInfo->m_Name0, 4, DataBotInfo::ms_aDataBot[m_BotID].m_aNameBot);
	}

	StrToInts(&pClientInfo->m_Clan0, 3, GetStatus());
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
	CCharacterBotAI* pChr = (CCharacterBotAI*)m_pCharacter;
	if((GetBotType() == TYPE_BOT_MOB || GetBotType() == TYPE_BOT_QUEST_MOB) && pChr && !pChr->AI()->GetTarget()->IsEmpty())
		return Mood::AGRESSED;
	else if(GetBotType() == TYPE_BOT_NPC)
	{
		if(NpcBotInfo::ms_aNpcBot[m_MobID].m_Function == FUNCTION_NPC_GUARDIAN && !pChr->AI()->GetTarget()->IsEmpty())
			return Mood::AGRESSED;
		else
			return Mood::FRIENDLY;
	}
	else if(GetBotType() == TYPE_BOT_EIDOLON)
		return Mood::FRIENDLY;
	else if(GetBotType() == TYPE_BOT_QUEST)
		return Mood::QUEST;
	else
		return Mood::NORMAL;
}

int CPlayerBot::GetBotLevel() const
{
	return (m_BotType == TYPE_BOT_MOB ? MobBotInfo::ms_aMobBot[m_MobID].m_Level : 1);
}

bool CPlayerBot::IsActiveQuests(int SnapClientID) const
{
	CPlayer* pSnappingPlayer = GS()->m_apPlayers[SnapClientID];
	if(SnapClientID >= MAX_PLAYERS || SnapClientID < 0 || !pSnappingPlayer)
		return false;

	if(m_BotType == TYPE_BOT_QUEST)
		return true;

	if(m_BotType == TYPE_BOT_NPC)
	{
		const int GivesQuest = GS()->Mmo()->BotsData()->GetQuestNPC(m_MobID);
		if(NpcBotInfo::ms_aNpcBot[m_MobID].m_Function == FUNCTION_NPC_GIVE_QUEST && pSnappingPlayer->GetQuest(GivesQuest)->GetState() == QuestState::NO_ACCEPT)
			return true;

		return false;
	}
	return false;
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
		if(GS()->IsDungeon())
			return "Boss";
		return "Raid";
	}

	if(m_BotType == TYPE_BOT_QUEST_MOB)
	{
		return "Quest mob";
	}

	if(m_BotType == TYPE_BOT_EIDOLON && GetEidolonOwner())
	{
		int OwnerID = GetEidolonOwner()->GetCID();
		return Server()->ClientName(OwnerID);
	}

	switch(GetMoodState())
	{
		default:
		case Mood::NORMAL:
		return "\0";
		case Mood::FRIENDLY:
		return "Friendly";
		case Mood::QUEST:
		{
			const int QuestID = QuestBotInfo::ms_aQuestBot[m_MobID].m_QuestID;
			return GS()->GetQuestInfo(QuestID)->GetName();
		}
		case Mood::ANGRY:
		return "Angry";
		case Mood::AGRESSED:
		return "Aggressive";
	}
}

int CPlayerBot::GetPlayerWorldID() const
{
	if(m_BotType == TYPE_BOT_MOB)
		return MobBotInfo::ms_aMobBot[m_MobID].m_WorldID;
	if(m_BotType == TYPE_BOT_NPC)
		return NpcBotInfo::ms_aNpcBot[m_MobID].m_WorldID;
	if(m_BotType == TYPE_BOT_EIDOLON)
		return Server()->GetClientWorldID(m_MobID);
	if(m_BotType == TYPE_BOT_QUEST_MOB)
		return m_QuestMobInfo.m_WorldID;
	return QuestBotInfo::ms_aQuestBot[m_MobID].m_WorldID;
}

CTeeInfo& CPlayerBot::GetTeeInfo() const
{
	dbg_assert(DataBotInfo::IsDataBotValid(m_BotID), "Assert getter TeeInfo from data bot");
	return DataBotInfo::ms_aDataBot[m_BotID].m_TeeInfos;
}

void CPlayerBot::HandlePathFinder()
{
	if(!m_BotActive || !m_pCharacter || !m_pCharacter->IsAlive())
		return;

	// Check if the bot type is TYPE_BOT_MOB
	if(GetBotType() == TYPE_BOT_MOB)
	{
		// Check if the target position is not (0, 0) and if the current server tick modulo (3 * m_ClientID) is 0
		if(m_TargetPos != vec2(0, 0) && (Server()->Tick() + 3 * m_ClientID) % (Server()->TickSpeed()) == 0)
		{
			// Prepare the path finder data for default path finding
			GS()->PathFinder()->SyncHandler()->Prepare<CPathFinderPrepared::TYPE::DEFAULT>(&m_PathFinderData, m_ViewPos, m_TargetPos);
		}
		// If the target position is (0, 0) or the distance between the view position and the target position is less than 128.0f
		else if(m_TargetPos == vec2(0, 0) || distance(m_ViewPos, m_TargetPos) < 128.0f)
		{
			// Set the last position tick to the current server tick plus a random time interval
			m_LastPosTick = Server()->Tick() + (Server()->TickSpeed() * 2 + random_int() % 4);
			// Prepare the path finder data for random path finding
			GS()->PathFinder()->SyncHandler()->Prepare<CPathFinderPrepared::TYPE::RANDOM>(&m_PathFinderData, m_ViewPos, m_TargetPos);
		}
	}

	// Check if the bot type is TYPE_BOT_EIDOLON
	else if(GetBotType() == TYPE_BOT_EIDOLON)
	{
		// Get the owner ID of the bot
		int OwnerID = m_MobID;
		// Check if the owner player exists and if the target position is not (0, 0) and if the current server tick modulo (Server()->TickSpeed() / 3) is 0
		if(const CPlayer* pPlayerOwner = GS()->GetPlayer(OwnerID, true, true); pPlayerOwner && m_TargetPos != vec2(0, 0) && Server()->Tick() % (Server()->TickSpeed() / 3) == 0)
		{
			// Prepare the path finder data for default path finding
			GS()->PathFinder()->SyncHandler()->Prepare<CPathFinderPrepared::TYPE::DEFAULT>(&m_PathFinderData, m_ViewPos, m_TargetPos);
		}
	}

	// Check if the bot type is TYPE_BOT_QUEST_MOB
	else if(GetBotType() == TYPE_BOT_QUEST_MOB)
	{
		// Check if the target position is not (0, 0) and if the current server tick modulo (Server()->TickSpeed() / 3) is 0
		if(m_TargetPos != vec2(0, 0) && Server()->Tick() % (Server()->TickSpeed() / 3) == 0)
		{
			// Prepare the path finder data for default path finding
			GS()->PathFinder()->SyncHandler()->Prepare<CPathFinderPrepared::TYPE::DEFAULT>(&m_PathFinderData, m_ViewPos, m_TargetPos);
		}
	}

	// Check if the bot type is TYPE_BOT_NPC and the function is FUNCTION_NPC_GUARDIAN
	else if(GetBotType() == TYPE_BOT_NPC && NpcBotInfo::ms_aNpcBot[m_MobID].m_Function == FUNCTION_NPC_GUARDIAN)
	{
		// Check if the target position is not (0, 0) and if the current server tick modulo (Server()->TickSpeed() / 3) is 0
		if(m_TargetPos != vec2(0, 0) && Server()->Tick() % (Server()->TickSpeed() / 3) == 0)
		{
			// Prepare the path finder data for default path finding
			GS()->PathFinder()->SyncHandler()->Prepare<CPathFinderPrepared::TYPE::DEFAULT>(&m_PathFinderData, m_ViewPos, m_TargetPos);
		}
	}
}