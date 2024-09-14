/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "player.h"

#include "gamecontext.h"
#include "worldmodes/dungeon.h"

#include "core/components/Accounts/AccountManager.h"
#include "core/components/Accounts/AccountMiningManager.h"
#include "core/components/achievements/achievement_manager.h"
#include "core/components/Bots/BotManager.h"
#include "core/components/Dungeons/DungeonData.h"
#include "core/components/Eidolons/EidolonInfoData.h"
#include "core/components/guilds/guild_manager.h"
#include "core/components/quests/quest_manager.h"

#include "core/components/Inventory/ItemData.h"
#include "core/components/skills/skill_data.h"
#include "core/components/groups/group_data.h"
#include "core/components/worlds/world_data.h"

#include "core/tools/vote_optional.h"

MACRO_ALLOC_POOL_ID_IMPL(CPlayer, MAX_CLIENTS* ENGINE_MAX_WORLDS + MAX_CLIENTS)

IServer* CPlayer::Server() const { return m_pGS->Server(); };

CPlayer::CPlayer(CGS* pGS, int ClientID) : m_pGS(pGS), m_ClientID(ClientID)
{
	m_aPlayerTick[Die] = Server()->Tick();
	m_aPlayerTick[Respawn] = Server()->Tick() + Server()->TickSpeed();
	m_SnapHealthNicknameTick = 0;

	m_WantSpawn = true;
	m_PrevTuningParams = *pGS->Tuning();
	m_NextTuningParams = m_PrevTuningParams;
	m_Scenarios.Init(ClientID);
	m_Cooldown.Init(ClientID);
	m_VotesData.Init(m_pGS, this);
	m_Dialog.Init(this);

	// constructor only for players
	if(m_ClientID < MAX_PLAYERS)
	{
		m_TutorialStep = 1;
		m_MoodState = Mood::NORMAL;
		Account()->m_Team = GetStartTeam();
		GS()->SendTuningParams(ClientID);

		m_Afk = false;
		m_pLastInput = new CNetObj_PlayerInput({ 0 });
		m_LastInputInit = false;
		m_LastPlaytime = 0;
	}
}

CPlayer::~CPlayer()
{
	// free data
	if(m_pCharacter)
	{
		delete m_pCharacter;
		m_pCharacter = nullptr;
	}
	delete m_pLastInput;
}

void CPlayer::GetFormatedName(char* aBuffer, int BufferSize)
{
	bool isChatting = m_PlayerFlags & PLAYERFLAG_CHATTING;
	bool isAuthed = IsAuthed();
	int currentTick = Server()->Tick();
	int tickSpeed = Server()->TickSpeed();

	// Player is not chatting and health nickname tick is valid
	if(!isChatting && currentTick < m_SnapHealthNicknameTick)
	{
		int PercentHP = translate_to_percent(GetStartHealth(), GetHealth());
		char aHealthProgressBuf[6];
		str_format(aHealthProgressBuf, sizeof(aHealthProgressBuf), ":%d%%", clamp(PercentHP, 1, 100));

		char aNicknameBuf[MAX_NAME_LENGTH];
		str_utf8_truncate(aNicknameBuf, sizeof(aNicknameBuf), Server()->ClientName(m_ClientID),
			(int)((MAX_NAME_LENGTH - 1) - str_length(aHealthProgressBuf)));

		str_format(aBuffer, BufferSize, "%s%s", aNicknameBuf, aHealthProgressBuf);
		return;
	}

	// Update nickname leveling tick if player is authenticated and the tick is a multiple of 10 seconds
	if(isAuthed && currentTick % (tickSpeed * 10) == 0)
	{
		m_aPlayerTick[RefreshNickLeveling] = currentTick + tickSpeed;
	}

	// Player is authenticated, nickname leveling tick is valid, and not chatting
	if(isAuthed && m_aPlayerTick[RefreshNickLeveling] > currentTick && !isChatting)
	{
		str_format(aBuffer, BufferSize, "Lv%d %.4s...", Account()->GetLevel(), Server()->ClientName(m_ClientID));
	}
	else
	{
		str_copy(aBuffer, Server()->ClientName(m_ClientID), BufferSize);
	}
}

void CPlayer::Tick()
{
	if(!IsAuthed())
		return;

	// do latency stuff
	{
		int Latency = Server()->GetClientLatency(m_ClientID);
		if(Latency > 0)
		{
			m_Latency.m_Accum += Latency;
			m_Latency.m_AccumMax = maximum(m_Latency.m_AccumMax, Latency);
			m_Latency.m_AccumMin = minimum(m_Latency.m_AccumMin, Latency);
		}
		// each second
		if(Server()->Tick() % Server()->TickSpeed() == 0)
		{
			m_Latency.m_Avg = m_Latency.m_Accum / Server()->TickSpeed();
			m_Latency.m_Max = m_Latency.m_AccumMax;
			m_Latency.m_Min = m_Latency.m_AccumMin;
			m_Latency.m_Accum = 0;
			m_Latency.m_AccumMin = 1000;
			m_Latency.m_AccumMax = 0;
		}

		Server()->SetClientScore(m_ClientID, Account()->GetLevel());
	}

	if(m_pCharacter)
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
	else if(m_WantSpawn && m_aPlayerTick[Respawn] + Server()->TickSpeed() * 3 <= Server()->Tick())
	{
		TryRespawn();
	}

	// update events
	m_FixedView.Tick(m_ViewPos);
	m_Scenarios.Tick();
	m_Cooldown.Tick();
	if(m_pMotdMenu)
	{
		m_pMotdMenu->Tick();
	}
	else
	{
		m_Dialog.Tick();
	}

	if(m_PlayerFlags & PLAYERFLAG_IN_MENU)
		m_VotesData.ApplyVoteUpdaterData();
}

void CPlayer::PostTick()
{
	// Check if the user is authenticated
	if(IsAuthed())
	{
		// update latency value
		if(Server()->ClientIngame(m_ClientID))
			GetTempData().m_TempPing = m_Latency.m_Min;

		// handlers
		HandleEffects();
		HandleTuningParams();
		HandlePrison();
		CVoteOptional::HandleVoteOptionals(m_ClientID);
		Account()->GetBonusManager().UpdateBonuses();
		m_Scenarios.PostTick();
		m_FixedView.PostTick();
	}

	// handlers
	HandleScoreboardColors();
}

void CPlayer::PrepareRespawnTick()
{
	m_aPlayerTick[Respawn] = Server()->Tick() + Server()->TickSpeed() / 2;
	m_WantSpawn = true;
}

CPlayerBot* CPlayer::GetEidolon() const
{
	if(!m_EidolonCID)
		return nullptr;

	return dynamic_cast<CPlayerBot*>(GS()->GetPlayer(m_EidolonCID.value()));
}

void CPlayer::TryCreateEidolon()
{
	if(IsBot() || !IsAuthed() || !GetCharacter() || m_EidolonCID.has_value())
		return;

	// check valid equppied item id
	const auto eidolonItemID = GetEquippedItemID(EQUIP_EIDOLON);
	if(!eidolonItemID.has_value())
		return;

	// try to create eidolon
	if(const auto* pEidolonData = GS()->GetEidolonByItemID(eidolonItemID.value()))
	{
		if(int eidolonCID = GS()->CreateBot(TYPE_BOT_EIDOLON, pEidolonData->GetDataBotID(), m_ClientID); eidolonCID != -1)
		{
			if(auto* pEidolonPlayer = dynamic_cast<CPlayerBot*>(GS()->GetPlayer(eidolonCID)))
			{
				pEidolonPlayer->m_EidolonItemID = eidolonItemID.value();
				m_EidolonCID = eidolonCID;
			}
		}
	}
}

void CPlayer::TryRemoveEidolon()
{
	// try to remove eidolon
	if(m_EidolonCID)
	{
		GS()->DestroyPlayer(m_EidolonCID.value());
		m_EidolonCID.reset();
	}
}

void CPlayer::UpdateAchievement(AchievementType Type, int Criteria, int Progress, int ProgressType)
{
	GS()->Core()->AchievementManager()->UpdateAchievement(this, Type, Criteria, Progress, ProgressType);
}

void CPlayer::HandleEffects()
{
	if(Server()->Tick() % Server()->TickSpeed() != 0 || CGS::ms_aEffects[m_ClientID].empty())
		return;

	for(auto pEffect = CGS::ms_aEffects[m_ClientID].begin(); pEffect != CGS::ms_aEffects[m_ClientID].end();)
	{
		pEffect->second--;
		if(pEffect->second <= 0)
		{
			GS()->Chat(m_ClientID, "You lost the {} effect.", pEffect->first.c_str());
			pEffect = CGS::ms_aEffects[m_ClientID].erase(pEffect);
			continue;
		}

		++pEffect;
	}
}

void CPlayer::HandleScoreboardColors()
{
	if(m_TickActivedGroupColors > Server()->Tick())
		return;

	bool ScoreboardActive = m_PlayerFlags & PLAYERFLAG_SCOREBOARD;
	if(ScoreboardActive != m_ActivedGroupColors)
	{
		CMsgPacker Msg(NETMSGTYPE_SV_TEAMSSTATE);
		CMsgPacker MsgLegacy(NETMSGTYPE_SV_TEAMSSTATELEGACY);

		for(int i = 0; i < VANILLA_MAX_CLIENTS; ++i)
		{
			CPlayer* pPlayer = GS()->GetPlayer(i, true);
			int TeamColor = (ScoreboardActive && pPlayer && pPlayer->Account()->GetGroup()) ?
				pPlayer->Account()->GetGroup()->GetTeamColor() : 0;

			Msg.AddInt(TeamColor);
			MsgLegacy.AddInt(TeamColor);
		}

		Server()->SendMsg(&Msg, MSGFLAG_VITAL, m_ClientID);

		int ClientVersion = Server()->GetClientVersion(m_ClientID);
		if(VERSION_DDRACE < ClientVersion && ClientVersion < VERSION_DDNET_MSG_LEGACY)
			Server()->SendMsg(&MsgLegacy, MSGFLAG_VITAL, m_ClientID);

		m_ActivedGroupColors = ScoreboardActive;
		m_TickActivedGroupColors = Server()->Tick() + (Server()->TickSpeed() / 4);
	}
}

void CPlayer::HandlePrison()
{
	// check prison state
	if(!Account()->IsInPrison() || !GetCharacter())
		return;

	// change world to jail world
	int JailWorldID = GS()->GetWorldData()->GetJailWorld()->GetID();
	if(GetPlayerWorldID() != JailWorldID)
	{
		ChangeWorld(JailWorldID);
		return;
	}

	// check leave prison
	if(distance(m_pCharacter->m_Core.m_Pos, GS()->GetJailPosition()) > 1000.f)
	{
		GetCharacter()->ChangePosition(GS()->GetJailPosition());
		GS()->Chat(m_ClientID, "You cannot leave the prison!");
	}

	// handle tick
	if(Server()->Tick() % Server()->TickSpeed() == 0)
	{
		Account()->m_PrisonSeconds--;
		GS()->Broadcast(m_ClientID, BroadcastPriority::MAIN_INFORMATION, 50, "You will be released from prison in {} seconds.", Account()->m_PrisonSeconds);

		if(Account()->m_PrisonSeconds <= 0)
		{
			Account()->FreeFromPrison();
		}
		else if(Server()->Tick() % (Server()->TickSpeed() * 10) == 0)
		{
			GS()->Core()->SaveAccount(this, SAVE_SOCIAL_STATUS);
		}
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
	CNetObj_ClientInfo* pClientInfo = Server()->SnapNewItem<CNetObj_ClientInfo>(m_ClientID);
	if(!pClientInfo)
		return;

	// Check if it's time to refresh the clan title
	if(m_aPlayerTick[RefreshClanTitle] < Server()->Tick())
	{
		// Rotate the clan string by the length of the first character
		int clanStringSize = str_utf8_fix_truncation(m_aRotateClanBuffer);
		std::ranges::rotate(m_aRotateClanBuffer, std::begin(m_aRotateClanBuffer) + str_utf8_forward(m_aRotateClanBuffer, 0));

		// Set the next tick for refreshing the clan title
		m_aPlayerTick[RefreshClanTitle] = Server()->Tick() + (((m_aRotateClanBuffer[0] == '|') || (clanStringSize - 1 < 10)) ? Server()->TickSpeed() : (Server()->TickSpeed() / 8));

		// If the clan string size is less than 10
		if(clanStringSize < 10)
		{
			m_aPlayerTick[RefreshClanTitle] = Server()->Tick() + Server()->TickSpeed();
			RefreshClanString();
		}
	}

	char aNameBuf[MAX_NAME_LENGTH];
	GetFormatedName(aNameBuf, sizeof(aNameBuf));
	StrToInts(&pClientInfo->m_Name0, 4, aNameBuf);
	StrToInts(&pClientInfo->m_Clan0, 3, m_aRotateClanBuffer);
	pClientInfo->m_Country = Server()->ClientCountry(m_ClientID);
	StrToInts(&pClientInfo->m_Skin0, 6, GetTeeInfo().m_aSkinName);
	pClientInfo->m_UseCustomColor = GetTeeInfo().m_UseCustomColor;
	pClientInfo->m_ColorBody = GetTeeInfo().m_ColorBody;
	pClientInfo->m_ColorFeet = GetTeeInfo().m_ColorFeet;

	CNetObj_PlayerInfo* pPlayerInfo = Server()->SnapNewItem<CNetObj_PlayerInfo>(m_ClientID);
	if(!pPlayerInfo)
		return;

	const bool localClient = m_ClientID == SnappingClient;
	pPlayerInfo->m_Local = localClient;
	pPlayerInfo->m_ClientId = m_ClientID;
	pPlayerInfo->m_Team = GetTeam();
	pPlayerInfo->m_Latency = (SnappingClient == -1 ? m_Latency.m_Min : GetTempData().m_TempPing);
	pPlayerInfo->m_Score = Account()->GetLevel();

	const auto& optViewLockAt = m_FixedView.GetCurrentView();
	if(auto pDDNetPlayer = static_cast<CNetObj_DDNetPlayer*>(Server()->SnapNewItem(NETOBJTYPE_DDNETPLAYER, m_ClientID, sizeof(CNetObj_DDNetPlayer))))
	{
		pDDNetPlayer->m_AuthLevel = Server()->GetAuthedState(m_ClientID);
		pDDNetPlayer->m_Flags = optViewLockAt.has_value() ? EXPLAYERFLAG_SPEC : 0;
	}

	if(localClient && (GetTeam() == TEAM_SPECTATORS || optViewLockAt.has_value()))
	{
		CNetObj_SpectatorInfo* pSpectatorInfo = static_cast<CNetObj_SpectatorInfo*>(Server()->SnapNewItem(NETOBJTYPE_SPECTATORINFO, m_ClientID, sizeof(CNetObj_SpectatorInfo)));
		if(!pSpectatorInfo)
			return;

		const bool isFixedViewLocked = optViewLockAt.has_value();
		pSpectatorInfo->m_X = m_ViewPos.x = (isFixedViewLocked ? optViewLockAt->x : m_ViewPos.x);
		pSpectatorInfo->m_Y = m_ViewPos.y = (isFixedViewLocked ? optViewLockAt->y : m_ViewPos.y);
		pSpectatorInfo->m_SpectatorId = m_ClientID;
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
	pPlayerInfo->m_ClientId = FakeID;
	pPlayerInfo->m_Score = -9999;
	pPlayerInfo->m_Team = TEAM_SPECTATORS;

	CNetObj_SpectatorInfo* pSpectatorInfo = static_cast<CNetObj_SpectatorInfo*>(Server()->SnapNewItem(NETOBJTYPE_SPECTATORINFO, FakeID, sizeof(CNetObj_SpectatorInfo)));
	if(!pSpectatorInfo)
		return;

	pSpectatorInfo->m_SpectatorId = -1;
	pSpectatorInfo->m_X = m_ViewPos.x;
	pSpectatorInfo->m_Y = m_ViewPos.y;
}

void CPlayer::RefreshClanString()
{
	if(!IsAuthed())
	{
		str_copy(m_aRotateClanBuffer, Server()->ClientClan(m_ClientID), sizeof(m_aRotateClanBuffer));
		return;
	}

	// location
	std::string Prepared(Server()->GetWorldName(GetPlayerWorldID()));

	// title
	if(const auto TitleItemID = GetEquippedItemID(EQUIP_TITLE); TitleItemID.has_value())
	{
		Prepared += " | ";
		Prepared += GetItem(TitleItemID.value())->Info()->GetName();
	}

	// guild
	if(const CGuild* pGuild = Account()->GetGuild())
	{
		Prepared += " | ";
		Prepared += pGuild->GetName();
		Prepared += " : ";
		Prepared += Account()->GetGuildMember()->GetRank()->GetName();
	}

	// class
	const char* pClassName;
	switch(m_Class.GetGroup())
	{
		case ClassGroup::Healer: pClassName = "_Healer_"; break;
		case ClassGroup::Dps: pClassName = "_DPS_"; break;
		case ClassGroup::Tank: pClassName = "_Tank_"; break;
		default: pClassName = "_Class_"; break;
	}
	char aBufClass[64];
	str_format(aBufClass, sizeof(aBufClass), "%-*s", 10 - str_length(pClassName), pClassName);
	Prepared += " | ";
	Prepared += aBufClass;

	// end format
	str_format(m_aRotateClanBuffer, sizeof(m_aRotateClanBuffer), "%s", Prepared.c_str());
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
	int SpawnType = Account()->IsInPrison() ? SPAWN_HUMAN_PRISON : SPAWN_HUMAN;

	// Check if the last killed by weapon is not WEAPON_WORLD
	if(!Account()->IsInPrison() && GetTempData().m_LastKilledByWeapon != WEAPON_WORLD)
	{
		int RespawnWorldID = GS()->GetWorldData()->GetRespawnWorld()->GetID();
		if(RespawnWorldID >= 0 && !GS()->IsPlayerInWorld(m_ClientID, RespawnWorldID))
		{
			ChangeWorld(RespawnWorldID);
			return;
		}
		SpawnType = SPAWN_HUMAN_TREATMENT;
	}

	// Check if the controller allows spawning of the given spawn type at the specified position
	if(GS()->m_pController->CanSpawn(SpawnType, &SpawnPos))
	{
		vec2 TeleportPosition = GetTempData().GetTeleportPosition();
		bool CanSelfCordSpawn = !is_negative_vec(TeleportPosition) && !GS()->Collision()->CheckPoint(TeleportPosition);

		// Use self-coordinated spawning if possible
		if(!GS()->IsWorldType(WorldType::Dungeon) && CanSelfCordSpawn)
		{
			SpawnPos = TeleportPosition;
		}

		// Create and spawn a new character
		int AllocMemoryCell = MAX_CLIENTS * GS()->GetWorldID() + m_ClientID;
		m_pCharacter = new(AllocMemoryCell) CCharacter(&GS()->m_World);
		m_pCharacter->Spawn(this, SpawnPos);
		GS()->CreatePlayerSpawn(SpawnPos);
		GetTempData().ClearTeleportPosition();
		m_WantSpawn = false;
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
	// Update view position for spectators
	if(!m_pCharacter && GetTeam() == TEAM_SPECTATORS)
	{
		m_ViewPos = vec2(pNewInput->m_TargetX, pNewInput->m_TargetY);
	}

	// parse event keys
	Server()->Input()->ParseInputClickedKeys(m_ClientID , pNewInput, m_pLastInput);
	if(m_pCharacter)
	{
		const int ActiveWeapon = m_pCharacter->m_Core.m_ActiveWeapon;
		Server()->Input()->ProcessCharacterInput(m_ClientID, ActiveWeapon, pNewInput, m_pLastInput);
	}

	// Reset input when chatting
	if(pNewInput->m_PlayerFlags & PLAYERFLAG_CHATTING)
	{
		if(m_PlayerFlags & PLAYERFLAG_CHATTING)
			return;

		if(m_pCharacter)
			m_pCharacter->ResetInput();

		m_PlayerFlags = pNewInput->m_PlayerFlags;
		return;
	}

	m_PlayerFlags = pNewInput->m_PlayerFlags;

	if(m_pCharacter)
	{
		// Update AFK status
		if(g_Config.m_SvMaxAfkTime != 0)
			m_Afk = m_LastPlaytime < time_get() - time_freq() * g_Config.m_SvMaxAfkTime;

		m_pCharacter->OnDirectInput(pNewInput);
	}

	// Check for activity
	if(mem_comp(pNewInput, m_pLastInput, sizeof(CNetObj_PlayerInput)))
	{
		mem_copy(m_pLastInput, pNewInput, sizeof(CNetObj_PlayerInput));
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
	if(GS()->Core()->AccountManager()->IsActive(m_ClientID))
		return Account()->m_Team;
	return TEAM_SPECTATORS;
}

/* #########################################################################
	FUNCTIONS PLAYER HELPER
######################################################################### */
void CPlayer::ProgressBar(const char* Name, int MyLevel, int MyExp, int ExpNeed, int GivedExp) const
{
	char aBufBroadcast[128];
	const float GetLevelProgress = translate_to_percent((float)ExpNeed, (float)MyExp);
	const float GetExpProgress = translate_to_percent((float)ExpNeed, (float)GivedExp);

	std::string ProgressBar = mystd::string::progressBar(100, (int)GetLevelProgress, 10, ":", " ");
	str_format(aBufBroadcast, sizeof(aBufBroadcast), "Lv%d %s[%s] %0.2f%%+%0.3f%%(%d)XP", MyLevel, Name, ProgressBar.c_str(), GetLevelProgress, GetExpProgress, GivedExp);
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
		GS()->Broadcast(m_ClientID, BroadcastPriority::GAME_WARNING, 100, "Not upgrade points for +{}. Required {}.", Value, UpgradeNeed);
		return false;
	}

	*Useless -= UpgradeNeed;
	*Upgrade += Value;
	return true;
}

/* #########################################################################
	FUNCTIONS PLAYER ACCOUNT
######################################################################### */
bool CPlayer::GiveEffect(const char* Potion, int Sec, float Chance)
{
	if(m_pCharacter && m_pCharacter->IsAlive())
	{
		const float RandomChance = random_float(100.0f);
		if(RandomChance < Chance)
		{
			GS()->Chat(m_ClientID, "You got the effect {} time {} seconds.", Potion, Sec);
			CGS::ms_aEffects[m_ClientID][Potion] = Sec;
			return true;
		}
	}

	return false;
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

bool CPlayer::IsAuthed() const
{
	if(GS()->Core()->AccountManager()->IsActive(m_ClientID))
		return Account()->GetID() > 0;
	return false;
}

int CPlayer::GetStartTeam() const
{
	if(IsAuthed())
		return TEAM_RED;
	return TEAM_SPECTATORS;
}

int CPlayer::GetStartHealth() const
{
	int DefaultHP = 10 + GetTotalAttributeValue(AttributeIdentifier::HP);
	DefaultHP += translate_to_percent_rest(DefaultHP, m_Class.GetExtraHP());
	Account()->GetBonusManager().ApplyBonuses(BONUS_TYPE_HP, &DefaultHP);
	return DefaultHP;
}

int CPlayer::GetStartMana() const
{
	int DefaultMP = 10 + GetTotalAttributeValue(AttributeIdentifier::MP);
	DefaultMP += translate_to_percent_rest(DefaultMP, m_Class.GetExtraMP());
	Account()->GetBonusManager().ApplyBonuses(BONUS_TYPE_MP, &DefaultMP);
	return DefaultMP;
}

int64_t CPlayer::GetAfkTime() const
{
	return m_Afk ? ((time_get() - m_LastPlaytime) / time_freq()) - g_Config.m_SvMaxAfkTime : 0;
}

void CPlayer::FormatBroadcastBasicStats(char* pBuffer, int Size, const char* pAppendStr)
{
	if(!IsAuthed() || !m_pCharacter)
		return;

	const int LevelPercent = translate_to_percent(static_cast<int>(computeExperience(Account()->GetLevel())), Account()->GetExperience());
	const int MaximumHealth = GetStartHealth();
	const int MaximumMana = GetStartMana();
	const int Health = m_pCharacter->Health();
	const int Mana = m_pCharacter->Mana();
	const auto BankGoldStr = fmt_big_digit(Account()->GetBank().to_string());
	const auto GoldStr = fmt_big_digit(std::to_string(Account()->GetGold()));
	const auto GoldCapacityStr = fmt_big_digit(std::to_string(Account()->GetGoldCapacity()));

	char aRecastInfo[32] = { '\0' };
	const int PotionRecastTime = m_aPlayerTick[PotionRecast] - Server()->Tick();
	if(PotionRecastTime > 0)
	{
		const int Seconds = maximum(0, PotionRecastTime / Server()->TickSpeed());
		str_format(aRecastInfo, sizeof(aRecastInfo), "Potion recast: %d", Seconds);
	}

	const auto BonusActivities = Account()->GetBonusManager().GetBonusActivitiesString();
	const int NewlinesNeeded = 7 - BonusActivities.first;
	std::string AdditionNewlines(NewlinesNeeded > 0 ? NewlinesNeeded : 0, '\n');
	const std::string ProgressBar = mystd::string::progressBar(100, LevelPercent, 10, ":", " ");

	str_format(pBuffer, Size,
		"\n\n\n\n\nLv%d[%s]\nHP %d/%d\nMP %d/%d\nGold %s of %s\nBank %s\n%s\n%s\n%s\n%-150s",
		Account()->GetLevel(), ProgressBar.c_str(), Health, MaximumHealth, Mana, MaximumMana,
		GoldStr.c_str(), GoldCapacityStr.c_str(), BankGoldStr.c_str(),
		BonusActivities.second.c_str(), aRecastInfo, AdditionNewlines.c_str(), pAppendStr);
}

/* #########################################################################
	FUNCTIONS PLAYER PARSING
######################################################################### */
bool CPlayer::ParseVoteOptionResult(int Vote)
{
	if(!m_pCharacter)
	{
		GS()->Chat(m_ClientID, "Use it when you're not dead!");
		return true;
	}

	if(!CVoteOptional::Data()[m_ClientID].empty())
	{
		CVoteOptional* pOptional = &CVoteOptional::Data()[m_ClientID].front();
		pOptional->ExecuteVote(Vote == 1);
	}

	// - - - - - F3- - - - - - -
	if(Vote == 1)
	{
		if(GS()->IsWorldType(WorldType::Dungeon))
		{
			const int DungeonID = dynamic_cast<CGameControllerDungeon*>(GS()->m_pController)->GetDungeonID();
			if(!CDungeonData::ms_aDungeon[DungeonID].IsDungeonPlaying())
			{
				GetTempData().m_TempDungeonReady ^= true;
				GS()->Chat(m_ClientID, "You changed the ready mode to \"{}\"!", GetTempData().m_TempDungeonReady ? "ready" : "not ready");
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

	const auto& playerSkills = CSkill::Data()[m_ClientID];
	auto iter = std::find_if(playerSkills.begin(), playerSkills.end(), [&ID](CSkill* pSkill){ return pSkill->GetID() == ID; });
	return (iter == playerSkills.end() ? CSkill::CreateElement(m_ClientID, ID) : *iter);
}

CPlayerQuest* CPlayer::GetQuest(QuestIdentifier ID) const
{
	dbg_assert(CQuestDescription::Data().find(ID) != CQuestDescription::Data().end(), "invalid referring to the CPlayerQuest");
	if(CPlayerQuest::Data()[m_ClientID].find(ID) == CPlayerQuest::Data()[m_ClientID].end())
		CPlayerQuest::CreateElement(ID, m_ClientID);
	return CPlayerQuest::Data()[m_ClientID][ID];
}

std::optional<int> CPlayer::GetEquippedItemID(ItemFunctional EquipID, int SkipItemID) const
{
	const auto& playerItems = CPlayerItem::Data()[m_ClientID];
	for(const auto& [itemID, item] : playerItems)
	{
		if(item.HasItem() && item.IsEquipped() && item.Info()->IsFunctional(EquipID) && itemID != SkipItemID)
			return itemID;
	}
	return std::nullopt;
}

bool CPlayer::IsEquipped(ItemFunctional EquipID) const
{
	return GetEquippedItemID(EquipID, -1) != std::nullopt;
}

int CPlayer::GetTotalAttributeValue(AttributeIdentifier ID) const
{
	// initialize variables
	const CAttributeDescription* pAtt = GS()->GetAttributeInfo(ID);

	// check if the player is in a dungeon and the attribute has a low improvement cost
	if(GS()->IsWorldType(WorldType::Dungeon))
	{
		const CGameControllerDungeon* pDungeon = dynamic_cast<CGameControllerDungeon*>(GS()->m_pController);
		if(pAtt->GetUpgradePrice() < 4 && CDungeonData::ms_aDungeon[pDungeon->GetDungeonID()].IsDungeonPlaying())
			return pDungeon->GetAttributeDungeonSync(this, ID);
	}

	// counting attributes from equipped items
	int totalValue = 0;
	for(const auto& [ItemID, ItemData] : CPlayerItem::Data()[m_ClientID])
	{
		if(ItemData.IsEquipped() && ItemData.Info()->IsEnchantable() && ItemData.Info()->GetInfoEnchantStats(ID))
		{
			totalValue += ItemData.GetEnchantStats(ID);
		}
	}

	// add attribute value from player's improvements
	if(pAtt->HasDatabaseField())
	{
		totalValue += Account()->m_aStats[ID];
	}

	return totalValue;
}

float CPlayer::GetAttributeChance(AttributeIdentifier ID) const
{
	// initialize variables
	int attributeValue = GetTotalAttributeValue(ID);
	float chance = 0.0f;

	// chance
	switch(ID)
	{
		case AttributeIdentifier::Vampirism:
		case AttributeIdentifier::Crit:
			chance = std::min(8.0f + static_cast<float>(attributeValue) * 0.0015f, 30.0f);
			break;

		case AttributeIdentifier::Lucky:
			chance = std::min(5.0f + static_cast<float>(attributeValue) * 0.0015f, 20.0f);
			break;

		default:
			break;
	}

	return chance;
}

int CPlayer::GetTotalAttributesInGroup(AttributeGroup Type)
{
	int totalSize = 0;
	for(const auto& [ID, pAttribute] : CAttributeDescription::Data())
	{
		if(pAttribute->IsGroup(Type))
		{
			totalSize += GetTotalAttributeValue(ID);
		}
	}
	return totalSize;
}

int CPlayer::GetTotalAttributes()
{
	int totalSize = 0;
	for(const auto& [ID, Attribute] : CAttributeDescription::Data())
	{
		totalSize += GetTotalAttributeValue(ID);
	}
	return totalSize;
}

void CPlayer::SetSnapHealthTick(int Sec)
{
	m_SnapHealthNicknameTick = Server()->Tick() + (Server()->TickSpeed() * Sec);
}

void CPlayer::ChangeWorld(int WorldID, std::optional<vec2> newWorldPosition) const
{
	// reset dungeon temporary data
	auto& tempData = GetTempData();
	tempData.m_TempDungeonReady = false;
	tempData.m_TempTimeDungeon = 0;

	// if new position is provided, set the teleport position
	if(newWorldPosition.has_value())
	{
		tempData.SetTeleportPosition(newWorldPosition.value());
	}

	// change the player's world
	Account()->m_aHistoryWorld.push_front(WorldID);
	Server()->ChangeWorld(m_ClientID, WorldID);
}

int CPlayer::GetPlayerWorldID() const
{
	return Server()->GetClientWorldID(m_ClientID);
}

CTeeInfo& CPlayer::GetTeeInfo() const
{
	return Account()->m_TeeInfos;
}