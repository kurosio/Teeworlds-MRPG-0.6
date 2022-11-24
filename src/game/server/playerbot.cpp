/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "playerbot.h"

#include "gamecontext.h"
#include "mmocore/PathFinder.h"

#include "entities/botai/character_bot_ai.h"
#include "gamemodes/dungeon.h"

#include "mmocore/Components/Bots/BotCore.h"

MACRO_ALLOC_POOL_ID_IMPL(CPlayerBot, MAX_CLIENTS * ENGINE_MAX_WORLDS + MAX_CLIENTS)

CPlayerBot::CPlayerBot(CGS *pGS, int ClientID, int BotID, int SubBotID, int SpawnPoint)
	: CPlayer(pGS, ClientID), m_BotType(SpawnPoint), m_BotID(BotID), m_MobID(SubBotID), m_BotHealth(0), m_LastPosTick(0), m_PathSize(0)
{
	m_EidolonCID = -1;
	m_OldTargetPos = vec2(0, 0);
	m_DungeonAllowedSpawn = false;
	m_BotStartHealth = m_BotType == TYPE_BOT_MOB ? CPlayerBot::GetAttributeSize(AttributeIdentifier::Hardness) : 10;
}

CPlayerBot::~CPlayerBot()
{
	for(int i = 0; i < MAX_PLAYERS; i++)
		DataBotInfo::ms_aDataBot[m_BotID].m_aVisibleActive[i] = false;

	delete m_pCharacter;
	m_pCharacter = nullptr;
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
			ThreadMobsPathFinder();
		}
		else
		{
			ClearWayPoint();
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
	if(Server()->Tick() % Server()->TickSpeed() != 0)
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
	switch(m_BotType)
	{
	case TYPE_BOT_QUEST:
	case TYPE_BOT_NPC:
	case TYPE_BOT_EIDOLON:
		return m_aPlayerTick[Respawn];
	default: 
		return m_aPlayerTick[Respawn] + Server()->TickSpeed() * 3;
	}
}

int CPlayerBot::GetAttributeSize(AttributeIdentifier ID, bool WorkedSize)
{
	if(m_BotType != TYPE_BOT_MOB && m_BotType != TYPE_BOT_EIDOLON)
		return 10;

	auto CalculateAttribute = [ID, this](int Power, int Spread, int HardtypeDividing) -> int
	{
		// get stats from the bot's equipment
		int Size = Power;
		for(unsigned i = 0; i < NUM_EQUIPPED; i++)
		{
			const int ItemID = GetEquippedItemID((ItemFunctional)i);
			if(ItemID > 0)
				Size += GS()->GetItemInfo(ItemID)->GetInfoEnchantStats(ID);
		}

		// spread weapons
		if(ID == AttributeIdentifier::SpreadShotgun || ID == AttributeIdentifier::SpreadGrenade || ID == AttributeIdentifier::SpreadRifle)
			Size = Spread;

		// all attribute stats without hardness
		else if(const CAttributeDescription* pAtt = GS()->GetAttributeInfo(ID); ID != AttributeIdentifier::Hardness && pAtt->GetDividing() > 0)
		{
			Size /= pAtt->GetDividing();
			if(pAtt->GetType() == AttributeType::Hardtype)
				Size /= HardtypeDividing;
		}

		return Size;
	};

	int Size = 0;
	if(m_BotType == TYPE_BOT_EIDOLON)
	{
		if(GS()->IsDungeon())
			Size = CalculateAttribute(translate_to_percent_rest(max(1, dynamic_cast<CGameControllerDungeon*>(GS()->m_pController)->GetSyncFactor()), 5), 1, 5);
		else
			Size = CalculateAttribute(EidolonsTools::getEidolonItemID(m_BotID), 1, 5);
	}
	else if(m_BotType == TYPE_BOT_MOB)
		Size = CalculateAttribute(MobBotInfo::ms_aMobBot[m_MobID].m_Power, MobBotInfo::ms_aMobBot[m_MobID].m_Spread, MobBotInfo::ms_aMobBot[m_MobID].m_Boss ? 30 : 2);
	return Size;
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
		if(!GS()->m_pController->CanSpawn(m_BotType, &SpawnPos, MobRespawnPosition))
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
		if(pSnappingPlayer->GetQuest(QuestID).GetState() != QuestState::ACCEPT)
			return 0;

		if((QuestBotInfo::ms_aQuestBot[m_MobID].m_Step != pSnappingPlayer->GetQuest(QuestID).m_Step) || pSnappingPlayer->GetQuest(QuestID).m_StepsQuestBot[GetBotMobID()].m_StepComplete)
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
	if(!IsVisibleForClient(SnappingClient) || !Server()->Translate(ID, SnappingClient))
		return;

	CNetObj_ClientInfo* pClientInfo = static_cast<CNetObj_ClientInfo*>(Server()->SnapNewItem(NETOBJTYPE_CLIENTINFO, ID, sizeof(CNetObj_ClientInfo)));
	if (!pClientInfo)
		return;

	if(GetBotType() == TYPE_BOT_MOB)
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
	if (!pPlayerInfo)
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
	if(GetBotType() == TYPE_BOT_MOB)
	{
		CCharacterBotAI *pChr = (CCharacterBotAI *)m_pCharacter;
		if(pChr && pChr->GetBotTarget() != m_ClientID)
			return Mood::AGRESSED;

		return Mood::ANGRY;
	}

	if(GetBotType() == TYPE_BOT_NPC)
		return Mood::FRIENDLY;

	if(GetBotType() == TYPE_BOT_EIDOLON)
		return Mood::FRIENDLY;

	if(GetBotType() == TYPE_BOT_QUEST)
		return Mood::QUEST;
	return Mood::NORMAL;
}

int CPlayerBot::GetBotLevel() const
{
	return (m_BotType == TYPE_BOT_MOB ? MobBotInfo::ms_aMobBot[m_MobID].m_Level : 1);
}

bool CPlayerBot::IsActiveQuests(int SnapClientID) const
{
	CPlayer* pSnappingPlayer = GS()->m_apPlayers[SnapClientID];
	if (SnapClientID >= MAX_PLAYERS || SnapClientID < 0 || !pSnappingPlayer)
		return false;

	if (m_BotType == TYPE_BOT_QUEST)
		return true;

	if(m_BotType == TYPE_BOT_NPC)
	{
		const int GivesQuest = GS()->Mmo()->BotsData()->GetQuestNPC(m_MobID);
		if(NpcBotInfo::ms_aNpcBot[m_MobID].m_Function == FUNCTION_NPC_GIVE_QUEST && pSnappingPlayer->GetQuest(GivesQuest).GetState() == QuestState::NO_ACCEPT)
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
	if (m_BotType == TYPE_BOT_MOB && MobBotInfo::ms_aMobBot[m_MobID].m_Boss)
	{
		if (GS()->IsDungeon())
			return "Boss";
		return "Raid";
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
			return GS()->GetQuestInfo(QuestID).GetName();
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
	return QuestBotInfo::ms_aQuestBot[m_MobID].m_WorldID;
}

CTeeInfo& CPlayerBot::GetTeeInfo() const
{
	dbg_assert(DataBotInfo::IsDataBotValid(m_BotID), "Assert getter TeeInfo from data bot");
	return DataBotInfo::ms_aDataBot[m_BotID].m_TeeInfos;
}

void CPlayerBot::FindThreadPath(CGS* pGameServer, CPlayerBot* pBotPlayer, vec2 StartPos, vec2 SearchPos)
{
	if(!pGameServer || !pBotPlayer || length(StartPos) <= 0 || length(SearchPos) <= 0)
		return;

	mtxThreadPathWritedNow.lock();

	while(pBotPlayer->m_ThreadReadNow.load(std::memory_order_acquire))
		std::this_thread::sleep_for(std::chrono::microseconds(1));

	pBotPlayer->m_OldTargetPos = pBotPlayer->m_TargetPos;

	pGameServer->PathFinder()->Init();
	pGameServer->PathFinder()->SetStart(StartPos);
	pGameServer->PathFinder()->SetEnd(SearchPos);
	pGameServer->PathFinder()->FindPath();
	pBotPlayer->m_PathSize = pGameServer->PathFinder()->m_FinalSize;
	for(int i = pBotPlayer->m_PathSize - 1, j = 0; i >= 0; i--, j++)
		pBotPlayer->m_WayPoints[j] = vec2(pGameServer->PathFinder()->m_lFinalPath[i].m_Pos.x * 32 + 16, pGameServer->PathFinder()->m_lFinalPath[i].m_Pos.y * 32 + 16);

	mtxThreadPathWritedNow.unlock();
}

void CPlayerBot::GetThreadRandomRadiusWaypointTarget(CGS* pGameServer, CPlayerBot* pBotPlayer, vec2 Pos, float Radius)
{
	if(!pGameServer || !pBotPlayer || length(Pos) <= 0)
		return;

	mtxThreadPathWritedNow.lock();

	while(pBotPlayer->m_ThreadReadNow.load(std::memory_order_acquire))
		std::this_thread::sleep_for(std::chrono::microseconds(1));

	pBotPlayer->m_OldTargetPos = pBotPlayer->m_TargetPos;

	const vec2 TargetPos = pGameServer->PathFinder()->GetRandomWaypointRadius(Pos, Radius);
	pBotPlayer->m_TargetPos = vec2(TargetPos.x * 32, TargetPos.y * 32);

	mtxThreadPathWritedNow.unlock();
}

void CPlayerBot::ThreadMobsPathFinder()
{
	if(!m_pCharacter || !m_pCharacter->IsAlive() || (m_TargetPos != vec2(0,0) && distance(m_TargetPos, m_OldTargetPos) < 48.0f))
		return;

	if(GetBotType() == TYPE_BOT_MOB)
	{
		if(m_TargetPos != vec2(0, 0) && (Server()->Tick() + 3 * m_ClientID) % (Server()->TickSpeed()) == 0)
		{
			std::thread(&FindThreadPath, GS(), this, m_ViewPos, m_TargetPos).detach();
		}
		else if(m_TargetPos == vec2(0, 0) || distance(m_ViewPos, m_TargetPos) < 128.0f)
		{
			m_LastPosTick = Server()->Tick() + (Server()->TickSpeed() * 2 + random_int() % 4);
			std::thread(&GetThreadRandomRadiusWaypointTarget, GS(), this, m_ViewPos, 800.0f).detach();
		}
	}

	else if(GetBotType() == TYPE_BOT_EIDOLON)
	{
		int OwnerID = m_MobID;
		if(const CPlayer* pPlayerOwner = GS()->GetPlayer(OwnerID, true, true); pPlayerOwner && m_TargetPos != vec2(0, 0) && Server()->Tick() % (Server()->TickSpeed() / 3) == 0)
		{
			std::thread(&FindThreadPath, GS(), this, m_ViewPos, m_TargetPos).detach();
		}
	}
}

void CPlayerBot::ClearWayPoint()
{
	if(!m_WayPoints.empty())
	{
		bool Status = false;
		if(!m_ThreadReadNow.compare_exchange_strong(Status, true, std::memory_order::memory_order_acquire, std::memory_order::memory_order_relaxed))
			return;

		m_WayPoints.clear();
		m_ThreadReadNow.store(false, std::memory_order::memory_order_release);
	}
}