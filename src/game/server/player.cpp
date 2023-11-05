/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "player.h"

#include "gamecontext.h"
#include "engine/shared/config.h"
#include "worldmodes/dungeon.h"

#include "mmocore/Components/Accounts/AccountManager.h"
#include "mmocore/Components/Bots/BotManager.h"
#include "mmocore/Components/Dungeons/DungeonData.h"
#include "mmocore/Components/Eidolons/EidolonInfoData.h"
#include "mmocore/Components/Guilds/GuildManager.h"
#include "mmocore/Components/Houses/HouseData.h"
#include "mmocore/Components/Quests/QuestManager.h"

#include "mmocore/Components/Inventory/ItemData.h"
#include "mmocore/Components/Skills/SkillData.h"

MACRO_ALLOC_POOL_ID_IMPL(CPlayer, MAX_CLIENTS* ENGINE_MAX_WORLDS + MAX_CLIENTS)

IServer* CPlayer::Server() const { return m_pGS->Server(); };
CPlayer::CPlayer(CGS* pGS, int ClientID) : m_pGS(pGS), m_ClientID(ClientID)
{
	for(short& SortTab : m_aSortTabs)
		SortTab = -1;

	m_EidolonCID = -1;
	m_Spawned = true;
	m_SnapHealthTick = 0;
	m_aPlayerTick[Respawn] = Server()->Tick() + Server()->TickSpeed();
	m_aPlayerTick[Die] = Server()->Tick();
	m_PrevTuningParams = *pGS->Tuning();
	m_NextTuningParams = m_PrevTuningParams;
	m_Relevation = 0;

	// constructor only for players
	if(m_ClientID < MAX_PLAYERS)
	{
		m_TutorialStep = 1;
		m_LastVoteMenu = NOPE;
		m_CurrentVoteMenu = MENU_MAIN;
		m_ZoneInvertMenu = false;
		m_MoodState = Mood::NORMAL;
		Acc().m_Team = GetStartTeam();
		GS()->SendTuningParams(ClientID);

		m_Afk = false;
		delete m_pLastInput;
		m_pLastInput = new CNetObj_PlayerInput({ 0 });
		m_LastInputInit = false;
		m_LastPlaytime = 0;
	}
}

CPlayer::~CPlayer()
{
	m_aHiddenMenu.clear();
	delete m_pLastInput;
	delete m_pCharacter;
	m_pCharacter = nullptr;
}

/* #########################################################################
	FUNCTIONS PLAYER ENGINE
######################################################################### */
void CPlayer::Tick()
{
	if(!IsAuthed())
		return;

	IServer::CClientInfo Info;
	if(Server()->GetClientInfo(m_ClientID, &Info))
	{
		m_Latency.m_AccumMax = max(m_Latency.m_AccumMax, Info.m_Latency);
		m_Latency.m_AccumMin = min(m_Latency.m_AccumMin, Info.m_Latency);
		Server()->SetClientScore(m_ClientID, Acc().m_Level);
	}

	if(Server()->Tick() % Server()->TickSpeed() == 0)
	{
		m_Latency.m_Max = m_Latency.m_AccumMax;
		m_Latency.m_Min = m_Latency.m_AccumMin;
		m_Latency.m_AccumMin = 1000;
		m_Latency.m_AccumMax = 0;
	}

	if(m_pCharacter)
	{
		if(m_pCharacter->IsAlive())
			m_ViewPos = m_pCharacter->GetPos();
		else
		{
			delete m_pCharacter;
			m_pCharacter = nullptr;
		}
	}
	else if(m_Spawned && m_aPlayerTick[Respawn] + Server()->TickSpeed() * 3 <= Server()->Tick())
	{
		TryRespawn();
	}

	// update dialog
	m_Dialog.TickUpdate();

	// post updated votes if player open menu
	if(m_PlayerFlags & PLAYERFLAG_IN_MENU && IsActivePostVoteList())
		PostVoteList();
}

void CPlayer::PostTick()
{
	// update latency value
	if(Server()->ClientIngame(m_ClientID) && IsAuthed())
		GetTempData().m_TempPing = m_Latency.m_Min;

	EffectsTick();
	HandleTuningParams();
}

CPlayerBot* CPlayer::GetEidolon() const
{
	if(m_EidolonCID < MAX_PLAYERS || m_EidolonCID >= MAX_CLIENTS)
		return nullptr;
	return dynamic_cast<CPlayerBot*>(GS()->m_apPlayers[m_EidolonCID]);
}

void CPlayer::TryCreateEidolon()
{
	if(IsBot() || !IsAuthed() || !GetCharacter())
		return;

	int EidolonItemID = GetEquippedItemID(EQUIP_EIDOLON);
	if(CEidolonInfoData* pEidolonData = GS()->GetEidolonByItemID(EidolonItemID))
	{
		if(const int EidolonCID = GS()->CreateBot(TYPE_BOT_EIDOLON, pEidolonData->GetDataBotID(), m_ClientID); EidolonCID != -1)
		{
			dynamic_cast<CPlayerBot*>(GS()->m_apPlayers[EidolonCID])->m_EidolonItemID = EidolonItemID;
			m_EidolonCID = EidolonCID;
		}
	}
}

void CPlayer::TryRemoveEidolon()
{
	if(IsBot())
		return;

	if(m_EidolonCID >= MAX_PLAYERS && m_EidolonCID < MAX_CLIENTS && GS()->m_apPlayers[m_EidolonCID])
	{
		if(GS()->m_apPlayers[m_EidolonCID]->GetCharacter())
			GS()->m_apPlayers[m_EidolonCID]->KillCharacter(WEAPON_WORLD);

		delete GS()->m_apPlayers[m_EidolonCID];
		GS()->m_apPlayers[m_EidolonCID] = nullptr;
	}

	m_EidolonCID = -1;
}


void CPlayer::EffectsTick()
{
	if(Server()->Tick() % Server()->TickSpeed() != 0 || CGS::ms_aEffects[m_ClientID].empty())
		return;

	for(auto pEffect = CGS::ms_aEffects[m_ClientID].begin(); pEffect != CGS::ms_aEffects[m_ClientID].end();)
	{
		pEffect->second--;
		if(pEffect->second <= 0)
		{
			GS()->Chat(m_ClientID, "You lost the {STR} effect.", pEffect->first.c_str());
			pEffect = CGS::ms_aEffects[m_ClientID].erase(pEffect);
			continue;
		}

		++pEffect;
	}
}

void CPlayer::HandleTuningParams()
{
	if(!(m_PrevTuningParams == m_NextTuningParams))
	{
		CMsgPacker Msg(NETMSGTYPE_SV_TUNEPARAMS);
		const int* pParams = reinterpret_cast<int*>(&m_NextTuningParams);
		for(unsigned i = 0; i < sizeof(m_NextTuningParams) / sizeof(int); i++)
		{
			Msg.AddInt(pParams[i]);
		}
		Server()->SendMsg(&Msg, MSGFLAG_VITAL, m_ClientID);
		m_PrevTuningParams = m_NextTuningParams;
	}

	m_NextTuningParams = *GS()->Tuning();
}

void CPlayer::Snap(int SnappingClient)
{
	CNetObj_ClientInfo* pClientInfo = static_cast<CNetObj_ClientInfo*>(Server()->SnapNewItem(NETOBJTYPE_CLIENTINFO, m_ClientID, sizeof(CNetObj_ClientInfo)));
	if(!pClientInfo)
		return;

	// Check if the player is not currently chatting and the server tick is less than the snapshot health tick
	if(!(m_PlayerFlags & PLAYERFLAG_CHATTING) && Server()->Tick() < m_SnapHealthTick)
	{
		const int PercentHP = translate_to_percent(GetStartHealth(), GetHealth());
		char aHealthProgressBuf[6];
		char aNicknameBuf[MAX_NAME_LENGTH];
		char aEndNicknameBuf[MAX_NAME_LENGTH];

		// Format the health progress string with the calculated percentage
		str_format(aHealthProgressBuf, sizeof(aHealthProgressBuf), ":%d%%", clamp(PercentHP, 1, 100));

		// Truncate the player's nickname to fit the available space, leaving room for the health progress string
		str_utf8_truncate(aNicknameBuf, sizeof(aNicknameBuf), Server()->ClientName(m_ClientID), (int)((MAX_NAME_LENGTH - 1) - str_length(aHealthProgressBuf)));

		// Concatenate the truncated nickname and the health progress string
		str_format(aEndNicknameBuf, sizeof(aEndNicknameBuf), "%s%s", aNicknameBuf, aHealthProgressBuf);

		// Convert the final nickname to integer values and store them in the client info structure
		StrToInts(&pClientInfo->m_Name0, 4, aEndNicknameBuf);
	}
	else
	{
		// Check if the player is authenticated and if the tick is a multiple of 10 seconds
		if(IsAuthed() && Server()->Tick() % (Server()->TickSpeed() * 10) == 0)
		{
			// Set the refresh tick for nickname leveling to be 1 second in the future
			m_aPlayerTick[RefreshNickLeveling] = Server()->Tick() + Server()->TickSpeed();
		}

		// Check if the player is authenticated and if the refresh tick for nickname leveling is in the future
		if(IsAuthed() && m_aPlayerTick[RefreshNickLeveling] > Server()->Tick())
		{
			// Create a buffer for the new nickname with the format "Level - X", where X is the player's level
			char aBufNicknameLeveling[MAX_NAME_LENGTH];
			str_format(aBufNicknameLeveling, sizeof(aBufNicknameLeveling), "Level - %d", Acc().m_Level);

			// Convert the new nickname to integer values and update the client info's m_Name0 field
			StrToInts(&pClientInfo->m_Name0, 4, aBufNicknameLeveling);
		}
		else
		{
			// Convert the default nickname to integer values and update the client info's m_Name0 field
			StrToInts(&pClientInfo->m_Name0, 4, Server()->ClientName(m_ClientID));
		}
	}

	// Check if it's time to refresh the clan title
	if(m_aPlayerTick[RefreshClanTitle] < Server()->Tick())
	{
		// Rotate the clan string by the length of the first character
		int clanStringSize = str_utf8_fix_truncation(m_aClanString);
		std::rotate(std::begin(m_aClanString), std::begin(m_aClanString) + str_utf8_forward(m_aClanString, 0), std::end(m_aClanString));

		// Set the next tick for refreshing the clan title
		m_aPlayerTick[RefreshClanTitle] = Server()->Tick() + (((m_aClanString[0] == '|') || (clanStringSize - 1 < 10)) ? Server()->TickSpeed() : (Server()->TickSpeed() / 6));

		// If the clan string size is less than 10
		if(clanStringSize < 10)
		{
			// Set the next tick for refreshing the clan title to current tick + 1 second
			m_aPlayerTick[RefreshClanTitle] = Server()->Tick() + Server()->TickSpeed();

			// Refresh the clan string
			RefreshClanString();
		}
	}

	StrToInts(&pClientInfo->m_Clan0, 3, m_aClanString);
	pClientInfo->m_Country = Server()->ClientCountry(m_ClientID);
	StrToInts(&pClientInfo->m_Skin0, 6, GetTeeInfo().m_aSkinName);
	pClientInfo->m_UseCustomColor = GetTeeInfo().m_UseCustomColor;
	pClientInfo->m_ColorBody = GetTeeInfo().m_ColorBody;
	pClientInfo->m_ColorFeet = GetTeeInfo().m_ColorFeet;

	CNetObj_PlayerInfo* pPlayerInfo = static_cast<CNetObj_PlayerInfo*>(Server()->SnapNewItem(NETOBJTYPE_PLAYERINFO, m_ClientID, sizeof(CNetObj_PlayerInfo)));
	if(!pPlayerInfo)
		return;

	const bool localClient = m_ClientID == SnappingClient;
	pPlayerInfo->m_Local = localClient;
	pPlayerInfo->m_ClientID = m_ClientID;
	pPlayerInfo->m_Team = GetTeam();
	pPlayerInfo->m_Latency = (SnappingClient == -1 ? m_Latency.m_Min : GetTempData().m_TempPing);
	pPlayerInfo->m_Score = Acc().m_Level;

	if(m_ClientID == SnappingClient && (GetTeam() == TEAM_SPECTATORS))
	{
		CNetObj_SpectatorInfo* pSpectatorInfo = static_cast<CNetObj_SpectatorInfo*>(Server()->SnapNewItem(NETOBJTYPE_SPECTATORINFO, m_ClientID, sizeof(CNetObj_SpectatorInfo)));
		if(!pSpectatorInfo)
			return;

		pSpectatorInfo->m_SpectatorID = -1;
		pSpectatorInfo->m_X = m_ViewPos.x;
		pSpectatorInfo->m_Y = m_ViewPos.y;
	}
}

void CPlayer::FakeSnap()
{
	int FakeID = VANILLA_MAX_CLIENTS - 1;
	CNetObj_ClientInfo* pClientInfo = static_cast<CNetObj_ClientInfo*>(Server()->SnapNewItem(NETOBJTYPE_CLIENTINFO, FakeID, sizeof(CNetObj_ClientInfo)));
	if(!pClientInfo)
		return;

	StrToInts(&pClientInfo->m_Name0, 4, " ");
	StrToInts(&pClientInfo->m_Clan0, 3, "");
	StrToInts(&pClientInfo->m_Skin0, 6, "default");

	CNetObj_PlayerInfo* pPlayerInfo = static_cast<CNetObj_PlayerInfo*>(Server()->SnapNewItem(NETOBJTYPE_PLAYERINFO, FakeID, sizeof(CNetObj_PlayerInfo)));
	if(!pPlayerInfo)
		return;

	pPlayerInfo->m_Latency = m_Latency.m_Min;
	pPlayerInfo->m_Local = 1;
	pPlayerInfo->m_ClientID = FakeID;
	pPlayerInfo->m_Score = -9999;
	pPlayerInfo->m_Team = TEAM_SPECTATORS;

	CNetObj_SpectatorInfo* pSpectatorInfo = static_cast<CNetObj_SpectatorInfo*>(Server()->SnapNewItem(NETOBJTYPE_SPECTATORINFO, FakeID, sizeof(CNetObj_SpectatorInfo)));
	if(!pSpectatorInfo)
		return;

	pSpectatorInfo->m_SpectatorID = -1;
	pSpectatorInfo->m_X = m_ViewPos.x;
	pSpectatorInfo->m_Y = m_ViewPos.y;
}

void CPlayer::RefreshClanString()
{
	if(!IsAuthed())
	{
		str_copy(m_aClanString, Server()->ClientClan(m_ClientID), sizeof(m_aClanString));
		return;
	}

	dynamic_string Buffer {};

	// location
	Buffer.append(Server()->GetWorldName(GetPlayerWorldID()));

	// guild
	if(Acc().IsGuild())
	{
		Buffer.append(" | ");
		Buffer.append(GS()->Mmo()->Member()->GuildName(Acc().m_GuildID));
	}

	// class
	const int AttributesByType[3] = { GetTypeAttributesSize(AttributeType::Tank),
										GetTypeAttributesSize(AttributeType::Healer), GetTypeAttributesSize(AttributeType::Dps) };

	int MaxAttributesPower = 0;
	AttributeType Class = AttributeType::Tank;
	for(int i = 0; i < 3; i++)
	{
		if(AttributesByType[i] > MaxAttributesPower)
		{
			MaxAttributesPower = AttributesByType[i];
			Class = static_cast<AttributeType>(i);
		}
	}

	const char* pClassName;
	switch(Class)
	{
		case AttributeType::Healer: pClassName = "_Healer_"; break;
		case AttributeType::Dps: pClassName = "_DPS_"; break;
		default: pClassName = "_Tank_"; break;
	}

	char aBufClass[64];
	str_format(aBufClass, sizeof(aBufClass), "%-*s | %dp", 10 - str_length(pClassName), pClassName, MaxAttributesPower);
	Buffer.append(" | ");
	Buffer.append(aBufClass);

	// end format
	str_format(m_aClanString, sizeof(m_aClanString), "%s", Buffer.buffer());

	Buffer.clear();
}

void CPlayer::PostVoteList()
{
	m_PostVotes();
	m_PostVotes = nullptr;
}

CCharacter* CPlayer::GetCharacter() const
{
	if(m_pCharacter && m_pCharacter->IsAlive())
		return m_pCharacter;
	return nullptr;
}

void CPlayer::TryRespawn()
{
	vec2 SpawnPos;
	int SpawnType = SPAWN_HUMAN;
	if(GetTempData().m_TempSafeSpawn)
	{
		const int SafezoneWorldID = GS()->GetRespawnWorld();
		if(SafezoneWorldID >= 0 && !GS()->IsPlayerEqualWorld(m_ClientID, SafezoneWorldID))
		{
			ChangeWorld(SafezoneWorldID);
			return;
		}

		SpawnType = SPAWN_HUMAN_SAFE;
	}

	if(GS()->m_pController->CanSpawn(SpawnType, &SpawnPos))
	{
		if(!GS()->IsDungeon() && total_size_vec2(GetTempData().m_TempTeleportPos) >= 1.0f)
		{
			SpawnPos = GetTempData().m_TempTeleportPos;
			GetTempData().m_TempTeleportPos = vec2(-1, -1);
		}

		const int AllocMemoryCell = MAX_CLIENTS * GS()->GetWorldID() + m_ClientID;
		m_pCharacter = new(AllocMemoryCell) CCharacter(&GS()->m_World);
		m_pCharacter->Spawn(this, SpawnPos);
		GS()->CreatePlayerSpawn(SpawnPos);
		m_Spawned = false;
	}
}

void CPlayer::KillCharacter(int Weapon)
{
	if(m_pCharacter)
	{
		m_pCharacter->Die(m_ClientID, Weapon);
		delete m_pCharacter;
		m_pCharacter = nullptr;
	}
}

void CPlayer::OnDisconnect()
{
	KillCharacter();
}

void CPlayer::OnDirectInput(CNetObj_PlayerInput* pNewInput)
{
	// update view pos
 	if(!m_pCharacter && GetTeam() == TEAM_SPECTATORS)
		m_ViewPos = vec2(pNewInput->m_TargetX, pNewInput->m_TargetY);

	// reset input with chating
	if(pNewInput->m_PlayerFlags & PLAYERFLAG_CHATTING)
	{
		// skip the input if chat is active
		if(m_PlayerFlags & PLAYERFLAG_CHATTING)
			return;

		// reset input
		if(m_pCharacter)
			m_pCharacter->ResetInput();

		m_PlayerFlags = pNewInput->m_PlayerFlags;
		return;
	}

	m_PlayerFlags = pNewInput->m_PlayerFlags;

	if(m_pCharacter)
	{
		// update afk time
		if(g_Config.m_SvMaxAfkTime != 0)
			m_Afk = (bool)(m_LastPlaytime < time_get() - time_freq() * g_Config.m_SvMaxAfkTime);

		m_pCharacter->OnDirectInput(pNewInput);
	}

	// check for activity
	if(mem_comp(pNewInput, m_pLastInput, sizeof(CNetObj_PlayerInput)))
	{
		mem_copy(m_pLastInput, pNewInput, sizeof(CNetObj_PlayerInput));
		// Ignore the first direct input and keep the player afk as it is sent automatically
		if(m_LastInputInit)
			m_LastPlaytime = time_get();

		m_LastInputInit = true;
	}
}

void CPlayer::OnPredictedInput(CNetObj_PlayerInput* pNewInput) const
{
	// skip the input if chat is active
	if((m_PlayerFlags & PLAYERFLAG_CHATTING) && (pNewInput->m_PlayerFlags & PLAYERFLAG_CHATTING))
		return;

	if(m_pCharacter)
		m_pCharacter->OnPredictedInput(pNewInput);
}

int CPlayer::GetTeam()
{
	if(GS()->Mmo()->Account()->IsActive(m_ClientID))
		return Acc().m_Team;
	return TEAM_SPECTATORS;
}

/* #########################################################################
	FUNCTIONS PLAYER HELPER
######################################################################### */
void CPlayer::ProgressBar(const char* Name, int MyLevel, int MyExp, int ExpNeed, int GivedExp) const
{
	char aBufBroadcast[128], aBufProgress[32];
	const float GetLevelProgress = translate_to_percent((float)ExpNeed, (float)MyExp);
	const float GetExpProgress = translate_to_percent((float)ExpNeed, (float)GivedExp);

	str_format_progress_bar(aBufProgress, sizeof(aBufProgress), 100, (int)GetLevelProgress, 10, ':', ' ');
	str_format(aBufBroadcast, sizeof(aBufBroadcast), "Lv%d %s%s %0.2f%%+%0.3f%%(%d)XP", MyLevel, Name, aBufProgress, GetLevelProgress, GetExpProgress, GivedExp);
	GS()->Broadcast(m_ClientID, BroadcastPriority::GAME_INFORMATION, 100, aBufBroadcast);
}

bool CPlayer::Upgrade(int Value, int* Upgrade, int* Useless, int Price, int MaximalUpgrade) const
{
	const int UpgradeNeed = Price * Value;
	if((*Upgrade + Value) > MaximalUpgrade)
	{
		GS()->Broadcast(m_ClientID, BroadcastPriority::GAME_WARNING, 100, "Upgrade has a maximum level.");
		return false;
	}

	if(*Useless < UpgradeNeed)
	{
		GS()->Broadcast(m_ClientID, BroadcastPriority::GAME_WARNING, 100, "Not upgrade points for +{INT}. Required {INT}.", Value, UpgradeNeed);
		return false;
	}

	*Useless -= UpgradeNeed;
	*Upgrade += Value;
	return true;
}

/* #########################################################################
	FUNCTIONS PLAYER ACCOUNT
######################################################################### */
bool CPlayer::SpendCurrency(int Price, int ItemID)
{
	if(Price <= 0)
		return true;

	CPlayerItem* pItem = GetItem(ItemID);
	if(pItem->GetValue() < Price)
	{
		GS()->Chat(m_ClientID, "Required {VAL}, but you have only {VAL} {STR}!", Price, pItem->GetValue(), pItem->Info()->GetName());
		return false;
	}
	return pItem->Remove(Price);
}

void CPlayer::GiveEffect(const char* Potion, int Sec, float Chance)
{
	if(m_pCharacter && m_pCharacter->IsAlive())
	{
		const float RandomChance = frandom() * 100.0f;
		if(RandomChance < Chance)
		{
			GS()->Chat(m_ClientID, "You got the effect {STR} time {INT} seconds.", Potion, Sec);
			CGS::ms_aEffects[m_ClientID][Potion] = Sec;
		}
	}
}

bool CPlayer::IsActiveEffect(const char* Potion) const
{
	return CGS::ms_aEffects[m_ClientID].count(Potion) > 0;
}

void CPlayer::ClearEffects()
{
	CGS::ms_aEffects[m_ClientID].clear();
}

const char* CPlayer::GetLanguage() const
{
	return Server()->GetClientLanguage(m_ClientID);
}

void CPlayer::UpdateTempData(int Health, int Mana)
{
	GetTempData().m_TempHealth = Health;
	GetTempData().m_TempMana = Mana;
}

void CPlayer::AddExp(int Exp)
{
	Acc().m_Exp += Exp;
	while(Acc().m_Exp >= ExpNeed(Acc().m_Level))
	{
		Acc().m_Exp -= ExpNeed(Acc().m_Level);
		Acc().m_Level++;
		Acc().m_Upgrade += 1;

		if(m_pCharacter)
		{
			GS()->CreateDeath(m_pCharacter->m_Core.m_Pos, m_ClientID);
			GS()->CreateSound(m_pCharacter->m_Core.m_Pos, 4);
			GS()->CreateText(m_pCharacter, false, vec2(0, -40), vec2(0, -1), 30, "level up");
		}

		GS()->Chat(m_ClientID, "Congratulations. You attain level {INT}!", Acc().m_Level);
		if(Acc().m_Exp < ExpNeed(Acc().m_Level))
		{
			GS()->StrongUpdateVotes(m_ClientID, MENU_MAIN);
			GS()->Mmo()->SaveAccount(this, SAVE_STATS);
			GS()->Mmo()->SaveAccount(this, SAVE_UPGRADES);
		}
	}
	ProgressBar("Account", Acc().m_Level, Acc().m_Exp, ExpNeed(Acc().m_Level), Exp);

	if(rand() % 5 == 0)
		GS()->Mmo()->SaveAccount(this, SAVE_STATS);

	if(Acc().IsGuild())
		GS()->Mmo()->Member()->AddExperience(Acc().m_GuildID);
}

void CPlayer::AddMoney(int Money)
{
	GetItem(itGold)->Add(Money);
}

bool CPlayer::GetHiddenMenu(int HideID) const
{
	if(m_aHiddenMenu.find(HideID) != m_aHiddenMenu.end())
		return m_aHiddenMenu.at(HideID);
	return false;
}

bool CPlayer::IsAuthed() const
{
	if(GS()->Mmo()->Account()->IsActive(m_ClientID))
		return Acc().m_ID > 0;
	return false;
}

int CPlayer::GetStartTeam() const
{
	if(IsAuthed())
		return TEAM_RED;
	return TEAM_SPECTATORS;
}

int CPlayer::ExpNeed(int Level)
{
	return computeExperience(Level);
}

int CPlayer::GetStartHealth()
{
	return 10 + GetAttributeSize(AttributeIdentifier::HP);
}

int CPlayer::GetStartMana()
{
	return 10 + GetAttributeSize(AttributeIdentifier::MP);
}

int64_t CPlayer::GetAfkTime() const
{
	return m_Afk ? ((time_get() - m_LastPlaytime) / time_freq()) - g_Config.m_SvMaxAfkTime : 0;
}

void CPlayer::FormatBroadcastBasicStats(char* pBuffer, int Size, const char* pAppendStr)
{
	if(!IsAuthed() || !m_pCharacter)
		return;

	char aBufProgressBarExp[32];
	const int LevelPercent = translate_to_percent(ExpNeed(Acc().m_Level), Acc().m_Exp);
	const int MaximumHealth = GetStartHealth();
	const int MaximumMana = GetStartMana();
	const int Health = m_pCharacter->Health();
	const int Mana = m_pCharacter->Mana();
	const int Gold = GetItem(itGold)->GetValue();

	char aRecastInfo[32] {};
	if(m_aPlayerTick[PotionRecast] > Server()->Tick())
	{
		int Seconds = max(0, (m_aPlayerTick[PotionRecast] - Server()->Tick()) / Server()->TickSpeed());
		str_format(aRecastInfo, sizeof(aRecastInfo), "Potion recast: %d", Seconds);
	}

	str_format_progress_bar(aBufProgressBarExp, sizeof(aBufProgressBarExp), 100, LevelPercent, 10, ':', ' ');
	str_format(pBuffer, Size, "\n\n\n\n\nLv%d%s\nHP %d/%d\nMP %d/%d\nGold %s\n%s\n\n\n\n\n\n\n\n\n\n\n%s",
		Acc().m_Level, aBufProgressBarExp, Health, MaximumHealth, Mana, MaximumMana, get_commas<int>(Gold).c_str(), aRecastInfo, pAppendStr);
	for(int space = 150, c = str_length(pBuffer); c < Size && space; c++, space--)
		pBuffer[c] = ' ';
}

void CPlayer::ShowInformationStats()
{
	GS()->Broadcast(m_ClientID, BroadcastPriority::GAME_BASIC_STATS, 100, "");
}

/* #########################################################################
	FUNCTIONS PLAYER PARSING
######################################################################### */
bool CPlayer::ParseItemsF3F4(int Vote)
{
	if(!m_pCharacter)
	{
		GS()->Chat(m_ClientID, "Use it when you're not dead!");
		return true;
	}

	// - - - - - F3- - - - - - -
	if(Vote == 1)
	{
		if(m_RequestChangeNickname)
		{
			if(GS()->Mmo()->Account()->ChangeNickname(m_ClientID))
				GS()->Broadcast(m_ClientID, BroadcastPriority::VERY_IMPORTANT, 300, "Your nickname has been successfully updated");
			else
				GS()->Broadcast(m_ClientID, BroadcastPriority::VERY_IMPORTANT, 300, "This nickname is already in use");

			m_RequestChangeNickname = false;
			return true;
		}

		if(GS()->IsDungeon())
		{
			const int DungeonID = GS()->GetDungeonID();
			if(!CDungeonData::ms_aDungeon[DungeonID].IsDungeonPlaying())
			{
				GetTempData().m_TempDungeonReady ^= true;
				GS()->Chat(m_ClientID, "You changed the ready mode to \"{STR}\"!", GetTempData().m_TempDungeonReady ? "ready" : "not ready");
			}
			return true;
		}

	}
	// - - - - - F4- - - - - - -
	else
	{
		// conversations
		if(m_Dialog.IsActive())
		{
			if(m_aPlayerTick[LastDialog] && m_aPlayerTick[LastDialog] > GS()->Server()->Tick())
				return true;

			m_aPlayerTick[LastDialog] = GS()->Server()->Tick() + (GS()->Server()->TickSpeed() / 4);
			GS()->CreatePlayerSound(m_ClientID, SOUND_PICKUP_ARMOR);
			m_Dialog.Next();
			return true;
		}
	}
	return false;
}
// vote parsing and improving statistics
bool CPlayer::ParseVoteUpgrades(const char* CMD, const int VoteID, const int VoteID2, int Get)
{
	if(PPSTR(CMD, "UPGRADE") == 0)
	{
		if(Upgrade(Get, &Acc().m_aStats[(AttributeIdentifier)VoteID], &Acc().m_Upgrade, VoteID2, 1000))
		{
			GS()->Mmo()->SaveAccount(this, SAVE_UPGRADES);
			GS()->UpdateVotes(m_ClientID, MENU_UPGRADES);
		}
		return true;
	}

	if(PPSTR(CMD, "BACK") == 0)
	{
		// close other tabs after checked new
		for(auto& [ID, Value] : m_aHiddenMenu)
		{
			if(ID > NUM_TAB_MENU)
				Value = false;
		}

		GS()->UpdateVotes(m_ClientID, m_LastVoteMenu);
		return true;
	}

	if(PPSTR(CMD, "HIDDEN") == 0)
	{
		if(VoteID < TAB_STAT)
			return true;

		// close other tabs after checked new
		for(auto& [ID, Value] : m_aHiddenMenu)
		{
			if((ID > NUM_TAB_MENU && VoteID > NUM_TAB_MENU && ID != VoteID))
				Value = false;
		}

		m_aHiddenMenu[VoteID] ^= true;
		if(m_aHiddenMenu[VoteID] == false)
			m_aHiddenMenu.erase(VoteID);

		GS()->StrongUpdateVotes(m_ClientID, m_CurrentVoteMenu);
		return true;
	}
	return false;
}

CPlayerItem* CPlayer::GetItem(ItemIdentifier ID)
{
	dbg_assert(CItemDescription::Data().find(ID) != CItemDescription::Data().end(), "invalid referring to the CPlayerItem");

	if(CPlayerItem::Data()[m_ClientID].find(ID) == CPlayerItem::Data()[m_ClientID].end())
	{
		CPlayerItem(ID, m_ClientID).Init({}, {}, {}, {});
		return &CPlayerItem::Data()[m_ClientID][ID];
	}

	return &CPlayerItem::Data()[m_ClientID][ID];
}

CSkill* CPlayer::GetSkill(SkillIdentifier ID)
{
	dbg_assert(CSkillDescription::Data().find(ID) != CSkillDescription::Data().end(), "invalid referring to the CSkillData");

	if(CSkill::Data()[m_ClientID].find(ID) == CSkill::Data()[m_ClientID].end())
	{
		CSkill(ID, m_ClientID).Init({}, {});
		return &CSkill::Data()[m_ClientID][ID];
	}

	return &CSkill::Data()[m_ClientID][ID];
}

CPlayerQuest* CPlayer::GetQuest(QuestIdentifier ID)
{
	dbg_assert(CQuestDescription::Data().find(ID) != CQuestDescription::Data().end(), "invalid referring to the CPlayerQuest");

	if(CPlayerQuest::Data()[m_ClientID].find(ID) == CPlayerQuest::Data()[m_ClientID].end())
	{
		CPlayerQuest(ID, m_ClientID).Init({});
		return &CPlayerQuest::Data()[m_ClientID][ID];
	}

	return &CPlayerQuest::Data()[m_ClientID][ID];
}

int CPlayer::GetEquippedItemID(ItemFunctional EquipID, int SkipItemID) const
{
	const auto Iter = std::find_if(CPlayerItem::Data()[m_ClientID].begin(), CPlayerItem::Data()[m_ClientID].end(), [EquipID, SkipItemID](const auto& p)
	{
		return (p.second.HasItem() && p.second.IsEquipped() && p.second.Info()->IsFunctional(EquipID) && p.first != SkipItemID);
	});
	return Iter != CPlayerItem::Data()[m_ClientID].end() ? Iter->first : -1;
}

int CPlayer::GetAttributeSize(AttributeIdentifier ID)
{
	// if the best tank class is selected among the players we return the sync dungeon stats
	const CAttributeDescription* pAtt = GS()->GetAttributeInfo(ID);
	if(GS()->IsDungeon() && pAtt->GetUpgradePrice() < 4 && CDungeonData::ms_aDungeon[GS()->GetDungeonID()].IsDungeonPlaying())
	{
		const CGameControllerDungeon* pDungeon = dynamic_cast<CGameControllerDungeon*>(GS()->m_pController);
		return pDungeon->GetAttributeDungeonSync(this, ID);
	}

	// get all attributes from items
	int Size = 0;
	for(const auto& [ItemID, ItemData] : CPlayerItem::Data()[m_ClientID])
	{
		if(ItemData.IsEquipped() && ItemData.Info()->IsEnchantable() && ItemData.Info()->GetInfoEnchantStats(ID))
		{
			Size += ItemData.GetEnchantStats(ID);
		}
	}

	// if the attribute has the value of player upgrades we sum up
	if(pAtt->HasDatabaseField())
		Size += Acc().m_aStats[ID];

	return Size;
}

float CPlayer::GetAttributePercent(AttributeIdentifier ID)
{
	float Percent = 0.0f;
	int Size = GetAttributeSize(ID);

	if(ID == AttributeIdentifier::Vampirism)
		Percent = min(8.0f + (float)Size * 0.0015f, 30.0f);
	if(ID == AttributeIdentifier::Crit)
		Percent = min(8.0f + (float)Size * 0.0015f, 30.0f);
	if(ID == AttributeIdentifier::Lucky)
		Percent = min(5.0f + (float)Size * 0.0015f, 20.0f);
	return Percent;
}

int CPlayer::GetTypeAttributesSize(AttributeType Type)
{
	int Size = 0;
	for(const auto& [ID, pAttribute] : CAttributeDescription::Data())
	{
		if(pAttribute->IsType(Type))
			Size += GetAttributeSize(ID);
	}
	return Size;
}

int CPlayer::GetAttributesSize()
{
	int Size = 0;
	for(const auto& [ID, Attribute] : CAttributeDescription::Data())
		Size += GetAttributeSize(ID);

	return Size;
}

void CPlayer::SetSnapHealthTick(int Sec)
{
	m_SnapHealthTick = Server()->Tick() + (Server()->TickSpeed() * Sec);
}

void CPlayer::ChangeWorld(int WorldID)
{
	// reset dungeon temp data
	GetTempData().m_TempAlreadyVotedDungeon = false;
	GetTempData().m_TempDungeonReady = false;
	GetTempData().m_TempTankVotingDungeon = 0;
	GetTempData().m_TempTimeDungeon = 0;

	// change worlds
	Acc().m_aHistoryWorld.push_front(WorldID);
	Server()->ChangeWorld(m_ClientID, WorldID);
}

int CPlayer::GetPlayerWorldID() const
{
	return Server()->GetClientWorldID(m_ClientID);
}

CTeeInfo& CPlayer::GetTeeInfo() const
{
	return Acc().m_TeeInfos;
}
