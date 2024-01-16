/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "player.h"

#include "gamecontext.h"
#include "engine/shared/config.h"
#include "worldmodes/dungeon.h"

#include "core/components/Accounts/AccountManager.h"
#include "core/components/Accounts/AccountMinerManager.h"
#include "core/components/Bots/BotManager.h"
#include "core/components/Dungeons/DungeonData.h"
#include "core/components/Eidolons/EidolonInfoData.h"
#include "core/components/Guilds/GuildManager.h"
#include "core/components/Quests/QuestManager.h"

#include "core/components/Inventory/ItemData.h"
#include "core/components/Skills/SkillData.h"
#include "core/components/Groups/GroupData.h"
#include "core/components/Worlds/WorldData.h"
#include "core/entities/Tools/draw_board.h"

MACRO_ALLOC_POOL_ID_IMPL(CPlayer, MAX_CLIENTS* ENGINE_MAX_WORLDS + MAX_CLIENTS)

IServer* CPlayer::Server() const { return m_pGS->Server(); };

CPlayer::CPlayer(CGS* pGS, int ClientID) : m_pGS(pGS), m_ClientID(ClientID)
{
	for(short& SortTab : m_aSortTabs)
		SortTab = -1;

	m_EidolonCID = -1;
	m_WantSpawn = true;
	m_SnapHealthTick = 0;
	m_aPlayerTick[Die] = Server()->Tick();
	m_aPlayerTick[Respawn] = Server()->Tick() + Server()->TickSpeed();
	m_PrevTuningParams = *pGS->Tuning();
	m_NextTuningParams = m_PrevTuningParams;
	m_Cooldown.Initilize(ClientID);

	// constructor only for players
	if(m_ClientID < MAX_PLAYERS)
	{
		m_TutorialStep = 1;
		m_LastVoteMenu = NOPE;
		m_CurrentVoteMenu = MENU_MAIN;
		m_ZoneInvertMenu = false;
		m_MoodState = Mood::NORMAL;
		Account()->m_Team = GetStartTeam();
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
		m_Latency.m_AccumMax = maximum(m_Latency.m_AccumMax, Info.m_Latency);
		m_Latency.m_AccumMin = minimum(m_Latency.m_AccumMin, Info.m_Latency);
		Server()->SetClientScore(m_ClientID, Account()->GetLevel());
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

	// update dialog
	m_Dialog.TickUpdate();
	m_Cooldown.Handler();

	// post updated votes if player open menu
	if(m_PlayerFlags & PLAYERFLAG_IN_MENU && IsActivePostVoteList())
		PostVoteList();
}

void CPlayer::PostTick()
{
	// Check if the user is authenticated
	if(IsAuthed())
	{
		// update latency value
		if(Server()->ClientIngame(m_ClientID))
			GetTempData().m_TempPing = m_Latency.m_Min;

		// Execute the effects tick function
		HandleEffects();

		// Handle tuning parameters
		HandleTuningParams();

		// Handle prison
		HandlePrison();
	}

	// Call the function HandleVoteOptionals() to handle any optional vote features.
	HandleVoteOptionals();

	// Handle scoreboard colors
	HandleScoreboardColors();
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


void CPlayer::HandleEffects()
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

void CPlayer::HandleScoreboardColors()
{
	if(m_TickActivedGroupColors > Server()->Tick())
		return;

	// If the player's flags include the PLAYERFLAG_SCOREBOARD flag
	if(m_PlayerFlags & PLAYERFLAG_SCOREBOARD)
	{
		// If the active group colors have not been set yet
		if(!m_ActivedGroupColors)
		{
			// Create two message packers: Msg and MsgLegacy
			CMsgPacker Msg(NETMSGTYPE_SV_TEAMSSTATE);
			CMsgPacker MsgLegacy(NETMSGTYPE_SV_TEAMSSTATELEGACY);
			for(int i = 0; i < VANILLA_MAX_CLIENTS; i++)
			{
				CPlayer* pPlayer = GS()->GetPlayer(i, true);

				// Add the team color of the group to both message packers
				if(pPlayer && pPlayer->Account()->GetGroup())
				{
					Msg.AddInt(pPlayer->Account()->GetGroup()->GetTeamColor());
					MsgLegacy.AddInt(pPlayer->Account()->GetGroup()->GetTeamColor());
				}
				else
				{
					Msg.AddInt(0);
					MsgLegacy.AddInt(0);
				}
			}
			Server()->SendMsg(&Msg, MSGFLAG_VITAL, m_ClientID);

			// Get the client version of the player the client version is between VERSION_DDRACE and VERSION_DDNET_MSG_LEGACY
			int ClientVersion = Server()->GetClientVersion(m_ClientID);
			if(VERSION_DDRACE < ClientVersion && ClientVersion < VERSION_DDNET_MSG_LEGACY)
				Server()->SendMsg(&MsgLegacy, MSGFLAG_VITAL, m_ClientID);

			// Set the active group colors to true
			m_ActivedGroupColors = true;
			m_TickActivedGroupColors = Server()->Tick() + (Server()->TickSpeed() / 4);
		}
	}
	// If the player flags do not have the PLAYERFLAG_SCOREBOARD flag
	else
	{
		// If group colors are active
		if(m_ActivedGroupColors)
		{
			// Create two message packers: Msg and MsgLegacy
			CMsgPacker Msg(NETMSGTYPE_SV_TEAMSSTATE);
			CMsgPacker MsgLegacy(NETMSGTYPE_SV_TEAMSSTATELEGACY);
			for(int i = 0; i < VANILLA_MAX_CLIENTS; i++)
			{
				// Add the team color of the group to both message packers
				Msg.AddInt(0);
				MsgLegacy.AddInt(0);
			}
			Server()->SendMsg(&Msg, MSGFLAG_VITAL, m_ClientID);

			// Get the client version of the player client version is between VERSION_DDRACE and VERSION_DDNET_MSG_LEGACY
			int ClientVersion = Server()->GetClientVersion(m_ClientID);
			if(VERSION_DDRACE < ClientVersion && ClientVersion < VERSION_DDNET_MSG_LEGACY)
				Server()->SendMsg(&MsgLegacy, MSGFLAG_VITAL, m_ClientID);

			// Set the active group colors to false
			m_ActivedGroupColors = false;
			m_TickActivedGroupColors = Server()->Tick() + (Server()->TickSpeed() / 4);
		}
	}
}

// This function is responsible for handling the prison state of a character.
void CPlayer::HandlePrison()
{
	// Check if the account is not prisoned or if the character is not available
	if(!Account()->IsPrisoned() || !GetCharacter())
		return;

	// Check if the player is not already in the jail world
	int JailWorldID = GS()->GetWorldData()->GetJailWorld()->GetID();
	if(GetPlayerWorldID() != JailWorldID)
	{
		// Change the player's world to the jail world
		ChangeWorld(JailWorldID);
		return;
	}

	// Check if the distance between the player's view position and the jail position is greater than 1000 units
	if(distance(m_pCharacter->m_Core.m_Pos, GS()->GetJailPosition()) > 1000.f)
	{
		// Move the player to the jail position
		GetCharacter()->ChangePosition(GS()->GetJailPosition());
		GS()->Chat(m_ClientID, "You are not allowed to leave the prison!");
	}

	// Check if the server tick is a multiple of the tick speed
	if(Server()->Tick() % Server()->TickSpeed() == 0)
	{
		// Decrease the prison seconds for the player's account
		Account()->m_PrisonSeconds--;

		// Broadcast a message to the player indicating the remaining prison seconds
		GS()->Broadcast(m_ClientID, BroadcastPriority::MAIN_INFORMATION, 50, "You will regain your freedom in {INT} seconds as you are being released from prison.", Account()->m_PrisonSeconds);

		// check if the player is not currently in prison
		if(!Account()->m_PrisonSeconds)
		{
			// if not in prison, unprison the player
			Account()->Unprison();
		}
		// if player is in prison, check if it's time to save their account's social status
		else if(Server()->Tick() % (Server()->TickSpeed() * 10) == 0)
		{
			// if it's time to save, call the SaveAccount function with the SAVE_SOCIAL_STATUS flag
			GS()->Mmo()->SaveAccount(this, SAVE_SOCIAL_STATUS);
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
		if(IsAuthed() && m_aPlayerTick[RefreshNickLeveling] > Server()->Tick() && !(m_PlayerFlags & PLAYERFLAG_CHATTING))
		{
			char aBufNicknameLeveling[MAX_NAME_LENGTH];
			str_format(aBufNicknameLeveling, sizeof(aBufNicknameLeveling), "Lv%d %.4s...", Account()->GetLevel(), Server()->ClientName(m_ClientID));

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
		m_aPlayerTick[RefreshClanTitle] = Server()->Tick() + (((m_aClanString[0] == '|') || (clanStringSize - 1 < 10)) ? Server()->TickSpeed() : (Server()->TickSpeed() / 8));

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
	pPlayerInfo->m_Score = Account()->GetLevel();

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
	if(Account()->HasGuild())
	{
		CGuildData* pGuild = Account()->GetGuild();

		Buffer.append(" | ");
		Buffer.append(pGuild->GetName());
		Buffer.append(" : ");
		Buffer.append(Account()->GetGuildMemberData()->GetRank()->GetName());
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

bool BoardHandlerCallback(DrawboardToolEvent Event, CPlayer* pPlayer, const EntityPoint* pPoint, void* pUser)
{
	if(Event == DrawboardToolEvent::ON_START)
	{
		dbg_msg("draw", "on start");
		return true;
	}
	else if(Event == DrawboardToolEvent::ON_END)
	{
		dbg_msg("draw", "on end");
		return true;
	}
	else if(Event == DrawboardToolEvent::ON_POINT_ADD)
	{
		dbg_msg("draw", "on draw");
		return true;
	}
	else if(Event == DrawboardToolEvent::ON_POINT_ERASE)
	{
		dbg_msg("draw", "on erase");
		return true;
	}

	return true;
}

void CPlayer::TryRespawn()
{
	// Declare a variable to store the spawn position
	vec2 SpawnPos;
	int SpawnType = SPAWN_HUMAN; // Default base spawn

	// Check if the player's account is in prison
	if(Account()->IsPrisoned())
	{
		// Set the spawn type to human prison
		SpawnType = SPAWN_HUMAN_PRISON;
	}
	// Check if the last killed by weapon is not WEAPON_WORLD
	else if(GetTempData().m_LastKilledByWeapon != WEAPON_WORLD)
	{
		// Check if the respawn world ID is valid and the player is not already in the respawn world
		const int RespawnWorldID = GS()->GetWorldData()->GetRespawnWorld()->GetID();
		if(RespawnWorldID >= 0 && !GS()->IsPlayerEqualWorld(m_ClientID, RespawnWorldID))
		{
			// Change the player's world to the respawn world
			ChangeWorld(RespawnWorldID);
			return;
		}

		// Set the spawn type to human treatment
		SpawnType = SPAWN_HUMAN_TREATMENT;
	}

	// Check if the controller allows spawning of the given spawn type at the specified position
	if(GS()->m_pController->CanSpawn(SpawnType, &SpawnPos))
	{
		// Check if self-coordinated spawning is possible
		vec2 TeleportPosition = GetTempData().GetTeleportPosition();
		bool TrySelfCordSpawn = !is_negative_vec(TeleportPosition) && !GS()->Collision()->CheckPoint(TeleportPosition);

		// If the game state is not a dungeon and the TrySelfCordSpawn is true
		if(!GS()->IsDungeon() && TrySelfCordSpawn)
		{
			// Set the spawn position to the teleport position
			SpawnPos = TeleportPosition;
		}

		// Create a new character object at the allocated memory cell
		const int AllocMemoryCell = MAX_CLIENTS * GS()->GetWorldID() + m_ClientID;
		m_pCharacter = new(AllocMemoryCell) CCharacter(&GS()->m_World);
		m_pCharacter->Spawn(this, SpawnPos);
		GS()->CreatePlayerSpawn(SpawnPos);
		GetTempData().ClearTeleportPosition();

		if(!GS()->pDrawBoard)
		{
			GS()->pDrawBoard = new CEntityDrawboard(&GS()->m_World, m_pCharacter->m_Core.m_Pos, 600.f);
			GS()->pDrawBoard->RegisterEvent(&BoardHandlerCallback, this);
		}
		if(GS()->pDrawBoard && GS()->pDrawBoard->StartDrawing(this))
			dbg_msg("start draw!", "start draw!");

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
	// update view pos
	if(!m_pCharacter && GetTeam() == TEAM_SPECTATORS)
		m_ViewPos = vec2(pNewInput->m_TargetX, pNewInput->m_TargetY);

	if(m_pCharacter && m_pCharacter->IsAlive())
	{
		// Check if the "Fire" button has been pressed
		if(CountInput(m_pLastInput->m_Fire, pNewInput->m_Fire).m_Presses)
		{
			Server()->AppendEventKeyClick(m_ClientID, KEY_EVENT_FIRE);

			// Set the corresponding key as clicked based on the active weapon
			const int& ActiveWeapon = m_pCharacter->m_Core.m_ActiveWeapon;
			Server()->AppendEventKeyClick(m_ClientID, 1 << (KEY_EVENT_FIRE + ActiveWeapon));
		}

		// Check if the next weapon button was pressed
		if(CountInput(m_pLastInput->m_NextWeapon, pNewInput->m_NextWeapon).m_Presses)
		{
			Server()->AppendEventKeyClick(m_ClientID, KEY_EVENT_NEXT_WEAPON);
		}

		// Check if the previous weapon button was pressed
		if(CountInput(m_pLastInput->m_PrevWeapon, pNewInput->m_PrevWeapon).m_Presses)
		{
			Server()->AppendEventKeyClick(m_ClientID, KEY_EVENT_PREV_WEAPON);
		}

		// Check if the wanted weapon button was pressed
		if(m_pLastInput->m_WantedWeapon != pNewInput->m_WantedWeapon)
		{
			Server()->AppendEventKeyClick(m_ClientID, KEY_EVENT_WANTED_WEAPON);

			// Set the corresponding key as clicked based on the wanted weapon
			const int Weapon = pNewInput->m_WantedWeapon - 1;
			Server()->AppendEventKeyClick(m_ClientID, KEY_EVENT_WANTED_WEAPON << (Weapon + 1));
		}
	}

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

	std::string ProgressBar = Tools::String::progressBar(100, (int)GetLevelProgress, 10, ":", " ");
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
void CPlayer::GiveEffect(const char* Potion, int Sec, float Chance)
{
	if(m_pCharacter && m_pCharacter->IsAlive())
	{
		const float RandomChance = random_float(100.0f);
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

bool CPlayer::GetHiddenMenu(int HideID) const
{
	if(m_aHiddenMenu.find(HideID) != m_aHiddenMenu.end())
		return m_aHiddenMenu.at(HideID);
	return false;
}

bool CPlayer::IsAuthed() const
{
	if(GS()->Mmo()->Account()->IsActive(m_ClientID))
		return Account()->GetID() > 0;
	return false;
}

int CPlayer::GetStartTeam() const
{
	if(IsAuthed())
		return TEAM_RED;
	return TEAM_SPECTATORS;
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

	const int LevelPercent = translate_to_percent((int)computeExperience(Account()->GetLevel()), Account()->GetExperience());
	const int MaximumHealth = GetStartHealth();
	const int MaximumMana = GetStartMana();
	const int Health = m_pCharacter->Health();
	const int Mana = m_pCharacter->Mana();
	const int Gold = GetItem(itGold)->GetValue();

	char aRecastInfo[32] {};
	if(m_aPlayerTick[PotionRecast] > Server()->Tick())
	{
		int Seconds = maximum(0, (m_aPlayerTick[PotionRecast] - Server()->Tick()) / Server()->TickSpeed());
		str_format(aRecastInfo, sizeof(aRecastInfo), "Potion recast: %d", Seconds);
	}

	std::string ProgressBar = Tools::String::progressBar(100, LevelPercent, 10, ":", " ");
	str_format(pBuffer, Size, "\n\n\n\n\nLv%d[%s]\nHP %d/%d\nMP %d/%d\nGold %s\n%s\n\n\n\n\n\n\n\n\n\n\n%-150s",
		Account()->GetLevel(), ProgressBar.c_str(), Health, MaximumHealth, Mana, MaximumMana, get_commas<int>(Gold).c_str(), aRecastInfo, pAppendStr);
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

	if(!GS()->GetVoteOptionalContainer(m_ClientID).empty())
	{
		CVoteEventOptional* pOptional = &GS()->GetVoteOptionalContainer(m_ClientID).front();
		RunEventOptional(Vote, pOptional);
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
		if(Upgrade(Get, &Account()->m_aStats[(AttributeIdentifier)VoteID], &Account()->m_Upgrade, VoteID2, 1000))
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

bool CPlayer::IsClickedKey(int KeyID) const
{
	return Server()->IsKeyClicked(m_ClientID, KeyID);
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

// This function returns the ID of the equipped item with the specified functionality, excluding the specified item ID.
int CPlayer::GetEquippedItemID(ItemFunctional EquipID, int SkipItemID) const
{
	// Iterate through each item
	const auto& playerItems = CPlayerItem::Data()[m_ClientID];
	for(const auto& [itemID, item] : playerItems)
	{
		// Check if the item has an item and is equipped and has the specified functionality and is not the excluded item
		if(item.HasItem() && item.IsEquipped() && item.Info()->IsFunctional(EquipID) && itemID != SkipItemID)
		{
			// Return the item ID
			return itemID;
		}
	}

	// Return -1 if no equipped item with the specified functionality was found
	return -1;
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
		Size += Account()->m_aStats[ID];

	return Size;
}

float CPlayer::GetAttributePercent(AttributeIdentifier ID)
{
	float Percent = 0.0f;
	int Size = GetAttributeSize(ID);

	if(ID == AttributeIdentifier::Vampirism)
		Percent = minimum(8.0f + (float)Size * 0.0015f, 30.0f);
	if(ID == AttributeIdentifier::Crit)
		Percent = minimum(8.0f + (float)Size * 0.0015f, 30.0f);
	if(ID == AttributeIdentifier::Lucky)
		Percent = minimum(5.0f + (float)Size * 0.0015f, 20.0f);
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

// Function to create a vote event with optional parameters
// Takes in a value, duration in seconds, information string, and variable arguments
CVoteEventOptional* CPlayer::CreateVoteOptional(int OptionID, int OptionID2, int Sec, const char* pInformation, ...)
{
	// Create an instance of the CVoteEventOptional class
	CVoteEventOptional Optional;
	Optional.m_CloseTime = time_get() + time_freq() * Sec;
	Optional.m_OptionID = OptionID;
	Optional.m_OptionID2 = OptionID2;

	// Format the information string using localization and variable arguments
	va_list VarArgs;
	va_start(VarArgs, pInformation);
	dynamic_string Buffer;
	Server()->Localization()->Format_VL(Buffer, GetLanguage(), pInformation, VarArgs);
	Optional.m_Description = Buffer.buffer();
	Buffer.clear();
	va_end(VarArgs);

	// Add the vote event to the list of optionals
	GS()->GetVoteOptionalContainer(m_ClientID).push(Optional);

	// Return a pointer to the newly created vote event
	return &GS()->GetVoteOptionalContainer(m_ClientID).back();
}

// This function is a member function of the CPlayer class.
// It is used to run an optional voting event for the player.
void CPlayer::RunEventOptional(int Option, CVoteEventOptional* pOptional)
{
	// Check if pOptional pointer exists and its callback function returns true
	if(pOptional && pOptional->Run(this, Option <= 0 ? false : true))
	{
		// Create a new network message to update the vote status
		CNetMsg_Sv_VoteStatus Msg;
		Msg.m_Total = 1;
		Msg.m_Yes = (Option >= 1 ? 1 : 0);
		Msg.m_No = (Option <= 0 ? 1 : 0);
		Msg.m_Pass = 0;

		// Send the network message to the client with the MSGFLAG_VITAL flag
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, m_ClientID);

		// Set the close time of pOptional to half a second from the current time
		pOptional->m_CloseTime = time_get() + (time_freq() / 2);
	}
}

// Function to handle optional voting options for a player
void CPlayer::HandleVoteOptionals()
{
	// If the list of optionals is empty, return
	if(GS()->GetVoteOptionalContainer(m_ClientID).empty())
		return;

	// Get a pointer to the first optional in the list
	CVoteEventOptional* pOptional = &GS()->GetVoteOptionalContainer(m_ClientID).front();

	// If the optional is not already being processed
	if(!pOptional->m_Working)
	{
		// Create a vote set message and send the message to the client
		CNetMsg_Sv_VoteSet Msg;
		Msg.m_Timeout = (pOptional->m_CloseTime - time_get()) / time_freq();
		Msg.m_pDescription = pOptional->m_Description.c_str();
		Msg.m_pReason = "\0";
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, m_ClientID);

		// Mark the optional as being processed
		pOptional->m_Working = true;
	}

	// If the closing time of the optional has not passed yet
	if(pOptional->m_CloseTime < time_get())
	{
		// Create a vote set message with timeout 0 and send the message to the client
		CNetMsg_Sv_VoteSet Msg;
		Msg.m_Timeout = 0;
		Msg.m_pDescription = "";
		Msg.m_pReason = "";
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, m_ClientID);

		// Remove the first optional from the list
		GS()->GetVoteOptionalContainer(m_ClientID).pop();
	}
}