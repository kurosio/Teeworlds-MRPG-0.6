/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "player.h"

#include "gamecontext.h"
#include "gamemodes/dungeon.h"

#include "mmocore/Components/Accounts/AccountCore.h"
#include "mmocore/Components/Bots/BotCore.h"
#include "mmocore/Components/Dungeons/DungeonData.h"
#include "mmocore/Components/Guilds/GuildCore.h"
#include "mmocore/Components/Quests/QuestCore.h"

#include "mmocore/Components/Inventory/ItemData.h"
#include "mmocore/Components/Skills/SkillData.h"

MACRO_ALLOC_POOL_ID_IMPL(CPlayer, MAX_CLIENTS * ENGINE_MAX_WORLDS + MAX_CLIENTS)

IServer* CPlayer::Server() const { return m_pGS->Server(); };
CPlayer::CPlayer(CGS *pGS, int ClientID) : m_pGS(pGS), m_ClientID(ClientID)
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

	// constructor only for players
	if(m_ClientID < MAX_PLAYERS)
	{
		int* pIdMap = Server()->GetIdMap(m_ClientID);
		memset(pIdMap, -1, sizeof(int) * VANILLA_MAX_CLIENTS);
		pIdMap[0] = m_ClientID;

		m_LastVoteMenu = NOPE;
		m_OpenVoteMenu = MENU_MAIN;
		m_MoodState = Mood::NORMAL;
		Acc().m_Team = GetStartTeam();
		GS()->SendTuningParams(ClientID);
		ClearTalking();
	}
}

CPlayer::~CPlayer()
{
	m_aHiddenMenu.clear();
	delete m_pCharacter;
	m_pCharacter = nullptr;
}

/* #########################################################################
	FUNCTIONS PLAYER ENGINE
######################################################################### */
void CPlayer::Tick()
{
	if(!m_pCharacter && GetTeam() == TEAM_SPECTATORS)
	{
		m_ViewPos -= vec2(clamp(m_ViewPos.x - m_LatestActivity.m_TargetX, -500.0f, 500.0f), clamp(m_ViewPos.y - m_LatestActivity.m_TargetY, -400.0f, 400.0f));
	}

	if(!IsAuthed())
		return;

	Server()->SetClientScore(m_ClientID, Acc().m_Level);
	{
		IServer::CClientInfo Info;
		if (Server()->GetClientInfo(m_ClientID, &Info))
		{
			m_Latency.m_AccumMax = max(m_Latency.m_AccumMax, Info.m_Latency);
			m_Latency.m_AccumMin = min(m_Latency.m_AccumMin, Info.m_Latency);
		}

		if (Server()->Tick() % Server()->TickSpeed() == 0)
		{
			m_Latency.m_Max = m_Latency.m_AccumMax;
			m_Latency.m_Min = m_Latency.m_AccumMin;
			m_Latency.m_AccumMin = 1000;
			m_Latency.m_AccumMax = 0;
		}
	}

	if (m_pCharacter)
	{
		if(m_pCharacter->IsAlive())
		{
			m_ViewPos = m_pCharacter->GetPos();
		}
		else
		{
			delete m_pCharacter;
			m_pCharacter = nullptr;
		}
	}
	else if (m_Spawned && m_aPlayerTick[Respawn] + Server()->TickSpeed() * 3 <= Server()->Tick())
	{
		TryRespawn();
	}

	// update player tick
	TickSystemTalk();
}

void CPlayer::PostTick()
{
	// update latency value
	if (Server()->ClientIngame(m_ClientID) && IsAuthed())
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

	mtxThreadPathWritedNow.lock();

	if(int EidolonItemID = GetEquippedItemID(EQUIP_EIDOLON); EidolonItemID > 0 && EidolonsTools::getEidolonBot(EidolonItemID) > 0)
	{
		const int EidolonCID = GS()->CreateBot(TYPE_BOT_EIDOLON, EidolonsTools::getEidolonBot(EidolonItemID), m_ClientID);
		m_EidolonCID = EidolonCID;
	}

	mtxThreadPathWritedNow.unlock();
}

void CPlayer::TryRemoveEidolon()
{
	if(IsBot())
		return;

	mtxThreadPathWritedNow.lock();

	if(m_EidolonCID >= MAX_PLAYERS && m_EidolonCID < MAX_CLIENTS && GS()->m_apPlayers[m_EidolonCID])
	{
		if(GS()->m_apPlayers[m_EidolonCID]->GetCharacter())
			GS()->m_apPlayers[m_EidolonCID]->KillCharacter(WEAPON_WORLD);

		delete GS()->m_apPlayers[m_EidolonCID];
		GS()->m_apPlayers[m_EidolonCID] = nullptr;
	}

	m_EidolonCID = -1;

	mtxThreadPathWritedNow.unlock();
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
			GS()->Chat(m_ClientID, "You lost the effect {STR}.", pEffect->first.c_str());
			pEffect = CGS::ms_aEffects[m_ClientID].erase(pEffect);
			continue;
		}

		++pEffect;
	}
}

void CPlayer::TickSystemTalk()
{
	if(const int TalkedID = m_DialogNPC.m_TalkedID; TalkedID != -1 && TalkedID != m_ClientID)
	{
		if(!m_pCharacter || TalkedID < MAX_PLAYERS || !GS()->m_apPlayers[TalkedID] || distance(m_ViewPos, GS()->m_apPlayers[TalkedID]->m_ViewPos) > 180.0f)
			ClearTalking();
	}
}

void CPlayer::HandleTuningParams()
{
	if(!(m_PrevTuningParams == m_NextTuningParams))
	{
		CMsgPacker Msg(NETMSGTYPE_SV_TUNEPARAMS);
		const int *pParams = reinterpret_cast<int*>(&m_NextTuningParams);
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
	if (!pClientInfo)
		return;

	if(!(m_PlayerFlags & PLAYERFLAG_CHATTING) && Server()->Tick() < m_SnapHealthTick)
	{
		const int PercentHP = translate_to_percent(GetStartHealth(), GetHealth());

		char aHealthProgressBuf[6];
		char aNicknameBuf[MAX_NAME_LENGTH];
		char aEndNicknameBuf[MAX_NAME_LENGTH];
		str_format(aHealthProgressBuf, sizeof(aHealthProgressBuf), ":%d%%", clamp(PercentHP, 1, 100));
		str_utf8_truncate(aNicknameBuf, sizeof(aNicknameBuf), Server()->ClientName(m_ClientID), (int)((MAX_NAME_LENGTH - 1) - str_length(aHealthProgressBuf)));
		str_format(aEndNicknameBuf, sizeof(aEndNicknameBuf), "%s%s", aNicknameBuf, aHealthProgressBuf);
		StrToInts(&pClientInfo->m_Name0, 4, aEndNicknameBuf);
	}
	else
	{
		StrToInts(&pClientInfo->m_Name0, 4, Server()->ClientName(m_ClientID));
	}

	StrToInts(&pClientInfo->m_Clan0, 3, GetStatus());
	pClientInfo->m_Country = Server()->ClientCountry(m_ClientID);
	StrToInts(&pClientInfo->m_Skin0, 6, GetTeeInfo().m_aSkinName);
	pClientInfo->m_UseCustomColor = GetTeeInfo().m_UseCustomColor;
	pClientInfo->m_ColorBody = GetTeeInfo().m_ColorBody;
	pClientInfo->m_ColorFeet = GetTeeInfo().m_ColorFeet;

	CNetObj_PlayerInfo* pPlayerInfo = static_cast<CNetObj_PlayerInfo*>(Server()->SnapNewItem(NETOBJTYPE_PLAYERINFO, m_ClientID, sizeof(CNetObj_PlayerInfo)));
	if (!pPlayerInfo)
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
		if (!pSpectatorInfo)
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

CCharacter *CPlayer::GetCharacter() const
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

	if(GS()->m_pController->CanSpawn(SpawnType, &SpawnPos, vec2(-1, -1)))
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

void CPlayer::OnDirectInput(CNetObj_PlayerInput *NewInput)
{
	if(NewInput->m_PlayerFlags&PLAYERFLAG_CHATTING)
	{
		// skip the input if chat is active
		if(m_PlayerFlags&PLAYERFLAG_CHATTING)
			return;

		// reset input
		if(m_pCharacter)
			m_pCharacter->ResetInput();

		m_PlayerFlags = NewInput->m_PlayerFlags;
		return;
	}

	m_PlayerFlags = NewInput->m_PlayerFlags;

	if(m_pCharacter)
		m_pCharacter->OnDirectInput(NewInput);

	// check for activity
	if(NewInput->m_Direction || m_LatestActivity.m_TargetX != NewInput->m_TargetX ||
		m_LatestActivity.m_TargetY != NewInput->m_TargetY || NewInput->m_Jump ||
		NewInput->m_Fire&1 || NewInput->m_Hook)
	{
		m_LatestActivity.m_TargetX = NewInput->m_TargetX;
		m_LatestActivity.m_TargetY = NewInput->m_TargetY;
	}
}

void CPlayer::OnPredictedInput(CNetObj_PlayerInput *NewInput) const
{
	// skip the input if chat is active
	if((m_PlayerFlags&PLAYERFLAG_CHATTING) && (NewInput->m_PlayerFlags&PLAYERFLAG_CHATTING))
		return;

	if(m_pCharacter)
		m_pCharacter->OnPredictedInput(NewInput);
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
void CPlayer::ProgressBar(const char *Name, int MyLevel, int MyExp, int ExpNeed, int GivedExp) const
{
	char aBufBroadcast[128];
	const float GetLevelProgress = (float)(MyExp * 100.0) / (float)ExpNeed;
	const float GetExpProgress = (float)(GivedExp * 100.0) / (float)ExpNeed;
	const std::unique_ptr<char[]> Level = std::move(GS()->LevelString(100, (int)GetLevelProgress, 10, ':', ' '));
	str_format(aBufBroadcast, sizeof(aBufBroadcast), "Lv%d %s%s %0.2f%%+%0.3f%%(%d)XP", MyLevel, Name, Level.get(), GetLevelProgress, GetExpProgress, GivedExp);
	GS()->Broadcast(m_ClientID, BroadcastPriority::GAME_INFORMATION, 100, aBufBroadcast);
}

bool CPlayer::Upgrade(int Value, int *Upgrade, int *Useless, int Price, int MaximalUpgrade) const
{
	const int UpgradeNeed = Price*Value;
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
	if (Price <= 0)
		return true;

	CPlayerItem* pItem = GetItem(ItemID);
	if(pItem->GetValue() < Price)
	{
		GS()->Chat(m_ClientID,"Required {VAL}, but you have only {VAL} {STR}!", Price, pItem->GetValue(), pItem->Info()->GetName());
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
			GS()->Chat(m_ClientID, "You got the effect {STR} time {INT}sec.", Potion, Sec);
			CGS::ms_aEffects[m_ClientID][Potion] = Sec;
		}
	}
}

bool CPlayer::IsActiveEffect(const char* Potion) const
{
	return CGS::ms_aEffects[m_ClientID].find(Potion) != CGS::ms_aEffects[m_ClientID].end();
}

void CPlayer::ClearEffects()
{
	CGS::ms_aEffects[m_ClientID].clear();
}

const char *CPlayer::GetLanguage() const
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
		Acc().m_Exp -= ExpNeed(Acc().m_Level), Acc().m_Level++;
		Acc().m_Upgrade += 10;

		GS()->CreateDeath(m_pCharacter->m_Core.m_Pos, m_ClientID);
		GS()->CreateSound(m_pCharacter->m_Core.m_Pos, 4);
		GS()->CreateText(m_pCharacter, false, vec2(0, -40), vec2(0, -1), 30, "level");
		GS()->Chat(m_ClientID, "Congratulations. Level UP. Now Level {INT}!", Acc().m_Level);
		if(Acc().m_Exp < ExpNeed(Acc().m_Level))
		{
			GS()->StrongUpdateVotes(m_ClientID, MENU_MAIN);
			GS()->Mmo()->SaveAccount(this, SAVE_STATS);
			GS()->Mmo()->SaveAccount(this, SAVE_UPGRADES);
		}
	}
	ProgressBar("Account", Acc().m_Level, Acc().m_Exp, ExpNeed(Acc().m_Level), Exp);

	if (rand() % 5 == 0)
		GS()->Mmo()->SaveAccount(this, SAVE_STATS);

	if (Acc().IsGuild())
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
		return Acc().m_UserID > 0;
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
	return 10 + GetAttributeSize(AttributeIdentifier::Hardness, true);
}

int CPlayer::GetStartMana()
{
	const int EnchantBonus = GetAttributeSize(AttributeIdentifier::Piety, true);
	return 10 + EnchantBonus;
}

void CPlayer::FormatBroadcastBasicStats(char *pBuffer, int Size, const char* pAppendStr)
{
	if(!IsAuthed() || !m_pCharacter)
		return;

	const int LevelPercent = translate_to_percent(ExpNeed(Acc().m_Level), Acc().m_Exp);
	const std::unique_ptr<char[]> Level = std::move(GS()->LevelString(100, (int)LevelPercent, 10, ':', ' '));
	const int MaximumHealth = GetStartHealth();
	const int MaximumMana = GetStartMana();
	const int Health = m_pCharacter->Health();
	const int Mana = m_pCharacter->Mana();
	const int Gold = GetItem(itGold)->GetValue();

	char aRecastInfo[32]{};
	if(m_aPlayerTick[PotionRecast] > Server()->Tick())
	{
		int Seconds = max(0, (m_aPlayerTick[PotionRecast] - Server()->Tick()) / Server()->TickSpeed());
		str_format(aRecastInfo, sizeof(aRecastInfo), "Potion recast: %d", Seconds);
	}

	str_format(pBuffer, Size, "\n\n\n\n\nLv%d%s\nHP %d/%d\nMP %d/%d\nGold %d\n%s\n\n\n\n\n\n\n\n\n\n\n%s", 
		Acc().m_Level, Level.get(), Health, MaximumHealth, Mana, MaximumMana, Gold, aRecastInfo, pAppendStr);
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
	if (!m_pCharacter)
	{
		GS()->Chat(m_ClientID, "Use it when you're not dead!");
		return true;
	}

	// - - - - - F3- - - - - - -
	if (Vote == 1)
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
				GS()->Chat(m_ClientID, "You change the ready mode to {STR}!", GetTempData().m_TempDungeonReady ? "ready" : "not ready");
			}
			return true;
		}

	}
	// - - - - - F4- - - - - - -
	else
	{
		// conversations
		if(GetTalkedID() > 0)
		{
			if(m_aPlayerTick[LastDialog] && m_aPlayerTick[LastDialog] > GS()->Server()->Tick())
				return true;

			m_aPlayerTick[LastDialog] = GS()->Server()->Tick() + (GS()->Server()->TickSpeed() / 4);
			GS()->CreatePlayerSound(m_ClientID, SOUND_PICKUP_ARMOR);
			SetTalking(GetTalkedID(), false);
			return true;
		}
	}
	return false;
}
// vote parsing and improving statistics
bool CPlayer::ParseVoteUpgrades(const char *CMD, const int VoteID, const int VoteID2, int Get)
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

		GS()->StrongUpdateVotes(m_ClientID, m_OpenVoteMenu);
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
		CSkill(ID, m_ClientID).Init({},{});
		return &CSkill::Data()[m_ClientID][ID];
	}

	return &CSkill::Data()[m_ClientID][ID];
}

CQuestData& CPlayer::GetQuest(int QuestID)
{
	CQuestData::ms_aPlayerQuests[m_ClientID][QuestID].m_QuestID = QuestID;
	CQuestData::ms_aPlayerQuests[m_ClientID][QuestID].m_pPlayer = this;
	return CQuestData::ms_aPlayerQuests[m_ClientID][QuestID];
}

int CPlayer::GetEquippedItemID(ItemFunctional EquipID, int SkipItemID) const
{
	const auto Iter = std::find_if(CPlayerItem::Data()[m_ClientID].begin(), CPlayerItem::Data()[m_ClientID].end(), [EquipID, SkipItemID](const auto& p)
	{
		return (p.second.HasItem() && p.second.IsEquipped() && p.second.Info()->IsFunctional(EquipID) && p.first != SkipItemID); 
	});
	return Iter != CPlayerItem::Data()[m_ClientID].end() ? Iter->first : -1;
}

int CPlayer::GetAttributeSize(AttributeIdentifier ID, bool WorkedSize)
{
	// if the best tank class is selected among the players we return the sync dungeon stats
	const CAttributeDescription* pAtt = GS()->GetAttributeInfo(ID);
	if(GS()->IsDungeon() && pAtt->GetUpgradePrice() < 10 && CDungeonData::ms_aDungeon[GS()->GetDungeonID()].IsDungeonPlaying())
	{
		const CGameControllerDungeon* pDungeon = dynamic_cast<CGameControllerDungeon*>(GS()->m_pController);
		return pDungeon->GetAttributeDungeonSync(this, ID);
	}

	// get all attributes from items
	int Size = 0;
	for(const auto& [ItemID, ItemData] : CPlayerItem::Data()[m_ClientID])
	{
		if(ItemData.IsEquipped() && ItemData.Info()->IsEnchantable() && ItemData.Info()->GetInfoEnchantStats(ID))
			Size += ItemData.GetEnchantStats(ID);
	}

	// if the attribute has the value of player upgrades we sum up
	if (pAtt->HasField())
		Size += Acc().m_aStats[ID];

	// to the final active attribute stats for the player
	if (WorkedSize && pAtt->GetDividing() > 0)
		Size /= pAtt->GetDividing();

	return Size;
}

int CPlayer::GetTypeAttributesSize(AttributeType Type)
{
	int Size = 0;
	for (const auto& [ID, pAttribute] : CAttributeDescription::Data())
	{
		if (pAttribute->IsType(Type))
			Size += GetAttributeSize(ID, true);
	}
	return Size;
}

int CPlayer::GetAttributesSize()
{
	int Size = 0;
	for(const auto& [ID, Attribute] : CAttributeDescription::Data())
		Size += GetAttributeSize(ID, true);

	return Size;
}

void CPlayer::SetSnapHealthTick(int Sec)
{
	m_SnapHealthTick = Server()->Tick() + (Server()->TickSpeed() * Sec);
}

// - - - - - - T A L K I N G - - - - B O T S - - - - - - - - -
void CPlayer::SetTalking(int TalkedID, bool IsStartDialogue)
{
	if (TalkedID < MAX_PLAYERS || !GetCharacter() || !GS()->m_apPlayers[TalkedID] || (!IsStartDialogue && m_DialogNPC.m_TalkedID == -1))
		return;

	m_DialogNPC.m_TalkedID = TalkedID;
	if(IsStartDialogue)
	{
		m_DialogNPC.m_FreezedProgress = false;
		m_DialogNPC.m_Progress = 0;
	}

	CPlayerBot* pBotPlayer = static_cast<CPlayerBot*>(GS()->m_apPlayers[TalkedID]);
	const int MobID = pBotPlayer->GetBotMobID();
	if (pBotPlayer->GetBotType() == TYPE_BOT_NPC)
	{
		// clearing the end of dialogs or a dialog that was meaningless
		const int sizeTalking = NpcBotInfo::ms_aNpcBot[MobID].m_aDialogs.size();
		const bool isTalkingEmpty = NpcBotInfo::ms_aNpcBot[MobID].m_aDialogs.empty();
		if ((isTalkingEmpty && m_DialogNPC.m_Progress == IS_TALKING_EMPTY) || (!isTalkingEmpty && m_DialogNPC.m_Progress >= sizeTalking))
		{
			ClearTalking();
			return;
		}

		// you get to know in general if the quest is to give out a random senseless dialog
		int GivingQuestID = GS()->Mmo()->BotsData()->GetQuestNPC(MobID);
		if (isTalkingEmpty || GetQuest(GivingQuestID).GetState() >= QuestState::ACCEPT)
		{
			const char* pMeaninglessDialog = GS()->Mmo()->BotsData()->GetMeaninglessDialog();
			GS()->Mmo()->BotsData()->DialogBotStepNPC(this, MobID, -1, pMeaninglessDialog);
			m_DialogNPC.m_Progress = IS_TALKING_EMPTY;
			return;
		}

		// get a quest for the progress of dialogue if it is in this progress we accept the quest
		GivingQuestID = NpcBotInfo::ms_aNpcBot[MobID].m_GiveQuestID;
		bool FinalStep = ((m_DialogNPC.m_Progress + 1) >= sizeTalking);
		if (FinalStep && GivingQuestID >= 1)
		{
			if(!m_DialogNPC.m_FreezedProgress)
			{
				GS()->Mmo()->BotsData()->DialogBotStepNPC(this, MobID, m_DialogNPC.m_Progress);
				m_DialogNPC.m_FreezedProgress = true;
				return;
			}

			GetQuest(GivingQuestID).Accept();
			m_DialogNPC.m_Progress++;
		}

		GS()->Mmo()->BotsData()->DialogBotStepNPC(this, MobID, m_DialogNPC.m_Progress);
	}

	else if (pBotPlayer->GetBotType() == TYPE_BOT_QUEST)
	{
		const int SizeDialogs = QuestBotInfo::ms_aQuestBot[MobID].m_aDialogs.size();
		if(m_DialogNPC.m_Progress >= SizeDialogs)
		{
			ClearTalking();
			return;
		}

		//  after
		if(m_DialogNPC.m_FreezedProgress)
		{
			// check collect step progression
			if(!GS()->Mmo()->Quest()->InteractiveQuestNPC(this, QuestBotInfo::ms_aQuestBot[MobID], false))
			{
				GS()->Mmo()->BotsData()->DialogBotStepQuest(this, MobID, m_DialogNPC.m_Progress, true);
				return;
			}

			// final step
			m_DialogNPC.m_Progress++;
			if(m_DialogNPC.m_Progress >= SizeDialogs)
			{
				GS()->Mmo()->Quest()->InteractiveQuestNPC(this, QuestBotInfo::ms_aQuestBot[MobID], true);
				ClearTalking();

				JsonTools::parseFromString(QuestBotInfo::ms_aQuestBot[MobID].m_EventJsonData, [this](nlohmann::json& pJson)
				{
					/* * * * * * * * *
					 * Teleport event
					 * * * * * * * * */
					if(pJson.find("teleport") != pJson.end())
					{
						auto pTeleport = pJson["teleport"];
						vec2 Position(pTeleport.value("x", -1.0f), pTeleport.value("y", -1.0f));

						if(pTeleport.find("world_id") != pTeleport.end() && GetPlayerWorldID() != pTeleport.value("world_id", MAIN_WORLD_ID))
						{
							GetTempData().m_TempTeleportPos = Position;
							ChangeWorld(pTeleport.value("world_id", MAIN_WORLD_ID));
							return;
						}

						GetCharacter()->ChangePosition(Position);
					}
				});
				return;
			}

			GS()->Mmo()->BotsData()->DialogBotStepQuest(this, MobID, m_DialogNPC.m_Progress, false);
			return;
		}

		// before
		const bool RequestAction = QuestBotInfo::ms_aQuestBot[MobID].m_aDialogs[m_DialogNPC.m_Progress].IsRequestAction();
		if(RequestAction && !m_DialogNPC.m_FreezedProgress)
		{
			GS()->Mmo()->Quest()->DoStepDropTakeItems(this, QuestBotInfo::ms_aQuestBot[MobID]);
			GS()->Mmo()->BotsData()->DialogBotStepQuest(this, MobID, m_DialogNPC.m_Progress, true);
			m_DialogNPC.m_RequestProgress = m_DialogNPC.m_Progress;
			m_DialogNPC.m_FreezedProgress = true;

			JsonTools::parseFromString(QuestBotInfo::ms_aQuestBot[MobID].m_EventJsonData, [this](nlohmann::json& pJson)
			{
				/* * * * * * * *
				 * Chat event
				 * * * * * * * */
				if(pJson.find("chat") != pJson.end())
				{
					bool Highlighting = false;
					for(auto& p : pJson["chat"])
					{
						std::string Text = p.value("text", "\0");
						if(p.find("broadcast") != p.end() && p.value("broadcast", 0))
							GS()->Broadcast(m_ClientID, BroadcastPriority::MAIN_INFORMATION, 300, Text.c_str());
						else
						{
							if(!Highlighting)
							{
								GS()->Chat(m_ClientID, "*****************************");
								Highlighting = true;
							}
							GS()->Chat(m_ClientID, Text.c_str());
						}
					}

					if(Highlighting)
						GS()->Chat(m_ClientID, "*****************************");
				}

				/* * * * * * * *
				 * Effect event
				 * * * * * * * */
				if(pJson.find("effect") != pJson.end())
				{
					auto pEffect = pJson["effect"];
					std::string Effect = pEffect.value("name", "\0");
					int Seconds = pEffect.value("seconds", 0);

					if(!Effect.empty())
						GiveEffect(Effect.c_str(), Seconds);
				}
			});

			return;
		}

		GS()->Mmo()->BotsData()->DialogBotStepQuest(this, MobID, m_DialogNPC.m_Progress, false);
	}

	m_DialogNPC.m_Progress++;
}

void CPlayer::ClearTalking()
{
	m_DialogNPC.m_TalkedID = -1;
	m_DialogNPC.m_Progress = 0;
	m_DialogNPC.m_FreezedProgress = false;

	GS()->Motd(m_ClientID, "\0");
}

// - - - - - - F O R M A T - - - - - T E X T - - - - - - - - -
const char *CPlayer::GetDialogText() const
{
	return m_aFormatDialogText;
}

void CPlayer::FormatDialogText(int DataBotID, const char *pText) // TODO: perform profiling and debugging to check the performance
{
	if(!DataBotInfo::IsDataBotValid(DataBotID) || m_aFormatDialogText[0] != '\0')
		return;

	str_copy(m_aFormatDialogText, GS()->Server()->Localization()->Localize(GetLanguage(), pText), sizeof(m_aFormatDialogText));

	// arrays replacing dialogs
	const char* pSearch = str_find(m_aFormatDialogText, "<bot_");
	while(pSearch != nullptr)
	{
		int SearchBotID = 0;
		if(sscanf(pSearch, "<bot_%d>", &SearchBotID) && DataBotInfo::IsDataBotValid(SearchBotID))
		{
			char aBufSearch[16];
			str_format(aBufSearch, sizeof(aBufSearch), "<bot_%d>", SearchBotID);
			str_replace(m_aFormatDialogText, aBufSearch, DataBotInfo::ms_aDataBot[SearchBotID].m_aNameBot);
		}
		pSearch = str_find(m_aFormatDialogText, "<bot_");
	}

	pSearch = str_find(m_aFormatDialogText, "<world_");
	while(pSearch != nullptr)
	{
		int WorldID = 0;
		if(sscanf(pSearch, "<world_%d>", &WorldID))
		{
			char aBufSearch[16];
			str_format(aBufSearch, sizeof(aBufSearch), "<world_%d>", WorldID);
			str_replace(m_aFormatDialogText, aBufSearch, Server()->GetWorldName(WorldID));
		}
		pSearch = str_find(m_aFormatDialogText, "<world_");
	}
	
	pSearch = str_find(m_aFormatDialogText, "<item_");
	while(pSearch != nullptr)
	{
		int ItemID = 0;
		if(sscanf(pSearch, "<item_%d>", &ItemID) && (CItemDescription::Data().find(ItemID) != CItemDescription::Data().end()))
		{
			char aBufSearch[16];
			str_format(aBufSearch, sizeof(aBufSearch), "<item_%d>", ItemID);
			str_replace(m_aFormatDialogText, aBufSearch, GS()->GetItemInfo(ItemID)->GetName());
		}
		pSearch = str_find(m_aFormatDialogText, "<item_");
	}

	// based replacing dialogs
	str_replace(m_aFormatDialogText, "<player>", GS()->Server()->ClientName(m_ClientID));
	str_replace(m_aFormatDialogText, "<talked>", DataBotInfo::ms_aDataBot[DataBotID].m_aNameBot);
	str_replace(m_aFormatDialogText, "<time> ", GS()->Server()->GetStringTypeDay());
	str_replace(m_aFormatDialogText, "<here>", GS()->Server()->GetWorldName(GS()->GetWorldID()));
	str_replace(m_aFormatDialogText, "<eidolon>", GetEidolon() ? DataBotInfo::ms_aDataBot[GetEidolon()->GetBotID()].m_aNameBot : "Eidolon");
}

void CPlayer::ClearDialogText()
{
	mem_zero(m_aFormatDialogText, sizeof(m_aFormatDialogText));
}

const char* CPlayer::GetStatus() const
{
	if(IsAuthed() && Acc().IsGuild())
		return GS()->Mmo()->Member()->GuildName(Acc().m_GuildID);
	return Server()->ClientClan(m_ClientID);
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
