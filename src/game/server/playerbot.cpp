/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "playerbot.h"
#include "gamecontext.h"

#include "entities/character_bot.h"
#include "worldmodes/dungeon.h"

#include "core/components/Bots/BotManager.h"
#include "core/tools/path_finder.h"

MACRO_ALLOC_POOL_ID_IMPL(CPlayerBot, MAX_CLIENTS* ENGINE_MAX_WORLDS + MAX_CLIENTS)

CPlayerBot::CPlayerBot(CGS* pGS, int ClientID, int BotID, int MobID, int SpawnPoint)
	: CPlayer(pGS, ClientID), m_BotType(SpawnPoint), m_BotID(BotID), m_MobID(MobID)
{
	m_OldTargetPos = vec2(0, 0);
	m_AllowedSpawn = true;
	m_Items.reserve(CItemDescription::Data().size());

	CPlayerBot::PrepareRespawnTick();
}

CPlayerBot::~CPlayerBot()
{
	std::memset(DataBotInfo::ms_aDataBot[m_BotID].m_aActiveByQuest, 0, MAX_PLAYERS * sizeof(bool));
}

void CPlayerBot::InitQuestBotMobInfo(CQuestBotMobInfo elem)
{
	if(m_BotType == TYPE_BOT_QUEST_MOB)
	{
		m_QuestMobInfo = elem;
		std::memset(m_QuestMobInfo.m_ActiveForClient, 0, MAX_PLAYERS * sizeof(bool));
	}
}

void CPlayerBot::InitBasicStats(int StartHP, int StartMP, int MaxHP, int MaxMP)
{
	m_Health = StartHP;
	m_Mana = StartMP;
	m_MaxHealth = MaxHP;
	m_MaxMana = MaxMP;
}

void CPlayerBot::Tick()
{
	if(m_pCharacter)
	{
		if(m_pCharacter->IsAlive())
		{
			if(m_BotType == TYPE_BOT_EIDOLON)
			{
				const auto* pOwner = GetEidolonOwner();
				if(pOwner && pOwner->GetCharacter() && distance(pOwner->GetCharacter()->GetPos(), m_ViewPos) > 1000.f)
				{
					vec2 OwnerPos = pOwner->GetCharacter()->GetPos();
					m_pCharacter->ResetDoorHit();
					m_pCharacter->ChangePosition(OwnerPos);

					m_pCharacter->m_Core.m_Vel = pOwner->GetCharacter()->m_Core.m_Vel;
					m_pCharacter->m_Core.m_Direction = pOwner->GetCharacter()->m_Core.m_Direction;
					m_pCharacter->m_Core.m_Input = pOwner->GetCharacter()->m_Core.m_Input;
				}
			}

			if(IsActive())
			{
				m_ViewPos = m_pCharacter->GetPos();
				HandlePathFinder();
			}
		}
		else
		{
			delete m_pCharacter;
			m_pCharacter = nullptr;
		}
	}
	else if(m_WantSpawn && m_aPlayerTick[Respawn] <= Server()->Tick())
	{
		TryRespawn();
	}

	// update events
	m_Scenarios.Tick();
}

void CPlayerBot::PostTick()
{
	HandleTuningParams();
	m_Effects.PostTick();
	m_Scenarios.PostTick();
}

CPlayer* CPlayerBot::GetEidolonOwner() const
{
	if(m_MobID < 0 || m_MobID >= MAX_PLAYERS)
		return nullptr;

	if(m_BotType != TYPE_BOT_EIDOLON)
		return nullptr;

	return GS()->GetPlayer(m_MobID);
}

CPlayerItem* CPlayerBot::GetItem(ItemIdentifier ID)
{
	dbg_assert(CItemDescription::Data().find(ID) != CItemDescription::Data().end(), "invalid referring to the CPlayerItem (from playerbot.h)");

	auto it = m_Items.find(ID);

	if(it == m_Items.end())
	{
		if(DataBotInfo::ms_aDataBot[m_BotID].m_EquippedModules.hasSet(std::to_string(ID)))
		{
			it = m_Items.emplace(ID, std::make_unique<CPlayerItem>(ID, m_ClientID, 1, 0, 100, 1)).first;
		}
		else
		{
			it = m_Items.emplace(ID, std::make_unique<CPlayerItem>(ID, m_ClientID, 0, 0, 0, 0)).first;
		}
	}

	return it->second.get();
}

bool CPlayerBot::IsActive() const
{
	return GS()->m_World.IsBotActive(m_ClientID);
}

bool CPlayerBot::IsConversational() const
{
	const auto* pChar = dynamic_cast<CCharacterBotAI*>(m_pCharacter);
	return IsActive() && pChar && pChar->AI()->IsConversational();
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
	else if(m_BotType == TYPE_BOT_EIDOLON)
	{
		m_DisabledBotDamage = false;
		m_aPlayerTick[Respawn] = Server()->Tick();
	}
	else if(m_BotType == TYPE_BOT_QUEST || m_BotType == TYPE_BOT_NPC)
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

int CPlayerBot::GetTotalAttributeValue(AttributeIdentifier ID) const
{
	auto CalculateAttribute = [ID, this](int Power, bool Boss) -> int
	{
		// get stats from the bot's equipment
		int AttributeValue = Power;
		for(unsigned i = 0; i < (unsigned)ItemType::NUM_EQUIPPED; i++)
		{
			if(const auto ItemID = GetEquippedItemID((ItemType)i))
			{
				AttributeValue += GS()->GetItemInfo(ItemID.value())->GetInfoEnchantStats(ID);
			}
		}

		// sync power mobs
		float Percent = 100.0f;
		const auto* pAttribute = GS()->GetAttributeInfo(ID);

		if(pAttribute->IsGroup(AttributeGroup::DamageType))
		{
			Percent = 5.0f;
		}
		else if(pAttribute->IsGroup(AttributeGroup::Dps))
		{
			Percent = 15.0f;
		}
		else if(pAttribute->IsGroup(AttributeGroup::Healer))
		{
			Percent = 20.0f;
		}

		// downcast for unhardness boss
		if(Boss && ID != AttributeIdentifier::HP)
		{
			Percent /= 10.0f;
		}

		const int SyncPercentSize = maximum(1, translate_to_percent_rest(AttributeValue, Percent));
		return SyncPercentSize;
	};


	// initiallize attributeValue by bot type
	int AttributeValue;
	if(m_BotType == TYPE_BOT_EIDOLON)
	{
		if(GS()->IsWorldType(WorldType::Dungeon))
		{
			const int SyncFactor = dynamic_cast<CGameControllerDungeon*>(GS()->m_pController)->GetSyncFactor();
			AttributeValue = CalculateAttribute(translate_to_percent_rest(maximum(1, SyncFactor), 5), false);
		}
		else
		{
			const auto* pOwner = GetEidolonOwner();
			AttributeValue = CalculateAttribute(pOwner ? pOwner->GetTotalAttributeValue(AttributeIdentifier::EidolonPWR) : 0, false);
		}
	}
	else if(m_BotType == TYPE_BOT_MOB)
	{
		const int PowerMob = MobBotInfo::ms_aMobBot[m_MobID].m_Power;
		const bool IsBoss = MobBotInfo::ms_aMobBot[m_MobID].m_Boss;

		AttributeValue = CalculateAttribute(PowerMob, IsBoss);
	}
	else if(m_BotType == TYPE_BOT_QUEST_MOB)
	{
		const int PowerQuestMob = m_QuestMobInfo.m_AttributePower;

		AttributeValue = CalculateAttribute(PowerQuestMob, true);
	}
	else if(m_BotType == TYPE_BOT_NPC)
	{
		AttributeValue = CalculateAttribute(10, true);
	}
	else
	{
		AttributeValue = 10;
	}

	return AttributeValue;
}

void CPlayerBot::TryRespawn()
{
	if(!m_AllowedSpawn)
		return;

	std::optional<vec2> FinalSpawnPos = std::nullopt;

	if(m_BotType == TYPE_BOT_MOB)
	{
		vec2 SpawnPos;
		const auto RespawnPosition = MobBotInfo::ms_aMobBot[m_MobID].m_Position;
		const auto Radius = MobBotInfo::ms_aMobBot[m_MobID].m_Radius;

		if(GS()->m_pController->CanSpawn(m_BotType, &SpawnPos, std::make_pair(RespawnPosition, Radius)))
			FinalSpawnPos = SpawnPos;
	}
	else if(m_BotType == TYPE_BOT_NPC)
	{
		FinalSpawnPos = NpcBotInfo::ms_aNpcBot[m_MobID].m_Position;
	}
	else if(m_BotType == TYPE_BOT_QUEST)
	{
		FinalSpawnPos = QuestBotInfo::ms_aQuestBot[m_MobID].m_Position;
	}
	else if(m_BotType == TYPE_BOT_EIDOLON)
	{
		auto* pOwner = GetEidolonOwner();
		if(!pOwner || !pOwner->GetCharacter())
			return;

		auto* pOwnerItem = pOwner->GetItem(pOwner->GetEquippedItemID(ItemType::EquipEidolon).value());
		if(pOwnerItem->GetDurability() <= 0)
			return;

		FinalSpawnPos = pOwner->GetCharacter()->GetPos();
	}
	else if(m_BotType == TYPE_BOT_QUEST_MOB)
	{
		FinalSpawnPos = GetQuestBotMobInfo().m_Position;
	}

	// create character
	if(FinalSpawnPos.has_value())
	{
		const int AllocMemoryCell = m_ClientID + GS()->GetWorldID() * MAX_CLIENTS;
		m_pCharacter = new(AllocMemoryCell) CCharacterBotAI(&GS()->m_World);
		m_pCharacter->Spawn(this, *FinalSpawnPos);
		GS()->CreatePlayerSpawn(*FinalSpawnPos, GetMaskVisibleForClients());
	}
}

int64_t CPlayerBot::GetMaskVisibleForClients() const
{
	int64_t Mask = 0;
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		if(IsActiveForClient(i))
			Mask |= CmaskOne(i);
	}

	return Mask;
}

ESnappingPriority CPlayerBot::IsActiveForClient(int ClientID) const
{
	CPlayer* pSnappingPlayer = GS()->GetPlayer(ClientID);
	if(ClientID < 0 || ClientID >= MAX_PLAYERS || !pSnappingPlayer)
		return SNAPPING_PRIORITY_NONE;

	if(m_BotType == TYPE_BOT_QUEST)
	{
		// is quest not accept
		const auto QuestID = QuestBotInfo::ms_aQuestBot[m_MobID].m_QuestID;
		if(pSnappingPlayer->GetQuest(QuestID)->GetState() != QuestState::Accepted)
			return SNAPPING_PRIORITY_NONE;

		// is step pos not equal current step pos
		if((QuestBotInfo::ms_aQuestBot[m_MobID].m_StepPos != pSnappingPlayer->GetQuest(QuestID)->GetStepPos()))
			return SNAPPING_PRIORITY_NONE;

		// is step pos completed
		if(pSnappingPlayer->GetQuest(QuestID)->GetStepByMob(GetBotMobID())->m_StepComplete)
			return SNAPPING_PRIORITY_NONE;
	}

	if(m_BotType == TYPE_BOT_NPC)
	{
		const auto FunctionNPC = NpcBotInfo::ms_aNpcBot[m_MobID].m_Function;

		// always show guardian and nurse
		if(FunctionNPC == FUNCTION_NPC_GUARDIAN || FunctionNPC == FUNCTION_NPC_NURSE)
			return SNAPPING_PRIORITY_HIGH;

		// does not show npc what active by quest
		const auto ActiveByQuest = DataBotInfo::ms_aDataBot[m_BotID].m_aActiveByQuest[ClientID];
		if(ActiveByQuest)
			return SNAPPING_PRIORITY_NONE;

		// is active or finished quest show only character
		const int GivesQuest = GS()->Core()->BotManager()->GetQuestNPC(m_MobID);
		if(FunctionNPC == FUNCTION_NPC_GIVE_QUEST && pSnappingPlayer->GetQuest(GivesQuest)->GetState() != QuestState::NoAccepted)
			return SNAPPING_PRIORITY_LOWER;
	}

	return SNAPPING_PRIORITY_HIGH;
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
		const float Progress = translate_to_percent((float)GetMaxHealth(), (float)GetHealth());

		std::string ProgressBar = mystd::string::progressBar(100, (int)Progress, 33, "\u25B0", "\u25B1");
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
	pPlayerInfo->m_ClientId = ID;
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
		return Mood::Normal;

	if(GetBotType() == TYPE_BOT_MOB && !pChar->AI()->GetTarget()->IsEmpty())
		return Mood::Angry;

	if(GetBotType() == TYPE_BOT_QUEST_MOB && !pChar->AI()->GetTarget()->IsEmpty())
		return Mood::Angry;

	if(GetBotType() == TYPE_BOT_EIDOLON)
		return Mood::Friendly;

	if(GetBotType() == TYPE_BOT_QUEST)
		return Mood::Quest;

	if(GetBotType() == TYPE_BOT_NPC)
	{
		bool IsGuardian = NpcBotInfo::ms_aNpcBot[m_MobID].m_Function == FUNCTION_NPC_GUARDIAN;
		if(IsGuardian && !pChar->AI()->GetTarget()->IsEmpty())
			return Mood::Agressed;

		return Mood::Friendly;
	}

	return Mood::Normal;
}

int CPlayerBot::GetLevel() const
{
	return (m_BotType == TYPE_BOT_MOB ? MobBotInfo::ms_aMobBot[m_MobID].m_Level : 1);
}

void CPlayerBot::GetFormatedName(char* aBuffer, int BufferSize)
{
	if(m_BotType == TYPE_BOT_MOB || m_BotType == TYPE_BOT_QUEST_MOB || (m_BotType == TYPE_BOT_NPC && NpcBotInfo::ms_aNpcBot[m_MobID].m_Function == FUNCTION_NPC_GUARDIAN))
	{
		const int PercentHP = translate_to_percent(GetMaxHealth(), GetHealth());
		str_format(aBuffer, BufferSize, "%s:%d%%", DataBotInfo::ms_aDataBot[m_BotID].m_aNameBot, clamp(PercentHP, 1, 100));
	}
	else
	{
		str_format(aBuffer, BufferSize, "%s", DataBotInfo::ms_aDataBot[m_BotID].m_aNameBot);
	}
}

std::optional<int> CPlayerBot::GetEquippedItemID(ItemType EquipID, int SkipItemID) const
{
	auto& DataBot = DataBotInfo::ms_aDataBot[m_BotID];
	if(DataBot.m_vEquippedSlot.contains(EquipID))
	{
		int itemID = DataBot.m_vEquippedSlot[EquipID];
		return (itemID > 0) ? std::make_optional(itemID) : std::nullopt;
	}

	return std::nullopt;
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

	const Mood State = GetMoodState();
	if(State == Mood::Quest)
	{
		const int QuestID = QuestBotInfo::ms_aQuestBot[m_MobID].m_QuestID;
		return GS()->GetQuestInfo(QuestID)->GetName();
	}

	return GetMoodName(State);
}

int CPlayerBot::GetCurrentWorldID() const
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

const CTeeInfo& CPlayerBot::GetTeeInfo() const
{
	dbg_assert(DataBotInfo::IsDataBotValid(m_BotID), "Assert getter TeeInfo from data bot");
	return DataBotInfo::ms_aDataBot[m_BotID].m_TeeInfos;
}

void CPlayerBot::HandlePathFinder()
{
	const auto* pChar = dynamic_cast<CCharacterBotAI*>(m_pCharacter);
	if(!IsActive() || !pChar || !pChar->IsAlive() || Server()->Tick() % 5 != 0)
		return;

	if(GetBotType() == TYPE_BOT_MOB)
	{
		if(m_TargetPos.has_value())
		{
			GS()->PathFinder()->RequestPath(m_PathHandle, pChar->m_Core.m_Pos, m_TargetPos.value());
		}
		else if(m_LastPosTick < Server()->Tick())
		{
			m_LastPosTick = Server()->Tick() + (Server()->TickSpeed() * 2 + rand() % 4);
			GS()->PathFinder()->RequestRandomPath(m_PathHandle, pChar->m_Core.m_Pos, 800.f);
		}
	}

	// Check if the bot type is TYPE_BOT_EIDOLON
	else if(GetBotType() == TYPE_BOT_EIDOLON)
	{
		int OwnerID = m_MobID;
		const auto* pPlayerOwner = GS()->GetPlayer(OwnerID, true, true);

		if(pPlayerOwner && m_TargetPos.has_value())
		{
			GS()->PathFinder()->RequestPath(m_PathHandle, pChar->m_Core.m_Pos, m_TargetPos.value());
		}
	}

	// Check if the bot type is TYPE_BOT_QUEST_MOB
	else if(GetBotType() == TYPE_BOT_QUEST_MOB)
	{
		if(m_TargetPos.has_value())
		{
			GS()->PathFinder()->RequestPath(m_PathHandle, pChar->m_Core.m_Pos, m_TargetPos.value());
		}
	}

	// Check if the bot type is TYPE_BOT_NPC and the function is FUNCTION_NPC_GUARDIAN
	else if(GetBotType() == TYPE_BOT_NPC && NpcBotInfo::ms_aNpcBot[m_MobID].m_Function == FUNCTION_NPC_GUARDIAN)
	{
		if(m_TargetPos.has_value())
		{
			GS()->PathFinder()->RequestPath(m_PathHandle, pChar->m_Core.m_Pos, m_TargetPos.value());
		}
	}
}