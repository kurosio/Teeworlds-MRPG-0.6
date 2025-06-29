/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "player.h"

#include "gamecontext.h"
#include "worldmodes/dungeon/dungeon.h"

#include "core/components/accounts/account_manager.h"
#include "core/components/achievements/achievement_manager.h"
#include "core/components/Bots/BotManager.h"
#include "core/components/duties/dungeon_data.h"
#include "core/components/Eidolons/EidolonInfoData.h"
#include "core/components/guilds/guild_manager.h"
#include "core/components/quests/quest_manager.h"

#include "core/components/inventory/item_data.h"
#include "core/components/skills/skill_data.h"
#include "core/components/groups/group_data.h"
#include "core/components/worlds/world_data.h"

#include "core/tools/vote_optional.h"
#include "core/scenarios/scenario_universal.h"
#include "core/tools/scenario_player_manager.h"

MACRO_ALLOC_POOL_ID_IMPL(CPlayer, MAX_CLIENTS * ENGINE_MAX_WORLDS + MAX_CLIENTS)

IServer* CPlayer::Server() const { return m_pGS->Server(); };

CPlayer::CPlayer(CGS* pGS, int ClientID) : m_pGS(pGS), m_ClientID(ClientID)
{
	m_aPlayerTick[Die] = Server()->Tick();
	m_aPlayerTick[Respawn] = Server()->Tick() + Server()->TickSpeed();
	m_ShowHealthNicknameTick = 0;

	m_WantSpawn = true;
	m_Afk = false;
	m_pLastInput = new CNetObj_PlayerInput({ 0 });
	m_LastInputInit = false;
	m_LastPlaytime = 0;
	m_MoodState = Mood::Normal;
	m_PrevTuningParams = *pGS->Tuning();
	m_NextTuningParams = m_PrevTuningParams;
	m_Cooldown.Init(ClientID);
	m_VotesData.Init(m_pGS, this);
	m_Dialog.Init(this);

	// constructor only for players
	if(m_ClientID < MAX_PLAYERS)
	{
		GS()->SendTuningParams(ClientID);
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
	const auto isChatting = m_PlayerFlags & PLAYERFLAG_CHATTING;
	const auto isAuthed = IsAuthed();
	const auto currentTick = Server()->Tick();
	const auto tickSpeed = Server()->TickSpeed();

	// Player is not chatting and health nickname tick is valid
	if(!isChatting && currentTick < m_ShowHealthNicknameTick)
	{
		char aHealthProgressBuf[6];
		char aNicknameBuf[MAX_NAME_LENGTH];
		const int PercentHP = round_to_int(translate_to_percent(GetMaxHealth(), GetHealth()));

		str_format(aHealthProgressBuf, sizeof(aHealthProgressBuf), ":%d%%", clamp(PercentHP, 1, 100));
		str_utf8_truncate(aNicknameBuf, sizeof(aNicknameBuf), Server()->ClientName(m_ClientID), MAX_NAME_LENGTH - 1 - str_length(aHealthProgressBuf));
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
	m_Cooldown.Tick();
	if(m_pMotdMenu)
	{
		m_pMotdMenu->Tick();
	}
	else
	{
		m_Dialog.Tick();
	}

	if(Account()->IsCrimeDecreaseTime())
		Account()->DecreaseCrime(10);

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
			GetSharedData().m_Ping = m_Latency.m_Min;

		// handlers
		HandleTuningParams();
		CVoteOptional::HandleVoteOptionals(m_ClientID);
		Account()->GetBonusManager().PostTick();
		Account()->GetPrisonManager().PostTick();
		m_Effects.PostTick();
	}

	// update view pos for spectators
	const bool isViewLocked = m_FixedView.GetCurrentView().has_value();
	const auto spectatorID = Server()->GetSpectatorID(m_ClientID);
	if(!isViewLocked && GetTeam() == TEAM_SPECTATORS && spectatorID != SPEC_FREEVIEW)
	{
		auto* pSpecPlayer = GS()->GetPlayer(spectatorID);
		if(pSpecPlayer)
		{
			if(pSpecPlayer->GetCharacter())
				m_ViewPos = pSpecPlayer->GetCharacter()->GetPos();
			else if(Server()->ClientIngame(spectatorID) &&
					Server()->GetClientWorldID(spectatorID) != Server()->GetClientWorldID(m_ClientID))
				ChangeWorld(Server()->GetClientWorldID(spectatorID));
		}
		else
			Server()->SetSpectatorID(m_ClientID, SPEC_FREEVIEW);
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
	const auto eidolonItemID = GetEquippedSlotItemID(ItemType::EquipEidolon);
	if(!eidolonItemID.has_value())
		return;

	// try to create eidolon
	if(const auto* pEidolonData = GS()->GetEidolonByItemID(eidolonItemID.value()))
	{
		if(auto pPlayerEidolon = GS()->CreateBot(TYPE_BOT_EIDOLON, pEidolonData->GetDataBotID(), m_ClientID))
		{
			pPlayerEidolon->m_EidolonItemID = eidolonItemID.value();
			m_EidolonCID = pPlayerEidolon->GetCID();
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

void CPlayer::HandleScoreboardColors()
{
	if(m_TickActivatedGroupColour > Server()->Tick())
		return;

	bool ScoreboardActive = m_PlayerFlags & PLAYERFLAG_SCOREBOARD;
	if(ScoreboardActive != m_ActivatedGroupColour)
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

		m_ActivatedGroupColour = ScoreboardActive;
		m_TickActivatedGroupColour = Server()->Tick() + (Server()->TickSpeed() / 4);
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
	// client info
	if(auto* pClientInfo = Server()->SnapNewItem<CNetObj_ClientInfo>(m_ClientID))
	{
		// prepare clan string
		if(m_aPlayerTick[RefreshClanTitle] < Server()->Tick())
		{
			const auto clanStringSize = str_utf8_fix_truncation(m_aRotateClanBuffer);
			std::rotate(m_aRotateClanBuffer, m_aRotateClanBuffer + str_utf8_forward(m_aRotateClanBuffer, 0), m_aRotateClanBuffer + clanStringSize);
			m_aPlayerTick[RefreshClanTitle] = Server()->Tick() + (m_aRotateClanBuffer[0] == '|' ? Server()->TickSpeed() : Server()->TickSpeed() / 8);

			if(m_aInitialClanBuffer[0] == '\0' || str_comp_nocase(m_aRotateClanBuffer, m_aInitialClanBuffer) == 0)
			{
				RefreshClanTagString();
			}
		}

		char aNameBuf[MAX_NAME_LENGTH];
		GetFormatedName(aNameBuf, sizeof(aNameBuf));
		StrToInts(&pClientInfo->m_Name0, 4, aNameBuf);
		StrToInts(&pClientInfo->m_Clan0, 3, m_aRotateClanBuffer);
		pClientInfo->m_Country = Server()->ClientCountry(m_ClientID);
		StrToInts(&pClientInfo->m_Skin0, 6, Account()->GetTeeInfo().m_aSkinName);
		pClientInfo->m_UseCustomColor = Account()->GetTeeInfo().m_UseCustomColor;
		pClientInfo->m_ColorBody = Account()->GetTeeInfo().m_ColorBody;
		pClientInfo->m_ColorFeet = Account()->GetTeeInfo().m_ColorFeet;
	}

	// player info
	if(auto* pPlayerInfo = Server()->SnapNewItem<CNetObj_PlayerInfo>(m_ClientID))
	{
		const bool localClient = m_ClientID == SnappingClient;
		const bool isViewLocked = m_FixedView.GetCurrentView().has_value();

		pPlayerInfo->m_Local = localClient;
		pPlayerInfo->m_ClientId = m_ClientID;
		pPlayerInfo->m_Team = GetTeam();
		pPlayerInfo->m_Latency = (SnappingClient == -1 ? m_Latency.m_Min : GetSharedData().m_Ping);
		pPlayerInfo->m_Score = Account()->GetLevel();

		// ddnet player
		if(auto* pDDNetPlayer = Server()->SnapNewItem<CNetObj_DDNetPlayer>(m_ClientID))
		{
			pDDNetPlayer->m_AuthLevel = Server()->GetAuthedState(m_ClientID);
			pDDNetPlayer->m_Flags = isViewLocked ? EXPLAYERFLAG_SPEC : 0;
		}

		// spectator info
		if(localClient && (GetTeam() == TEAM_SPECTATORS || isViewLocked))
		{
			if(auto* pSpectatorInfo = Server()->SnapNewItem<CNetObj_SpectatorInfo>(m_ClientID))
			{
				pSpectatorInfo->m_SpectatorId = (isViewLocked ? m_ClientID : Server()->GetSpectatorID(m_ClientID));
				pSpectatorInfo->m_X = m_ViewPos.x;
				pSpectatorInfo->m_Y = m_ViewPos.y;
				m_FixedView.Reset();
			}

			if(auto* pDDNetSpectatorInfo = Server()->SnapNewItem<CNetObj_DDNetSpectatorInfo>(m_ClientID))
			{
				pDDNetSpectatorInfo->m_HasCameraInfo = 0;
				pDDNetSpectatorInfo->m_Zoom = 1000;
				pDDNetSpectatorInfo->m_Deadzone = 800;
				pDDNetSpectatorInfo->m_FollowFactor = 0;
			}
		}
	}
}

void CPlayer::FakeSnap()
{
	constexpr int FakeID = VANILLA_MAX_CLIENTS - 1;

	// client info
	if(auto* pClientInfo = Server()->SnapNewItem<CNetObj_ClientInfo>(FakeID))
	{
		StrToInts(&pClientInfo->m_Name0, 4, " ");
		StrToInts(&pClientInfo->m_Clan0, 3, "");
		StrToInts(&pClientInfo->m_Skin0, 6, "default");
	}

	// player info
	if(auto* pPlayerInfo = Server()->SnapNewItem<CNetObj_PlayerInfo>(FakeID))
	{
		pPlayerInfo->m_Latency = m_Latency.m_Min;
		pPlayerInfo->m_Local = 1;
		pPlayerInfo->m_ClientId = FakeID;
		pPlayerInfo->m_Score = -9999;
		pPlayerInfo->m_Team = TEAM_SPECTATORS;
	}

	// spectator info
	if(auto* pSpectatorInfo = Server()->SnapNewItem<CNetObj_SpectatorInfo>(FakeID))
	{
		pSpectatorInfo->m_SpectatorId = Server()->GetSpectatorID(FakeID);
		pSpectatorInfo->m_X = m_ViewPos.x;
		pSpectatorInfo->m_Y = m_ViewPos.y;
	}
}

void CPlayer::RefreshClanTagString()
{
	// is not authed send only clan
	if(!IsAuthed())
	{
		str_copy(m_aRotateClanBuffer, Server()->ClientClan(m_ClientID), sizeof(m_aRotateClanBuffer));
		return;
	}

	// top rank position
	auto* pAccount = Account();
	auto RatingRank = Server()->GetAccountRank(pAccount->GetID());
	auto RatingPoints = pAccount->GetRatingSystem().GetRating();
	auto RatingRankName = pAccount->GetRatingSystem().GetRankName();
	std::string prepared = fmt_default(" | #{} {}({})", RatingRank, RatingRankName, RatingPoints);

	// location
	prepared += fmt_default(" | {}", Server()->GetWorldName(GetCurrentWorldID()));

	// title
	if(const auto titleItemID = GetEquippedSlotItemID(ItemType::EquipTitle))
		prepared += fmt_default(" | {}", GetItem(titleItemID.value())->Info()->GetName());

	// guild
	if(const auto* pGuild = pAccount->GetGuild())
		prepared += fmt_default(" | {} : {}", pGuild->GetName(), pAccount->GetGuildMember()->GetRank()->GetName());

	// class
	char classBuffer[64];
	const char* professionName = GetProfessionName(Account()->GetActiveProfessionID());
	str_format(classBuffer, sizeof(classBuffer), " %-*s ", 8 - str_length(professionName), professionName);
	prepared += fmt_default(" | {}", classBuffer);

	// end format
	str_format(m_aRotateClanBuffer, sizeof(m_aRotateClanBuffer), "%s", prepared.c_str());
	str_format(m_aInitialClanBuffer, sizeof(m_aInitialClanBuffer), "%s", prepared.c_str());
}

CCharacter* CPlayer::GetCharacter() const
{
	if(m_pCharacter && m_pCharacter->IsAlive())
		return m_pCharacter;
	return nullptr;
}

void CPlayer::TryRespawn()
{
	int SpawnType = SPAWN_HUMAN;
	std::optional<vec2> FinalSpawnPos = std::nullopt;

	// spawn by prison
	if(Account()->GetPrisonManager().IsInPrison())
	{
		SpawnType = SPAWN_HUMAN_PRISON;
	}

	// spawn by kill
	else if(GetSharedData().m_LastKilledByWeapon != WEAPON_WORLD)
	{
		auto* pRespawnWorld = GS()->GetWorldData()->GetRespawnWorld();
		if(pRespawnWorld && !GS()->IsPlayerInWorld(m_ClientID, pRespawnWorld->GetID()))
		{
			ChangeWorld(pRespawnWorld->GetID());
			return;
		}
	}

	// spawn by optional teleport
	else if(!GS()->IsWorldType(WorldType::Dungeon))
	{
		auto optionalSpawnPos = GetSharedData().GetSpawnPosition();
		if(optionalSpawnPos.has_value() && !GS()->Collision()->CheckPoint(*optionalSpawnPos))
			FinalSpawnPos = optionalSpawnPos;
	}


	// prepare spawn position
	if(!FinalSpawnPos.has_value())
	{
		vec2 SpawnPos;
		if(GS()->m_pController->CanSpawn(SpawnType, &SpawnPos))
			FinalSpawnPos = SpawnPos;
	}


	// respawn character
	if(FinalSpawnPos.has_value())
	{
		int AllocMemoryCell = MAX_CLIENTS * GS()->GetWorldID() + m_ClientID;
		m_pCharacter = new(AllocMemoryCell) CCharacter(&GS()->m_World);
		m_pCharacter->Spawn(this, *FinalSpawnPos);
		GS()->CreatePlayerSpawn(*FinalSpawnPos);
		GetSharedData().ClearSpawnPosition();
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
	if(!m_pCharacter && GetTeam() == TEAM_SPECTATORS && Server()->GetSpectatorID(m_ClientID) == SPEC_FREEVIEW)
		m_ViewPos = vec2(pNewInput->m_TargetX, pNewInput->m_TargetY);

	// parse event keys
	Server()->Input()->ParseInputClickedKeys(m_ClientID, pNewInput, m_pLastInput);
	if(m_pCharacter)
	{
		const int ActiveWeapon = m_pCharacter->m_Core.m_ActiveWeapon;
		Server()->Input()->ProcessCharacterInput(m_ClientID, ActiveWeapon, pNewInput, m_pLastInput);
	}

	// Reset input when chatting
	if(pNewInput->m_PlayerFlags & PLAYERFLAG_CHATTING)
	{
		if(!(m_PlayerFlags & PLAYERFLAG_CHATTING))
		{
			if(m_pCharacter)
				m_pCharacter->ResetInput();

			m_PlayerFlags = pNewInput->m_PlayerFlags;
		}

		// hidden broadcast priority
		GS()->Broadcast(m_ClientID, BroadcastPriority::HiddenBroadcast, 100, "");
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
	return IsAuthed() ? TEAM_RED : TEAM_SPECTATORS;
}

/* #########################################################################
	FUNCTIONS PLAYER HELPER
######################################################################### */
void CPlayer::ProgressBar(const char* pType, int Level, uint64_t Exp, uint64_t ExpNeeded, uint64_t GainedExp) const
{
	const auto ExpProgress = translate_to_percent(ExpNeeded, Exp);
	const auto GainedExpProgress = translate_to_percent(ExpNeeded, GainedExp);

	// send and format
	const auto ProgressBar = mystd::string::progressBar(100, static_cast<int>(ExpProgress), 10, ":", " ");
	const auto Result = fmt_default("Lv{lv} {type}[{bar}] {~.2}%+{~.3}%({})XP", Level, pType, ProgressBar, ExpProgress, GainedExpProgress, GainedExp);
	GS()->Broadcast(m_ClientID, BroadcastPriority::GamePriority, 100, Result.c_str());
}

/* #########################################################################
	FUNCTIONS PLAYER ACCOUNT
######################################################################### */
const char* CPlayer::GetLanguage() const
{
	return Server()->GetClientLanguage(m_ClientID);
}

void CPlayer::UpdateSharedCharacterData(int Health, int Mana)
{
	auto& TempData = GetSharedData();
	TempData.m_Health = Health;
	TempData.m_Mana = Mana;
}

int CPlayer::GetTotalAttributeValue(AttributeIdentifier AttributeID) const
{
	return m_aStats.contains(AttributeID) ? m_aStats.at(AttributeID) : 0;
}

bool CPlayer::IsAuthed() const
{
	if(CAccountManager::IsActive(m_ClientID))
		return Account()->GetID() > 0;
	return false;
}


int CPlayer::GetMaxHealth() const
{
	auto DefaultHP = DEFAULT_BASE_HP + GetTotalAttributeValue(AttributeIdentifier::HP);
	Account()->GetBonusManager().ApplyBonuses(BONUS_TYPE_HP, &DefaultHP);
	return DefaultHP;
}

int CPlayer::GetMaxMana() const
{
	auto DefaultMP = DEFAULT_BASE_MP + GetTotalAttributeValue(AttributeIdentifier::MP);
	Account()->GetBonusManager().ApplyBonuses(BONUS_TYPE_MP, &DefaultMP);
	return DefaultMP;
}

int64_t CPlayer::GetAfkTime() const
{
	return m_Afk ? ((time_get() - m_LastPlaytime) / time_freq()) - g_Config.m_SvMaxAfkTime : 0;
}

void CPlayer::FormatBroadcastBasicStats(char* pBuffer, int Size, const char* pAppendStr) const
{
	if(!IsAuthed() || !m_pCharacter || m_PlayerFlags & PLAYERFLAG_IN_MENU)
		return;

	// information
	const int LevelPercent = round_to_int(translate_to_percent(computeExperience(Account()->GetLevel()), Account()->GetExperience()));
	const auto ProgressBar = mystd::string::progressBar(100, LevelPercent, 10, ":", " ");
	const auto MaxHP = GetMaxHealth();
	const auto MaxMP = GetMaxMana();
	const auto HP = m_pCharacter->Health();
	const auto MP = m_pCharacter->Mana();
	const auto Bank = Account()->GetBankManager();
	const auto Gold = Account()->GetGold();
	const auto GoldCapacity = Account()->GetGoldCapacity();
	const auto BonusActivitiesStr = Account()->GetBonusManager().GetBonusActivitiesString();

	// result
	auto Result = fmt_localize(m_ClientID, "\n\n\n\n\nLv{}[{}]\nHP {$}/{$}\nMP {$}/{$}\nGold {$} of {$}\nBank {$}",
		Account()->GetLevel(), ProgressBar, HP, MaxHP, MP, MaxMP, Gold, GoldCapacity, Bank);

	// recast heal info
	auto PotionRecastTime = m_aPlayerTick[HealPotionRecast] - Server()->Tick();
	if(PotionRecastTime > 0)
	{
		const auto Seconds = std::max(0, PotionRecastTime / Server()->TickSpeed());
		Result += "\n" + fmt_localize(m_ClientID, "Potion HP recast: {}", Seconds);
	}

	// recast mana info
	PotionRecastTime = m_aPlayerTick[ManaPotionRecast] - Server()->Tick();
	if(PotionRecastTime > 0)
	{
		const auto Seconds = std::max(0, PotionRecastTime / Server()->TickSpeed());
		Result += "\n" + fmt_localize(m_ClientID, "Potion MP recast: {}", Seconds);
	}

	// bonus activities info
	if(!BonusActivitiesStr.empty())
	{
		Result += "\n" + BonusActivitiesStr;
	}

	// normalize lines
	constexpr int MaxLines = 20;
	const auto Lines = std::ranges::count(Result, '\n');
	if(Lines < MaxLines)
	{
		Result.append(MaxLines - Lines, '\n');
	}

	str_format(pBuffer, Size, "%s%-200s", Result.c_str(), pAppendStr);
}

/* #########################################################################
	FUNCTIONS PLAYER PARSING
######################################################################### */
bool CPlayer::ParseVoteOptionResult(int Vote)
{
	// check valid character
	if(!m_pCharacter)
	{
		GS()->Chat(m_ClientID, "Use it when you're not dead!");
		return true;
	}

	// execute is exist vote optional
	auto& voteOptions = CVoteOptional::Data()[m_ClientID];
	if(!voteOptions.empty())
	{
		CVoteOptional* pOptional = &voteOptions.front();
		pOptional->ExecuteVote(Vote == 1);
	}

	if(Vote == 1)
	{
		// dungeon change ready state
		if(GS()->IsWorldType(WorldType::Dungeon))
		{
			auto* pController = dynamic_cast<CGameControllerDungeon*>(GS()->m_pController);
			if(!pController || !pController->GetDungeon() || pController->GetDungeon()->IsPlaying())
				return false;

			GetSharedData().m_TempDungeonReady = !GetSharedData().m_TempDungeonReady;
			GS()->Chat(m_ClientID, "You changed the ready mode to \"{}\"!", GetSharedData().m_TempDungeonReady ? "ready" : "not ready");
			return true;
		}
	}
	else
	{
		// continue dialog
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
	const auto& itemsDescription = CItemDescription::Data();
	dbg_assert(itemsDescription.contains(ID), "invalid referring to the CPlayerItem");

	auto& playerItems = CPlayerItem::Data()[m_ClientID];
	if(!playerItems.contains(ID))
	{
		CPlayerItem(ID, m_ClientID).Init({}, {}, {}, {});
	}

	return &playerItems[ID];
}

CSkill* CPlayer::GetSkill(int SkillID) const
{
	const auto& skillsDescription = CSkillDescription::Data();
	dbg_assert(skillsDescription.contains(SkillID), "invalid referring to the CSkill");

	auto& playerSkills = CSkill::Data()[m_ClientID];
	const auto iter = std::ranges::find_if(playerSkills, [SkillID](const auto* pSkill)
	{
		return pSkill->GetID() == SkillID;
	});

	return (iter == playerSkills.end() ? CSkill::CreateElement(m_ClientID, SkillID) : *iter);
}

CPlayerQuest* CPlayer::GetQuest(QuestIdentifier ID) const
{
	const auto& questsDescription = CQuestDescription::Data();
	dbg_assert(questsDescription.contains(ID), "invalid referring to the CPlayerQuest");

	auto& questData = CPlayerQuest::Data()[m_ClientID];
	if(!questData.contains(ID))
	{
		CPlayerQuest::CreateElement(ID, m_ClientID);
	}

	return questData[ID];
}

std::optional<int> CPlayer::GetEquippedSlotItemID(ItemType EquipType) const
{
	return Account()->GetEquippedSlotItemID(EquipType);
}

bool CPlayer::IsEquippedSlot(ItemType EquipID) const
{
	const auto& ItemIdOpt = GetEquippedSlotItemID(EquipID);
	return ItemIdOpt.has_value();
}


int CPlayer::GetTotalRawAttributeValue(AttributeIdentifier ID) const
{
	int totalValue = 0;

	// counting by modules
	const auto& vPlayerItems = CPlayerItem::Data()[m_ClientID];
	for(const auto& [ItemID, PlayerItem] : vPlayerItems)
	{
		if(PlayerItem.HasItem() && PlayerItem.Info()->HasAttributes() && PlayerItem.GetDurability() > 0 &&
			 PlayerItem.Info()->IsEquipmentModules() && PlayerItem.IsEquipped())
		{
			totalValue += PlayerItem.GetEnchantAttributeValue(ID);
		}
	}

	// lamba for counting by equipment slots
	auto addItemEnchantStats = [&](const std::optional<int>& ItemIdOpt) mutable
	{
		if(ItemIdOpt && vPlayerItems.contains(*ItemIdOpt))
		{
			auto& PlayerItem = vPlayerItems.at(*ItemIdOpt);
			if(PlayerItem.HasItem() && PlayerItem.Info()->HasAttributes() && PlayerItem.GetDurability() > 0 &&
				PlayerItem.Info()->IsEquipmentSlot())
			{
				totalValue += PlayerItem.GetEnchantAttributeValue(ID);
			}
		}
	};

	// counting by shared equipment slots
	for(const auto& [Type, ItemIdOpt] : Account()->GetEquippedSlots().getSlots())
		addItemEnchantStats(ItemIdOpt);

	// counting by active profession
	auto* pActiveProfession = Account()->GetActiveProfession();
	if(pActiveProfession)
	{
		for(const auto& [Type, ItemIdOpt] : pActiveProfession->GetEquippedSlots().getSlots())
			addItemEnchantStats(ItemIdOpt);

		totalValue += pActiveProfession->GetAttributeValue(ID);
		totalValue += std::max(1, translate_to_percent_rest(totalValue, pActiveProfession->GetAmplifier(ID)));
	}

	// counting by other professions
	for(auto& Prof : Account()->GetProfessions())
	{
		if(Prof.IsProfessionType(PROFESSION_TYPE_OTHER))
		{
			for(const auto& [Type, ItemIdOpt] : Prof.GetEquippedSlots().getSlots())
				addItemEnchantStats(ItemIdOpt);

			totalValue += Prof.GetAttributeValue(ID);
		}
	}

	return totalValue;
}

std::optional<float> CPlayer::GetTotalAttributeChance(AttributeIdentifier ID) const
{
	// collect chance sum
	int totalValue = GetTotalAttributeValue(ID);
	auto Result = CAttributeDescription::CalculateChance(ID, totalValue);
	if(!Result)
		return std::nullopt;

	// prepare result
	float max = 0.0f;
	float base = 0.0f;
	switch(ID)
	{
		case AttributeIdentifier::AttackSPD: base = 100.f; max = MAX_PERCENT_ATTRIBUTE_ATTACK_SPEED; break;
		case AttributeIdentifier::AmmoRegen: base = 100.f; max = MAX_PERCENT_ATTRIBUTE_AMMO_REGEN; break;
		case AttributeIdentifier::Vampirism: base = 5.0f; max = MAX_PERCENT_ATTRIBUTE_VAMPIRISM; break;
		case AttributeIdentifier::Crit: base = 5.0f; max = MAX_PERCENT_ATTRIBUTE_CRIT_CHANCE; break;
		case AttributeIdentifier::Lucky: base = 5.0f; max = MAX_PERCENT_ATTRIBUTE_LUCKY; break;
		case AttributeIdentifier::LuckyDropItem: base = 0.0f; max = MAX_PERCENT_ATTRIBUTE_LUCKY_DROP; break;
		default: base = 0.f;
	}

	return std::min(base + (*Result), max);
}

void CPlayer::ShowHealthNickname(int Sec)
{
	m_ShowHealthNicknameTick = Server()->Tick() + (Server()->TickSpeed() * Sec);
}

void CPlayer::ChangeWorld(int WorldID, std::optional<vec2> newWorldPosition)
{
	// reset dungeon temporary data
	auto& tempData = GetSharedData();
	tempData.m_TempDungeonReady = false;

	// if new position is provided, set the teleport position
	if(newWorldPosition.has_value())
	{
		tempData.SetSpawnPosition(newWorldPosition.value());
	}

	// change the player's world
	Account()->m_aHistoryWorld.push_front(WorldID);
	Server()->ChangeWorld(m_ClientID, WorldID);
}

int CPlayer::GetCurrentWorldID() const
{
	return Server()->GetClientWorldID(m_ClientID);
}

const CTeeInfo& CPlayer::GetTeeInfo() const
{
	return Account()->GetTeeInfo();
}

void CPlayer::StartUniversalScenario(const std::string& ScenarioData, int ScenarioID)
{
	if(ScenarioData.empty())
		return;

	// parse scenario
	mystd::json::parse(ScenarioData, [ScenarioID, this](nlohmann::json& pJson)
	{
		std::string ObjElem {};
		switch(ScenarioID)
		{
			case SCENARIO_ON_DIALOG_RECIEVE_OBJECTIVES: ObjElem = "on_recieve_objectives"; break;
			case SCENARIO_ON_DIALOG_COMPLETE_OBJECTIVES: ObjElem = "on_complete_objectives"; break;
			case SCENARIO_ON_END_STEP: ObjElem = "on_end"; break;
			case SCENARIO_ON_ITEM_EQUIP: ObjElem = "on_equip"; break;
			case SCENARIO_ON_ITEM_GOT: ObjElem = "on_got"; break;
			case SCENARIO_ON_ITEM_LOST: ObjElem = "on_lost"; break;
			case SCENARIO_ON_ITEM_UNEQUIP: ObjElem = "on_unequip"; break;
		}

		// start scenario
		const auto& scenarioJsonData = ObjElem.empty() ? pJson : pJson[ObjElem];
		if(!scenarioJsonData.empty())
		{
			GS()->ScenarioPlayerManager()->RegisterScenario<CUniversalScenario>(m_ClientID, scenarioJsonData);
		}
	});
}