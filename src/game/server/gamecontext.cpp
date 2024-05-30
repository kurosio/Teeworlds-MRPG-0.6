/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "gamecontext.h"

#include <engine/storage.h>
#include <engine/map.h>

#include "worldmodes/dungeon.h"
#include "worldmodes/default.h"
#include "worldmodes/tutorial.h"

#include "entity_manager.h"
#include "core/command_processor.h"
#include "core/utilities/pathfinder.h"
#include "core/entities/items/drop_items.h"

#include "core/components/Accounts/AccountManager.h"
#include "core/components/Bots/BotManager.h"
#include "core/components/guilds/guild_manager.h"
#include "core/components/Quests/QuestManager.h"
#include "core/components/skills/skill_manager.h"

#include "core/components/Eidolons/EidolonInfoData.h"
#include "core/components/mails/mail_wrapper.h"
#include "core/components/worlds/world_data.h"
#include "core/utilities/vote_wrapper.h"

CGS::CGS()
{
	for(auto& pBroadcastState : m_aBroadcastStates)
	{
		pBroadcastState.m_NoChangeTick = 0;
		pBroadcastState.m_LifeSpanTick = 0;
		pBroadcastState.m_NextMessage[0] = 0;
		pBroadcastState.m_TimedMessage[0] = 0;
		pBroadcastState.m_PrevMessage[0] = 0;
		pBroadcastState.m_NextPriority = BroadcastPriority::LOWER;
	}

	for(auto& apPlayer : m_apPlayers)
		apPlayer = nullptr;

	m_pServer = nullptr;
	m_pController = nullptr;
	m_pMmoController = nullptr;
	m_pCommandProcessor = nullptr;
	m_pPathFinder = nullptr;
	m_pLayers = nullptr;
	m_pEntityManager = nullptr;
}

CGS::~CGS()
{
	m_Events.Clear();
	for(auto& pEffects : ms_aEffects)
		pEffects.clear();
	for(auto& apPlayer : m_apPlayers)
	{
		delete apPlayer;
		apPlayer = nullptr;
	}

	delete m_pController;
	delete m_pMmoController;
	delete m_pCommandProcessor;
	delete m_pPathFinder;
	delete m_pLayers;
	delete m_pEntityManager;
}

class CCharacter* CGS::GetPlayerChar(int ClientID) const
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || !m_apPlayers[ClientID])
		return nullptr;
	return m_apPlayers[ClientID]->GetCharacter();
}
CPlayer* CGS::GetPlayer(int ClientID, bool CheckAuthed, bool CheckCharacter) const
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || !m_apPlayers[ClientID])
		return nullptr;

	CPlayer* pPlayer = m_apPlayers[ClientID];
	if(CheckAuthed && !pPlayer->IsAuthed())
		return nullptr;
	if(CheckCharacter && !pPlayer->GetCharacter())
		return nullptr;
	return pPlayer;
}

CPlayer* CGS::GetPlayerByUserID(int AccountID) const
{
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		CPlayer* pPlayer = GetPlayer(i, true);
		if(pPlayer && pPlayer->Account()->GetID() == AccountID)
		{
			int WorldID = pPlayer->GetPlayerWorldID();
			CGS* pGS = (CGS*)Instance::GameServer(WorldID);
			return pGS->GetPlayer(i, true);
		}
	}

	return nullptr;
}

CItemDescription* CGS::GetItemInfo(ItemIdentifier ItemID) const
{
	dbg_assert(CItemDescription::Data().find(ItemID) != CItemDescription::Data().end(), "invalid referring to the CItemDescription");

	return &CItemDescription::Data()[ItemID];
}

CQuestDescription* CGS::GetQuestInfo(QuestIdentifier QuestID) const
{
	return CQuestDescription::Data().find(QuestID) != CQuestDescription::Data().end() ? CQuestDescription::Data()[QuestID] : nullptr;
}

CAttributeDescription* CGS::GetAttributeInfo(AttributeIdentifier ID) const
{
	dbg_assert(CAttributeDescription::Data().find(ID) != CAttributeDescription::Data().end(), "invalid referring to the CAttributeDescription");

	return CAttributeDescription::Data()[ID].get();
}

CQuestsDailyBoard* CGS::GetQuestDailyBoard(int ID) const
{
	dbg_assert(CQuestsDailyBoard::Data().find(ID) != CQuestsDailyBoard::Data().end(), "invalid referring to the CQuestsDailyBoard");

	return &CQuestsDailyBoard::Data()[ID];
}

CWorldData* CGS::GetWorldData(int ID) const
{
	int WorldID = ID == -1 ? GetWorldID() : ID;
	const auto& p = std::find_if(CWorldData::Data().begin(), CWorldData::Data().end(), [WorldID](const WorldDataPtr& p){return WorldID == p->GetID(); });

	return p != CWorldData::Data().end() ? (*p).get() : nullptr;
}

CEidolonInfoData* CGS::GetEidolonByItemID(ItemIdentifier ItemID) const
{
	const auto& p = std::find_if(CEidolonInfoData::Data().begin(), CEidolonInfoData::Data().end(), [ItemID](CEidolonInfoData& p){ return p.GetItemID() == ItemID; });
	return p != CEidolonInfoData::Data().end() ? &(*p) : nullptr;
}

/* #########################################################################
	EVENTS
######################################################################### */
void CGS::CreateDamage(vec2 Pos, int FromCID, int Amount, bool CritDamage, float Angle, int64_t Mask)
{
	float a = 3 * pi / 2 + Angle;
	//float a = get_angle(dir);
	float s = a - pi / 3;
	float e = a + pi / 3;
	for(int i = 0; i < Amount; i++)
	{
		float f = mix(s, e, float(i + 1) / float(Amount + 2));
		CNetEvent_DamageInd* pEvent = (CNetEvent_DamageInd*)m_Events.Create(NETEVENTTYPE_DAMAGEIND, sizeof(CNetEvent_DamageInd), Mask);
		if(pEvent)
		{
			pEvent->m_X = (int)Pos.x;
			pEvent->m_Y = (int)Pos.y;
			pEvent->m_Angle = (int)(f * 256.0f);
		}
	}

	if(CritDamage)
	{
		if(CPlayer* pPlayer = GetPlayer(FromCID, true, true); pPlayer && pPlayer->GetItem(itShowCriticalDamage)->IsEquipped())
			Chat(FromCID, ":: Crit damage: {}p.", Amount);
	}
}

void CGS::CreateHammerHit(vec2 Pos, int64_t Mask)
{
	CNetEvent_HammerHit* pEvent = (CNetEvent_HammerHit*)m_Events.Create(NETEVENTTYPE_HAMMERHIT, sizeof(CNetEvent_HammerHit), Mask);
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
	}
}

void CGS::CreateExplosion(vec2 Pos, int Owner, int Weapon, int MaxDamage)
{
	// create the event
	CNetEvent_Explosion* pEvent = (CNetEvent_Explosion*)m_Events.Create(NETEVENTTYPE_EXPLOSION, sizeof(CNetEvent_Explosion));
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
	}

	// deal damage
	CCharacter* apEnts[MAX_CLIENTS];
	constexpr float Radius = 135.0f;
	constexpr float InnerRadius = 48.0f;
	const int Num = m_World.FindEntities(Pos, Radius, (CEntity**)apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
	for(int i = 0; i < Num; i++)
	{
		vec2 Diff = apEnts[i]->GetPos() - Pos;
		vec2 ForceDir(0, 1);
		const float Length = length(Diff);
		if(Length)
			ForceDir = normalize(Diff) * 1.0f;

		const float Factor = 1 - clamp((Length - InnerRadius) / (Radius - InnerRadius), 0.0f, 1.0f);
		if(const int Damage = (int)(Factor * MaxDamage))
		{
			float Strength;
			if(Owner == -1 || !m_apPlayers[Owner])
				Strength = 0.5f;
			else
				Strength = m_apPlayers[Owner]->m_NextTuningParams.m_ExplosionStrength;

			apEnts[i]->TakeDamage(ForceDir * (Strength * Length), Damage, Owner, Weapon);
		}
	}
}

void CGS::CreatePlayerSpawn(vec2 Pos, int64_t Mask)
{
	CNetEvent_Spawn* ev = (CNetEvent_Spawn*)m_Events.Create(NETEVENTTYPE_SPAWN, sizeof(CNetEvent_Spawn), Mask);
	if(ev)
	{
		ev->m_X = (int)Pos.x;
		ev->m_Y = (int)Pos.y;
	}
}

void CGS::CreateDeath(vec2 Pos, int ClientID, int64_t Mask)
{
	CNetEvent_Death* pEvent = (CNetEvent_Death*)m_Events.Create(NETEVENTTYPE_DEATH, sizeof(CNetEvent_Death), Mask);
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
		pEvent->m_ClientID = ClientID;
	}
}

void CGS::CreateSound(vec2 Pos, int Sound, int64_t Mask)
{
	// fix for vanilla unterstand SoundID
	if(Sound < 0 || Sound > 40)
		return;

	CNetEvent_SoundWorld* pEvent = (CNetEvent_SoundWorld*)m_Events.Create(NETEVENTTYPE_SOUNDWORLD, sizeof(CNetEvent_SoundWorld), Mask);
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
		pEvent->m_SoundID = Sound;
	}
}

void CGS::CreatePlayerSound(int ClientID, int Sound)
{
	// fix for vanilla unterstand SoundID
	if(!m_apPlayers[ClientID] || Sound < 0 || Sound > 40)
		return;

	CNetEvent_SoundWorld* pEvent = (CNetEvent_SoundWorld*)m_Events.Create(NETEVENTTYPE_SOUNDWORLD, sizeof(CNetEvent_SoundWorld), CmaskOne(ClientID));
	if(pEvent)
	{
		pEvent->m_X = (int)m_apPlayers[ClientID]->m_ViewPos.x;
		pEvent->m_Y = (int)m_apPlayers[ClientID]->m_ViewPos.y;
		pEvent->m_SoundID = Sound;
	}
}

void CGS::SendChatTarget(int ClientID, const char* pText) const
{
	CNetMsg_Sv_Chat Msg;
	Msg.m_Team = 0;
	Msg.m_pMessage = pText;
	Msg.m_ClientID = -1;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

/* #########################################################################
	CHAT FUNCTIONS
######################################################################### */
void CGS::SendChat(int ChatterClientID, int Mode, const char* pText)
{
	char aBuf[256];
	if(ChatterClientID >= 0 && ChatterClientID < MAX_CLIENTS)
		str_format(aBuf, sizeof(aBuf), "%d:%d:%s: %s", ChatterClientID, Mode, Server()->ClientName(ChatterClientID), pText);
	else
		str_format(aBuf, sizeof(aBuf), "*** %s", pText);
	Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, Mode == CHAT_TEAM ? "teamchat" : "chat", aBuf);

	CNetMsg_Sv_Chat Msg;
	Msg.m_Team = 0;
	Msg.m_ClientID = ChatterClientID;
	Msg.m_pMessage = pText;

	if(Mode == CHAT_ALL)
	{
		// send discord chat only from players
		if(ChatterClientID < MAX_PLAYERS)
			Server()->SendDiscordMessage(g_Config.m_SvDiscordServerChatChannel, DC_SERVER_CHAT, Server()->ClientName(ChatterClientID), pText);

		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, -1);
	}
	else if(Mode == CHAT_TEAM)
	{
		CPlayer* pChatterPlayer = GetPlayer(ChatterClientID, true);
		if(!pChatterPlayer || !pChatterPlayer->Account()->HasGuild())
		{
			Chat(ChatterClientID, "This chat is for guilds and team members.");
			return;
		}

		// pack one for the recording only
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NOSEND, -1);

		// send chat to guild team
		Msg.m_Team = 1;
		for(int i = 0; i < MAX_PLAYERS; i++)
		{
			CPlayer* pSearchPlayer = GetPlayer(i, true);
			if(pSearchPlayer && pChatterPlayer->Account()->SameGuild(i))
				Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, i);
		}
	}
}

/* #########################################################################
	BROADCAST FUNCTIONS
######################################################################### */
void CGS::AddBroadcast(int ClientID, const char* pText, BroadcastPriority Priority, int LifeSpan)
{
	if(ClientID < 0 || ClientID >= MAX_PLAYERS)
		return;

	if(LifeSpan > 0)
	{
		if(m_aBroadcastStates[ClientID].m_TimedPriority > Priority)
			return;

		str_copy(m_aBroadcastStates[ClientID].m_TimedMessage, pText, sizeof(m_aBroadcastStates[ClientID].m_TimedMessage));
		m_aBroadcastStates[ClientID].m_LifeSpanTick = LifeSpan;
		m_aBroadcastStates[ClientID].m_TimedPriority = Priority;
	}
	else
	{
		if(m_aBroadcastStates[ClientID].m_NextPriority > Priority)
			return;

		str_copy(m_aBroadcastStates[ClientID].m_NextMessage, pText, sizeof(m_aBroadcastStates[ClientID].m_NextMessage));
		m_aBroadcastStates[ClientID].m_NextPriority = Priority;
	}
}

// the tick of the broadcast and his life
void CGS::BroadcastTick(int ClientID)
{
	// Check if the ClientID is valid
	if(ClientID < 0 || ClientID >= MAX_PLAYERS)
		return;

	// Check if the player exists and is in the same world
	if(m_apPlayers[ClientID] && IsPlayerEqualWorld(ClientID))
	{
		// Get the broadcast state for the given client ID
		CBroadcastState& Broadcast = m_aBroadcastStates[ClientID];

		// Check if the broadcast has a lifespan and the timed priority is greater than the next priority
		if(Broadcast.m_LifeSpanTick > 0 && Broadcast.m_TimedPriority > Broadcast.m_NextPriority)
		{
			// Copy the timed message to the next message buffer
			str_copy(Broadcast.m_NextMessage, Broadcast.m_TimedMessage, sizeof(Broadcast.m_NextMessage));
		}

		//Send broadcast only if the message is different, or to fight auto-fading
		if(Broadcast.m_Updated || str_comp(Broadcast.m_PrevMessage, Broadcast.m_NextMessage) != 0 || Broadcast.m_NoChangeTick < Server()->Tick())
		{
			// Check if the timed priority of the broadcast is less than MAIN_INFORMATION
			if(Broadcast.m_TimedPriority < BroadcastPriority::MAIN_INFORMATION)
			{
				// Format the broadcast message with basic player stats and append pAppend
				const char* pAppend = m_apPlayers[ClientID]->m_PlayerFlags & PLAYERFLAG_CHATTING ? "\0" : Broadcast.m_NextMessage;
				m_apPlayers[ClientID]->FormatBroadcastBasicStats(Broadcast.m_aCompleteMsg, sizeof(Broadcast.m_aCompleteMsg), pAppend);
			}
			else
			{
				// Copy aBufAppend into the complete message buffer
				str_copy(Broadcast.m_aCompleteMsg, Broadcast.m_NextMessage, sizeof(Broadcast.m_aCompleteMsg));
			}

			// Create a broadcast net message and send
			CNetMsg_Sv_Broadcast Msg;
			Msg.m_pMessage = Broadcast.m_aCompleteMsg;
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);

			// Reset the complete message buffer and no change tick
			str_copy(Broadcast.m_PrevMessage, Broadcast.m_NextMessage, sizeof(Broadcast.m_PrevMessage));
			Broadcast.m_aCompleteMsg[0] = '\0';
			Broadcast.m_Updated = false;
			Broadcast.m_NoChangeTick = Server()->Tick() + (Server()->TickSpeed() * 3);
		}

		// Update broadcast state
		if(Broadcast.m_LifeSpanTick > 0)
		{
			// Check if the lifespan tick is greater than 0
			Broadcast.m_LifeSpanTick--;
		}

		if(Broadcast.m_LifeSpanTick <= 0)
		{
			// Check if the lifespan tick is less than or equal to 0
			Broadcast.m_TimedMessage[0] = 0;
			Broadcast.m_TimedPriority = BroadcastPriority::LOWER;
		}

		Broadcast.m_NextMessage[0] = 0;
		Broadcast.m_NextPriority = BroadcastPriority::LOWER;
	}
	else
	{
		// Full reset
		m_aBroadcastStates[ClientID].m_LifeSpanTick = 0;
		m_aBroadcastStates[ClientID].m_NextPriority = BroadcastPriority::LOWER;
		m_aBroadcastStates[ClientID].m_TimedPriority = BroadcastPriority::LOWER;
		m_aBroadcastStates[ClientID].m_PrevMessage[0] = 0;
		m_aBroadcastStates[ClientID].m_NextMessage[0] = 0;
		m_aBroadcastStates[ClientID].m_TimedMessage[0] = 0;
		m_aBroadcastStates[ClientID].m_Updated = false;
	}
}

void CGS::MarkUpdatedBroadcast(int ClientID)
{
	if(ClientID >= 0 && ClientID < MAX_PLAYERS)
		m_aBroadcastStates[ClientID].m_Updated = true;
}

/* #########################################################################
	PACKET MESSAGE FUNCTIONS
######################################################################### */
void CGS::SendEmoticon(int ClientID, int Emoticon)
{
	CNetMsg_Sv_Emoticon Msg;
	Msg.m_ClientID = ClientID;
	Msg.m_Emoticon = Emoticon;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1, m_WorldID);
}

void CGS::SendWeaponPickup(int ClientID, int Weapon)
{
	CNetMsg_Sv_WeaponPickup Msg;
	Msg.m_Weapon = Weapon;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGS::SendMotd(int ClientID, const char* pText)
{
	CNetMsg_Sv_Motd Msg;
	Msg.m_pMessage = pText;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGS::SendTuningParams(int ClientID)
{
	CMsgPacker Msg(NETMSGTYPE_SV_TUNEPARAMS);
	int* pParams = (int*)&m_Tuning;
	for(unsigned i = 0; i < sizeof(m_Tuning) / sizeof(int); i++)
		Msg.AddInt(pParams[i]);
	Server()->SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

/* #########################################################################
	ENGINE GAMECONTEXT
######################################################################### */
void CGS::UpdateDiscordStatus()
{
#ifdef CONF_DISCORD
	if(Server()->Tick() % (Server()->TickSpeed() * 10) != 0 || m_WorldID != MAIN_WORLD_ID)
		return;

	int Players = 0;
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		if(Server()->ClientIngame(i))
			Players++;
	}

	if(Players > 0)
	{
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "%d player's play MRPG!", Players);
		Server()->UpdateDiscordStatus(aBuf);
		return;
	}
	Server()->UpdateDiscordStatus("and expects players.");
#endif
}

void CGS::OnInit(int WorldID)
{
	m_pServer = Kernel()->RequestInterface<IServer>();
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	m_pStorage = Kernel()->RequestInterface<IStorageEngine>();
	m_World.SetGameServer(this);
	m_Events.SetGameServer(this);
	m_WorldID = WorldID;

	for(int i = 0; i < NUM_NETOBJTYPES; i++)
		Server()->SnapSetStaticsize(i, m_NetObjHandler.GetObjSize(i));

	// create controller
	m_pLayers = new CLayers();
	m_pLayers->Init(Kernel(), WorldID);
	m_Collision.Init(m_pLayers);
	m_pEntityManager = new CEntityManager(this);
	m_pMmoController = new CMmoController(this);
	m_pMmoController->LoadLogicWorld();

	InitWorldzone();

	// command processor
	m_pCommandProcessor = new CCommandProcessor(this);

	// initialize cores
	CMapItemLayerTilemap* pTileMap = m_pLayers->GameLayer();
	CTile* pTiles = (CTile*)Kernel()->RequestInterface<IMap>(WorldID)->GetData(pTileMap->m_Data);
	for(int y = 0; y < pTileMap->m_Height; y++)
	{
		for(int x = 0; x < pTileMap->m_Width; x++)
		{
			const int Index = pTiles[y * pTileMap->m_Width + x].m_Index;
			if(Index >= ENTITY_OFFSET)
			{
				const vec2 Pos(x * 32.0f + 16.0f, y * 32.0f + 16.0f);
				m_pController->OnEntity(Index - ENTITY_OFFSET, Pos);
			}
		}
	}
	m_pController->CanSpawn(SPAWN_HUMAN_PRISON, &m_JailPosition);

	// initialize pathfinder
	m_pPathFinder = new CPathFinder(m_pLayers, &m_Collision);
	Console()->Chain("sv_motd", ConchainSpecialMotdupdate, this);
}

void CGS::OnConsoleInit()
{
	m_pServer = Kernel()->RequestInterface<IServer>();
	m_pConsole = Kernel()->RequestInterface<IConsole>();

	Console()->Register("set_world_time", "i[hour]", CFGFLAG_SERVER, ConSetWorldTime, m_pServer, "Set worlds time.");
	Console()->Register("itemlist", "", CFGFLAG_SERVER, ConItemList, m_pServer, "items list");
	Console()->Register("giveitem", "i[cid]i[itemid]i[count]i[enchant]i[mail]", CFGFLAG_SERVER, ConGiveItem, m_pServer, "Give item <clientid> <itemid> <count> <enchant> <mail 1=yes 0=no>");
	Console()->Register("removeitem", "i[cid]i[itemid]i[count]", CFGFLAG_SERVER, ConRemItem, m_pServer, "Remove item <clientid> <itemid> <count>");
	Console()->Register("disband_guild", "r[guildname]", CFGFLAG_SERVER, ConDisbandGuild, m_pServer, "Disband the guild with the name");
	Console()->Register("say", "r[text]", CFGFLAG_SERVER, ConSay, m_pServer, "Say in chat");
	Console()->Register("addcharacter", "i[cid]r[botname]", CFGFLAG_SERVER, ConAddCharacter, m_pServer, "(Warning) Add new bot on database or update if finding <clientid> <bot name>");
	Console()->Register("sync_lines_for_translate", "", CFGFLAG_SERVER, ConSyncLinesForTranslate, m_pServer, "Perform sync lines in translated files. Order non updated translated to up");
	Console()->Register("afk_list", "", CFGFLAG_SERVER, ConListAfk, m_pServer, "List all afk players");
	Console()->Register("is_afk", "i[cid]", CFGFLAG_SERVER, ConCheckAfk, m_pServer, "Check if player is afk");

	Console()->Register("ban_acc", "i[cid]s[time]r[reason]", CFGFLAG_SERVER, ConBanAcc, m_pServer, "Ban account, time format: d - days, h - hours, m - minutes, s - seconds, example: 3d15m");
	Console()->Register("unban_acc", "i[banid]", CFGFLAG_SERVER, ConUnBanAcc, m_pServer, "UnBan account, pass ban id from bans_acc");
	Console()->Register("bans_acc", "", CFGFLAG_SERVER, ConBansAcc, m_pServer, "Accounts bans");
}

void CGS::OnDaytypeChange(int NewDaytype)
{
	// update multiplier
	UpdateExpMultiplier();

	// send day info
	const char* pWorldname = Server()->GetWorldName(m_WorldID);
	switch(NewDaytype)
	{
		case MORNING_TYPE:
			ChatWorld(m_WorldID, "", "Rise and shine! The sun has made its triumphant return, banishing the darkness of night. It's time to face the challenges of a brand new day.");
			ChatWorld(m_WorldID, "", "The exp multiplier in the '{}' zone is 100%.", pWorldname);
			break;
		case EVENING_TYPE:
			ChatWorld(m_WorldID, "", "The exp multiplier in the '{}' zone is 100%.", pWorldname);
		break;
		case NIGHT_TYPE:
			ChatWorld(m_WorldID, "", "Nighttime adventure in the '{}' zone has been boosted by {}%!", pWorldname, m_MultiplierExp);
			break;
		default:
			ChatWorld(m_WorldID, "", "The exp multiplier in the '{}' zone is 100%.", pWorldname);
		break;
	}
}

void CGS::OnTick()
{
	m_World.m_Core.m_Tuning = m_Tuning;
	m_World.Tick();
	m_pController->Tick();

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!Server()->ClientIngame(i) || !m_apPlayers[i] || m_apPlayers[i]->GetPlayerWorldID() != m_WorldID)
			continue;

		m_apPlayers[i]->Tick();
		m_apPlayers[i]->PostTick();
		if(i < MAX_PLAYERS)
		{
			BroadcastTick(i);
		}
	}

	Core()->OnTick();
}

void CGS::OnTickGlobal()
{
	// check if it's time to check the player's time period based on the configured interval
	if(Server()->Tick() % (Server()->TickSpeed() * g_Config.m_SvCheckPlayerTimePeriod) == 0)
	{
		for(int i = 0; i < MAX_PLAYERS; i++)
		{
			if(CPlayer* pPlayer = GetPlayer(i, true))
				Core()->HandlePlayerTimePeriod(pPlayer);
		}
	}

	// check if the current tick is a multiple of the specified chat message time interval
	if(Server()->Tick() % (Server()->TickSpeed() * g_Config.m_SvInfoChatMessageTime) == 0)
	{
		// Create a deque (double-ended queue) to hold the chat messages
		std::deque<std::string> ChatMsg
		{
			"[INFO] We recommend that you use the function in F1 console \"ui_close_window_after_changing_setting 1\", this will allow the voting menu not to close after clicking to vote.",
			"[INFO] If you can't see the dialogs with NPCs, check in F1 console \"cl_motd_time\" so that the value is set.",
			"[INFO] Information and data can be found in the call voting menu.",
			"[INFO] The mod supports translation, you can find it in \"Call vote -> Settings -> Settings language\".",
			"[INFO] Don't know what to do? For example, try to find ways to improve your attributes, of which there are more than 25."
		};

		// Select a random chat message from the deque and send it as a chat message to all players (-1)
		Chat(-1, ChatMsg[rand() % ChatMsg.size()].c_str());
	}

	// check if it's time to display the top message based on the configured interval
	if(Server()->Tick() % (Server()->TickSpeed() * g_Config.m_SvInfoChatTopMessageTime) == 0)
	{
		// declare a variable to store the type of top list
		const char* StrTypeName;
		// generate a random top list type
		ToplistType RandomType = (ToplistType)(rand() % (int)ToplistType::NUM_TOPLIST_TYPES);

		// determine the appropriate message based on the random top list type
		switch(RandomType)
		{
			case ToplistType::GUILDS_LEVELING:
				StrTypeName = "---- [Top 5 guilds by leveling] ----";
				break;
			case ToplistType::GUILDS_WEALTHY:
				StrTypeName = "---- [Top 5 guilds by gold] ----";
				break;
			case ToplistType::PLAYERS_LEVELING:
				StrTypeName = "---- [Top 5 players by leveling] ----";
				break;
			default:
				StrTypeName = "---- [Top 5 players by gold] ----";
				break;
		}

		// display the top message in the chat
		Chat(-1, StrTypeName);
		// show the top list to all players
		Core()->ShowTopList(-1, RandomType, 5);
	}

	// discord status
	UpdateDiscordStatus();
}

void CGS::OnSnap(int ClientID)
{
	// check valid player
	CPlayer* pPlayer = m_apPlayers[ClientID];
	if(!pPlayer || pPlayer->GetPlayerWorldID() != GetWorldID())
		return;

	// snap all objects
	m_pController->Snap();
	for(const auto& pIterPlayer : m_apPlayers)
	{
		if(pIterPlayer)
			pIterPlayer->Snap(ClientID);
	}
	m_World.Snap(ClientID);
	m_Events.Snap(ClientID);
}

void CGS::OnPreSnap() {}
void CGS::OnPostSnap()
{
	m_World.PostSnap();
	m_Events.Clear();
}

void CGS::OnMessage(int MsgID, CUnpacker* pUnpacker, int ClientID)
{
	// If the unpacking failed, print a debug message and return
	void* pRawMsg = m_NetObjHandler.SecureUnpackMsg(MsgID, pUnpacker);
	if(!pRawMsg)
	{
		if(g_Config.m_Debug)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "dropped weird message '%s' (%d), failed on '%s'", m_NetObjHandler.GetMsgName(MsgID), MsgID, m_NetObjHandler.FailedMsgOn());
			Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "server", aBuf);
		}

		return;
	}

	// If the player object does not exist (i.e., it is a null pointer), return without doing anything
	CPlayer* pPlayer = m_apPlayers[ClientID];
	if(!pPlayer)
		return;

	if(Server()->ClientIngame(ClientID))
	{
		if(MsgID == NETMSGTYPE_CL_SAY)
		{
			// check last chat tick
			if(pPlayer->m_aPlayerTick[LastChat] > Server()->Tick())
				return;

			// initialize variables
			const auto pMsg = (CNetMsg_Cl_Say*)pRawMsg;
			pPlayer->m_aPlayerTick[LastChat] = Server()->Tick() + Server()->TickSpeed();

			// check msg contains valid UTF-8 characters
			if(!str_utf8_check(pMsg->m_pMessage))
				return;

			// check message
			char firstChar = pMsg->m_pMessage[0];
			if(firstChar == '/')
				CommandProcessor()->ChatCmd(pMsg->m_pMessage, pPlayer);
			else if(firstChar == '#')
				ChatWorld(pPlayer->GetPlayerWorldID(), "Nearby:", "'{}' performed an act '{}'.", Server()->ClientName(ClientID), pMsg->m_pMessage);
			else
				SendChat(ClientID, pMsg->m_Team ? CHAT_TEAM : CHAT_ALL, pMsg->m_pMessage);

			// set last message
			str_copy(pPlayer->m_aLastMsg, pMsg->m_pMessage, sizeof(pPlayer->m_aLastMsg));
			return;
		}

		if(MsgID == NETMSGTYPE_CL_CALLVOTE)
		{
			// check last vote tick
			if(pPlayer->m_aPlayerTick[LastVote] > Server()->Tick())
				return;

			// initialize variables
			const auto pMsg = (CNetMsg_Cl_CallVote*)pRawMsg;
			pPlayer->m_aPlayerTick[LastVote] = Server()->Tick() + (Server()->TickSpeed() / 2);

			// check is option type
			if(str_comp_nocase(pMsg->m_pType, "option") == 0)
			{
				// post player votes
				pPlayer->m_VotesData.ApplyVoteUpdaterData();

				// use vote by command with the provided values
				if(CVoteOption* pActionVote = VoteWrapper::GetOptionVoteByAction(ClientID, pMsg->m_pValue))
				{
					const int ReasonNumber = clamp(str_toint(pMsg->m_pReason), 1, 100000);
					if(pActionVote->m_Callback.m_Impl)
						pActionVote->m_Callback.m_Impl(pPlayer, ReasonNumber, pMsg->m_pReason, pActionVote->m_Callback.m_pData);
					else
						OnClientVoteCommand(ClientID, pActionVote->m_aCommand, pActionVote->m_Extra1, pActionVote->m_Extra2, ReasonNumber, pMsg->m_pReason);
				}
			}
			return;
		}

		if(MsgID == NETMSGTYPE_CL_VOTE)
		{
			// update event key
			const auto pMsg = (CNetMsg_Cl_Vote*)pRawMsg;
			if(pMsg->m_Vote == 1)
				Server()->AppendEventKeyClick(ClientID, KEY_EVENT_VOTE_YES);
			else if(pMsg->m_Vote == 0)
				Server()->AppendEventKeyClick(ClientID, KEY_EVENT_VOTE_NO);

			// parse vote option result
			pPlayer->ParseVoteOptionResult(pMsg->m_Vote);
			return;
		}

		if(MsgID == NETMSGTYPE_CL_SETTEAM)
		{
			// check change team last tick
			if(pPlayer->m_aPlayerTick[LastChangeTeam] > Server()->Tick())
				return;

			// initialize variables
			pPlayer->m_aPlayerTick[LastChangeTeam] = Server()->Tick() + Server()->TickSpeed();

			// send broadcast message
			if(!pPlayer->IsAuthed())
				Broadcast(pPlayer->GetCID(), BroadcastPriority::MAIN_INFORMATION, 100, "Use /register <name> <pass>\nOr /login <name> <pass>.");
			else
				Broadcast(ClientID, BroadcastPriority::MAIN_INFORMATION, 100, "Team change is not allowed.");
			return;
		}

		if(MsgID == NETMSGTYPE_CL_SETSPECTATORMODE)
		{
			return;
		}

		if(MsgID == NETMSGTYPE_CL_CHANGEINFO)
		{
			// check last change info tick
			if(pPlayer->m_aPlayerTick[LastChangeInfo] > Server()->Tick())
				return;

			// initialize variables
			auto pMsg = (CNetMsg_Cl_ChangeInfo*)pRawMsg;
			pPlayer->m_aPlayerTick[LastChangeInfo] = Server()->Tick() + (Server()->TickSpeed() * g_Config.m_SvInfoChangeDelay);

			// check valid utf-8 characters
			if(!str_utf8_check(pMsg->m_pClan) || !str_utf8_check(pMsg->m_pSkin))
				return;

			// set client info
			if(pPlayer->IsAuthed())
			{
				// check if the player has an account and the nickname is different
				if(str_comp(Server()->ClientName(ClientID), pMsg->m_pName) != 0)
				{
					pPlayer->m_RequestChangeNickname = true;
					Server()->SetClientNameChangeRequest(ClientID, pMsg->m_pName);
					Broadcast(ClientID, BroadcastPriority::VERY_IMPORTANT, 300,
						"Press F3 to confirm the nickname change to [{}]\n- After the change, you will only be able to log in with the new nickname", pMsg->m_pName);
				}
			}
			else
			{
				Server()->SetClientName(ClientID, pMsg->m_pName);
			}
			Server()->SetClientClan(ClientID, pMsg->m_pClan);
			Server()->SetClientCountry(ClientID, pMsg->m_Country);

			// set player info
			str_copy(pPlayer->GetTeeInfo().m_aSkinName, pMsg->m_pSkin, sizeof(pPlayer->GetTeeInfo().m_aSkinName));
			pPlayer->GetTeeInfo().m_UseCustomColor = pMsg->m_UseCustomColor;
			pPlayer->GetTeeInfo().m_ColorBody = pMsg->m_ColorBody;
			pPlayer->GetTeeInfo().m_ColorFeet = pMsg->m_ColorFeet;
			pPlayer->GetClass()->SetClassSkin(pPlayer->Account()->m_TeeInfos, pPlayer->GetItem(itCustomizer)->IsEquipped());

			// expire server info
			Server()->ExpireServerInfo();
			return;
		}

		if(MsgID == NETMSGTYPE_CL_EMOTICON)
		{
			// check last emote tick
			if(pPlayer->m_aPlayerTick[LastEmote] > Server()->Tick())
				return;

			// initialize variables
			const auto pMsg = (CNetMsg_Cl_Emoticon*)pRawMsg;
			pPlayer->m_aPlayerTick[LastEmote] = Server()->Tick() + (Server()->TickSpeed() / 2);

			// send emoticon and use skills by emoticon
			SendEmoticon(ClientID, pMsg->m_Emoticon);
			Core()->SkillManager()->UseSkillsByEmoticion(pPlayer, pMsg->m_Emoticon);
			return;
		}

		if(MsgID == NETMSGTYPE_CL_KILL)
		{
			// check last self kill tick
			if(pPlayer->m_aPlayerTick[LastSelfKill] > Server()->Tick())
				return;

			// send broadcast message
			pPlayer->m_aPlayerTick[LastSelfKill] = Server()->Tick() + (Server()->TickSpeed() / 2);
			Broadcast(ClientID, BroadcastPriority::MAIN_INFORMATION, 100, "Self kill is not allowed.");
			return;
		}

		if(MsgID == NETMSGTYPE_CL_ISDDNETLEGACY)
		{
			// Get client information from the server
			IServer::CClientInfo Info;
			Server()->GetClientInfo(ClientID, &Info);

			// Check if client has already provided DDNet version
			if(Info.m_GotDDNetVersion)
				return;

			// Get DDNet version from the unpacker
			int DDNetVersion = pUnpacker->GetInt();

			// Check for errors or negative DDNet version to default if there were errors or negative version
			if(pUnpacker->Error() || DDNetVersion < 0)
				DDNetVersion = VERSION_DDRACE;

			// Set the DDNet version for the client on the server side
			Server()->SetClientDDNetVersion(ClientID, DDNetVersion);
			return;
		}

		if(MsgID == NETMSGTYPE_CL_SHOWOTHERSLEGACY)
		{
			dbg_msg("msg", "msg show others legacy cid '%d'", ClientID);
			return;
		}

		if(MsgID == NETMSGTYPE_CL_SHOWOTHERS)
		{
			dbg_msg("msg", "msg show others cid '%d'", ClientID);
			return;
		}

		if(MsgID == NETMSGTYPE_CL_SHOWDISTANCE)
		{
			dbg_msg("msg", "msg show distance cid '%d'", ClientID);
			return;
		}

		// custom
		if(MsgID == NETMSGTYPE_CL_ISMRPGSERVER)
		{
			// check protocol version
			if(const auto pMsg = (CNetMsg_Cl_IsMRPGServer*)pRawMsg; pMsg->m_Version != CURRENT_PROTOCOL_VERSION_MRPG)
			{
				Server()->Kick(ClientID, "Update client use updater or download in discord.");
				return;
			}

			// update protocol version and send good check
			CNetMsg_Sv_AfterIsMRPGServer GoodCheck;
			Server()->SetStateClientMRPG(ClientID, true);
			Server()->SendPackMsg(&GoodCheck, MSGFLAG_VITAL | MSGFLAG_FLUSH | MSGFLAG_NORECORD, ClientID);
		}
	}
	else
	{
		if(MsgID == NETMSGTYPE_CL_STARTINFO)
		{
			// check last change info tick (once use)
			if(pPlayer->m_aPlayerTick[LastChangeInfo] != 0)
				return;

			// initialize variables
			pPlayer->m_aPlayerTick[LastChangeInfo] = Server()->Tick();

			// is authed
			if(pPlayer->IsAuthed())
			{
				CNetMsg_Sv_ReadyToEnter m;
				Server()->SendPackMsg(&m, MSGFLAG_VITAL | MSGFLAG_FLUSH, ClientID);
				pPlayer->m_VotesData.ClearVotes();
				return;
			}

			// set client info
			const auto pMsg = (CNetMsg_Cl_StartInfo*)pRawMsg;
			if(!str_utf8_check(pMsg->m_pName))
			{
				Server()->Kick(ClientID, "name is not valid utf8");
				return;
			}
			if(!str_utf8_check(pMsg->m_pClan))
			{
				Server()->Kick(ClientID, "clan is not valid utf8");
				return;
			}
			if(!str_utf8_check(pMsg->m_pSkin))
			{
				Server()->Kick(ClientID, "skin is not valid utf8");
				return;
			}
			Server()->SetClientName(ClientID, pMsg->m_pName);
			Server()->SetClientClan(ClientID, pMsg->m_pClan);
			Server()->SetClientCountry(ClientID, pMsg->m_Country);

			// set player info
			str_copy(pPlayer->GetTeeInfo().m_aSkinName, pMsg->m_pSkin, sizeof(pPlayer->GetTeeInfo().m_aSkinName));
			pPlayer->GetTeeInfo().m_UseCustomColor = pMsg->m_UseCustomColor;
			pPlayer->GetTeeInfo().m_ColorBody = pMsg->m_ColorBody;
			pPlayer->GetTeeInfo().m_ColorFeet = pMsg->m_ColorFeet;
			pPlayer->GetClass()->SetClassSkin(pPlayer->Account()->m_TeeInfos, pPlayer->GetItem(itCustomizer)->IsEquipped());

			// send ready to enter
			CNetMsg_Sv_ReadyToEnter m;
			Server()->SendPackMsg(&m, MSGFLAG_VITAL | MSGFLAG_FLUSH, ClientID);
			pPlayer->m_VotesData.ClearVotes();

			// expire server info
			Server()->ExpireServerInfo();
		}
	}
}

void CGS::OnClientConnected(int ClientID)
{
	if(!m_apPlayers[ClientID])
	{
		const int AllocMemoryCell = ClientID + m_WorldID * MAX_CLIENTS;
		m_apPlayers[ClientID] = new(AllocMemoryCell) CPlayer(this, ClientID);
	}

	SendMotd(ClientID, g_Config.m_SvMotd);
	m_aBroadcastStates[ClientID] = {};
}

void CGS::OnClientEnter(int ClientID)
{
	CPlayer* pPlayer = m_apPlayers[ClientID];
	if(!pPlayer || pPlayer->IsBot())
		return;

	m_pController->OnPlayerConnect(pPlayer);

	// another
	if(!pPlayer->IsAuthed())
	{
		Chat(-1, "{} entered and joined the MRPG", Server()->ClientName(ClientID));
		ChatDiscord(DC_JOIN_LEAVE, Server()->ClientName(ClientID), "connected and enter in MRPG");

		CMmoController::AsyncClientEnterMsgInfo(Server()->ClientName(ClientID), ClientID);
		ShowVotesNewbieInformation(ClientID);
		return;
	}

	Chat(ClientID, "Welcome to {}! Zone multiplier exp is at {}%.", Server()->GetWorldName(m_WorldID), m_MultiplierExp);
	Core()->AccountManager()->LoadAccount(pPlayer, false);
	Core()->SaveAccount(m_apPlayers[ClientID], SAVE_POSITION);
}

void CGS::OnClientDrop(int ClientID, const char* pReason)
{
	if(!m_apPlayers[ClientID] || m_apPlayers[ClientID]->IsBot())
		return;

	// update clients on drop
	m_pController->OnPlayerDisconnect(m_apPlayers[ClientID]);

	if((Server()->ClientIngame(ClientID) || Server()->IsClientChangesWorld(ClientID)) && IsPlayerEqualWorld(ClientID))
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "leave player='%d:%s'", ClientID, Server()->ClientName(ClientID));
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "game", aBuf);

		Chat(-1, "{} has left the MRPG", Server()->ClientName(ClientID));
		ChatDiscord(DC_JOIN_LEAVE, Server()->ClientName(ClientID), "leave game MRPG");
		Core()->SaveAccount(m_apPlayers[ClientID], SAVE_POSITION);
	}

	delete m_apPlayers[ClientID];
	m_apPlayers[ClientID] = nullptr;
}

void CGS::OnClientDirectInput(int ClientID, void* pInput)
{
	m_apPlayers[ClientID]->OnDirectInput((CNetObj_PlayerInput*)pInput);

	int Flags = ((CNetObj_PlayerInput*)pInput)->m_PlayerFlags;
	if((Flags & 256) || (Flags & 512))
	{
		Server()->Kick(ClientID, "please update your client or use DDNet client");
	}
}

void CGS::OnClientPredictedInput(int ClientID, void* pInput)
{
	m_apPlayers[ClientID]->OnPredictedInput((CNetObj_PlayerInput*)pInput);
}

void CGS::OnUpdateClientServerInfo(nlohmann::json* pJson, int ClientID)
{
	CPlayer* pPlayer = GetPlayer(ClientID);
	if(!pPlayer)
		return;

	CTeeInfo& TeeInfo = pPlayer->GetTeeInfo();
	(*pJson)["skin"]["name"] = TeeInfo.m_aSkinName;
	if(TeeInfo.m_UseCustomColor)
	{
		(*pJson)["skin"]["color_body"] = TeeInfo.m_ColorBody;
		(*pJson)["skin"]["color_feet"] = TeeInfo.m_ColorFeet;
	}
	(*pJson)["afk"] = false;
	(*pJson)["team"] = pPlayer->GetTeam();
}

void CGS::OnClientPrepareChangeWorld(int ClientID)
{
	if(m_apPlayers[ClientID])
	{
		m_apPlayers[ClientID]->KillCharacter(WEAPON_WORLD);
		delete m_apPlayers[ClientID];
		m_apPlayers[ClientID] = nullptr;
	}

	const int AllocMemoryCell = ClientID + m_WorldID * MAX_CLIENTS;
	m_apPlayers[ClientID] = new(AllocMemoryCell) CPlayer(this, ClientID);
}

int CGS::GetRank(int AccountID) const
{
	return Core()->AccountManager()->GetRank(AccountID);
}

bool CGS::IsClientReady(int ClientID) const
{
	CPlayer* pPlayer = GetPlayer(ClientID);
	return pPlayer && pPlayer->m_aPlayerTick[LastChangeInfo] > 0;
}

bool CGS::IsClientPlayer(int ClientID) const
{
	CPlayer* pPlayer = GetPlayer(ClientID);
	return !pPlayer || pPlayer->GetTeam() != TEAM_SPECTATORS;
}

bool CGS::IsClientCharacterExist(int ClientID) const
{
	return GetPlayer(ClientID, false, true) != nullptr;
}

bool CGS::IsClientMRPG(int ClientID) const
{
	return Server()->GetStateClientMRPG(ClientID) || (ClientID >= MAX_PLAYERS && ClientID < MAX_CLIENTS);
}

void* CGS::GetLastInput(int ClientID) const
{
	CPlayer* pPlayer = GetPlayer(ClientID);
	return pPlayer ? (void*)pPlayer->m_pLastInput : nullptr;
}

int CGS::GetClientVersion(int ClientID) const
{
	IServer::CClientInfo Info = { 0 };
	Server()->GetClientInfo(ClientID, &Info);
	return Info.m_DDNetVersion;
}

const char* CGS::Version() const { return GAME_VERSION; }
const char* CGS::NetVersion() const { return GAME_NETVERSION; }

void CGS::OnClearClientData(int ClientID)
{
	Core()->ResetClientData(ClientID);
	VoteWrapper::Data()[ClientID].clear();
	ms_aEffects[ClientID].clear();

	// clear active snap bots for player
	for(auto& pActiveSnap : DataBotInfo::ms_aDataBot)
		pActiveSnap.second.m_aVisibleActive[ClientID] = false;
}

/* #########################################################################
	CONSOLE GAMECONTEXT
######################################################################### */
void CGS::ConSetWorldTime(IConsole::IResult* pResult, void* pUserData)
{
	const int Hour = pResult->GetInteger(0);
	IServer* pServer = (IServer*)pUserData;
	pServer->SetOffsetGameTime(Hour);
}

void CGS::ConItemList(IConsole::IResult* pResult, void* pUserData)
{
	IServer* pServer = (IServer*)pUserData;
	CGS* pSelf = (CGS*)pServer->GameServer(MAIN_WORLD_ID);

	char aBuf[256];
	for(auto& p : CItemDescription::Data())
	{
		str_format(aBuf, sizeof(aBuf), "ID: %d | Name: %s | %s", p.first, p.second.GetName(), p.second.IsEnchantable() ? "Enchantable" : "Default stack");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "item_list", aBuf);
	}
}

// give the item to the player
void CGS::ConGiveItem(IConsole::IResult* pResult, void* pUserData)
{
	const int ClientID = clamp(pResult->GetInteger(0), 0, MAX_PLAYERS - 1);
	const ItemIdentifier ItemID = pResult->GetInteger(1);
	const int Value = pResult->GetInteger(2);
	const int Enchant = pResult->GetInteger(3);
	const int ByMailbox = pResult->GetInteger(4);

	IServer* pServer = (IServer*)pUserData;
	CGS* pSelf = (CGS*)pServer->GameServer(pServer->GetClientWorldID(ClientID));

	CPlayer* pPlayer = pSelf->GetPlayer(ClientID, true);
	if(pPlayer && CItemDescription::Data().find(ItemID) != CItemDescription::Data().end())
	{
		if(ByMailbox == 0)
		{
			pPlayer->GetItem(ItemID)->Add(Value, 0, Enchant);
			return;
		}

		MailWrapper Mail("Console", pPlayer->Account()->GetID(), "The sender heavens.");
		Mail.AddDescLine("Sent from console");
		Mail.AttachItem(CItem(ItemID, Value, Enchant));
		Mail.Send();
	}
}

void CGS::ConDisbandGuild(IConsole::IResult* pResult, void* pUserData)
{
	char aBuf[256];
	IServer* pServer = (IServer*)pUserData;
	CGS* pSelf = (CGS*)pServer->GameServer(MAIN_WORLD_ID);
	const char* pGuildName = pResult->GetString(0);
	CGuild* pGuild = pSelf->Core()->GuildManager()->GetGuildByName(pGuildName);

	if(!pGuild)
	{
		str_format(aBuf, sizeof(aBuf), "\"%s\", no such guild has been found.", pGuildName);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "disbandguild", aBuf);
		return;
	}

	str_format(aBuf, sizeof(aBuf), "Guild with identifier %d and by the name of %s has been disbanded.", pGuild->GetID(), pGuildName);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "disbandguild", aBuf);
	pSelf->Core()->GuildManager()->Disband(pGuild->GetID());
}

void CGS::ConRemItem(IConsole::IResult* pResult, void* pUserData)
{
	const int ClientID = clamp(pResult->GetInteger(0), 0, MAX_PLAYERS - 1);
	const ItemIdentifier ItemID = pResult->GetInteger(1);
	const int Value = pResult->GetInteger(2);

	IServer* pServer = (IServer*)pUserData;
	CGS* pSelf = (CGS*)pServer->GameServer(pServer->GetClientWorldID(ClientID));

	CPlayer* pPlayer = pSelf->GetPlayer(ClientID, true);
	if(pPlayer)
	{
		pPlayer->GetItem(ItemID)->Remove(Value);
	}
}

void CGS::ConSay(IConsole::IResult* pResult, void* pUserData)
{
	IServer* pServer = (IServer*)pUserData;
	CGS* pSelf = (CGS*)pServer->GameServer(MAIN_WORLD_ID);
	pSelf->SendChat(-1, CHAT_ALL, pResult->GetString(0));
}

// add a new bot player to the database
void CGS::ConAddCharacter(IConsole::IResult* pResult, void* pUserData)
{
	const int ClientID = clamp(pResult->GetInteger(0), 0, MAX_PLAYERS - 1);
	IServer* pServer = (IServer*)pUserData;
	CGS* pSelf = (CGS*)pServer->GameServer(pServer->GetClientWorldID(ClientID));

	// we check if there is a player
	if(ClientID < 0 || ClientID >= MAX_PLAYERS || !pSelf->m_apPlayers[ClientID])
		return;

	// add a new kind of bot
	pSelf->Core()->BotManager()->ConAddCharacterBot(ClientID, pResult->GetString(1));
}

// dump dialogs for translate
void CGS::ConSyncLinesForTranslate(IConsole::IResult* pResult, void* pUserData)
{
	IServer* pServer = (IServer*)pUserData;
	CGS* pSelf = (CGS*)pServer->GameServer();

	// dump
	std::thread(&CMmoController::SyncLocalizations, pSelf->m_pMmoController).detach();
}

void CGS::ConListAfk(IConsole::IResult* pResult, void* pUserData)
{
	IServer* pServer = (IServer*)pUserData;
	CGS* pSelf = (CGS*)pServer->GameServer(MAIN_WORLD_ID);

	char aBuf[1024];
	int Counter = 0;
	for(int i = 0; i < MAX_PLAYERS; ++i)
	{
		// check client in-game
		if(pServer->ClientIngame(i))
		{
			// check afk state
			pSelf = (CGS*)pServer->GameServer(pServer->GetClientWorldID(i));
			if(CPlayer* pPlayer = pSelf->GetPlayer(i); pPlayer && pPlayer->IsAfk())
			{
				// write information about afk
				str_format(aBuf, sizeof(aBuf), "id=%d name='%s' afk_time='%ld's", i, pServer->ClientName(i), pPlayer->GetAfkTime());
				pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "AFK", aBuf);
				Counter++;
			}
		}
	}

	// total afk players
	str_format(aBuf, sizeof(aBuf), "%d afk players in total", Counter);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "AFK", aBuf);
}

void CGS::ConCheckAfk(IConsole::IResult* pResult, void* pUserData)
{
	IServer* pServer = (IServer*)pUserData;
	CGS* pSelf = (CGS*)pServer->GameServer(MAIN_WORLD_ID);

	// check client in-game
	int ClientID = pResult->GetInteger(0);
	if(pServer->ClientIngame(ClientID))
	{
		// check afk state
		pSelf = (CGS*)pServer->GameServer(pServer->GetClientWorldID(ClientID));
		if(CPlayer* pPlayer = pSelf->GetPlayer(ClientID); pPlayer && pPlayer->IsAfk())
		{
			// write information about afk
			char aBuf[1024];
			str_format(aBuf, sizeof(aBuf), "id=%d name='%s' afk_time='%ld's", ClientID, pServer->ClientName(ClientID), pPlayer->GetAfkTime());
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "AFK", aBuf);
			return;
		}
	}

	// if not found information about afk
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "AFK", "No such player or he's not afk");
}

void CGS::ConBanAcc(IConsole::IResult* pResult, void* pUserData)
{
	const int ClientID = pResult->GetInteger(0);
	IServer* pServer = (IServer*)pUserData;
	CGS* pSelf = (CGS*)pServer->GameServerPlayer(ClientID);

	// check valid timeperiod
	TimePeriodData time(pResult->GetString(1));
	if(time.isZero())
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "BanAccount", "Time bad formatted or equals zero!");
		return;
	}

	// check player
	CPlayer* pPlayer = pSelf->GetPlayer(ClientID, true);
	if(!pPlayer)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "BanAccount", "Player not found or isn't logged in");
		return;
	}

	// ban account
	pSelf->Core()->AccountManager()->BanAccount(pPlayer, time, pResult->GetString(2));
}

void CGS::ConUnBanAcc(IConsole::IResult* pResult, void* pUserData)
{
	IServer* pServer = (IServer*)pUserData;
	CGS* pSelf = (CGS*)pServer->GameServer();

	// unban account by banid
	pSelf->Core()->AccountManager()->UnBanAccount(pResult->GetInteger(0));
}

void CGS::ConBansAcc(IConsole::IResult* pResult, void* pUserData)
{
	IServer* pServer = (IServer*)pUserData;
	CGS* pSelf = (CGS*)pServer->GameServer(MAIN_WORLD_ID);

	char aBuf[1024];
	int Counter = 0;
	for(const auto& p : pSelf->Core()->AccountManager()->BansAccount())
	{
		// write information about afk
		str_format(aBuf, sizeof(aBuf), "ban_id=%d name='%s' ban_until='%s' reason='%s'", p.id, p.nickname.c_str(), p.until.c_str(), p.reason.c_str());
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "BansAccount", aBuf);
		Counter++;
	}

	// total bans
	str_format(aBuf, sizeof(aBuf), "%d bans in total", Counter);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "BansAccount", aBuf);
}

void CGS::ConchainSpecialMotdupdate(IConsole::IResult* pResult, void* pUserData, IConsole::FCommandCallback pfnCallback, void* pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
	{
		CGS* pSelf = (CGS*)pUserData;
		pSelf->SendMotd(-1, g_Config.m_SvMotd);
	}
}

void CGS::ConchainGameinfoUpdate(IConsole::IResult* pResult, void* pUserData, IConsole::FCommandCallback pfnCallback, void* pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
	{
		return;
	}
}

void CGS::ShowVotesNewbieInformation(int ClientID)
{
	CPlayer* pPlayer = GetPlayer(ClientID);
	if(!pPlayer)
		return;

	VoteWrapper VWelcome(ClientID, VWF_SEPARATE_OPEN, "Hi, new adventurer!");
	VWelcome.MarkList().Add("Information:");
	{
		VWelcome.BeginDepth();
		VWelcome.MarkList().Add("This server is a mmo server. You'll have to finish");
		{
			VWelcome.BeginDepth();
			VWelcome.Add("quests to continue the game. In these quests,");
			VWelcome.Add("you'll have to get items to give to quest npcs.");
			VWelcome.Add("To get a quest, you need to talk to NPCs.");
			VWelcome.Add("You talk to them by hammering them.");
			VWelcome.Add("You give these items by talking them again. ");
			VWelcome.Add("Hearts and Shields around you show the position");
			VWelcome.Add("quests' npcs. Hearts show Main quest, Shields show Others.");
			VWelcome.EndDepth();
		}
		VWelcome.MarkList().Add("Don't ask other people to give you the items,");
		{
			VWelcome.BeginDepth();
			VWelcome.Add("but you can ask for some help. Keep in mind that");
			VWelcome.EndDepth();
		}
		VWelcome.MarkList().Add("You can see that your shield");
		{
			VWelcome.BeginDepth();
			VWelcome.Add("(below your health bar) doesn't protect you,");
			VWelcome.Add("it's because it's not shield, it's mana.");
			VWelcome.Add("It is used for active skills, which you will need to buy");
			VWelcome.Add("in the future. Active skills use mana, but they use %% of mana.");
			VWelcome.EndDepth();
		}
		VWelcome.EndDepth();
	}
	VWelcome.AddLine();
	VWelcome.MarkList().Add("Dev:");
	{
		VWelcome.BeginDepth();
		VWelcome.Add("Test information");
		VWelcome.EndDepth();
	}
}

void CGS::UpdateExpMultiplier()
{
	// is dungeon
	if(Server()->GetWorldDetail(m_WorldID)->GetType() == WorldType::Dungeon)
	{
		m_MultiplierExp = g_Config.m_SvMultiplierExpRaidDungeon;
		return;
	}

	// is nighttype
	if(Server()->GetCurrentTypeday() == NIGHT_TYPE)
		m_MultiplierExp = (100 + maximum(20, rand() % 200));
	else
		m_MultiplierExp = 100;
}

// strong update votes variability of the data
void CGS::UpdateVotesIfForAll(int MenuList)
{
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		if(m_apPlayers[i] && m_apPlayers[i]->m_VotesData.GetCurrentMenuID() == MenuList)
			m_apPlayers[i]->m_VotesData.UpdateVotes(MenuList);
	}
}

// vote parsing of all functions of action methods
bool CGS::OnClientVoteCommand(int ClientID, const char* pCmd, const int Extra1, const int Extra2, int ReasonNumber, const char* pReason)
{
	CPlayer* pPlayer = GetPlayer(ClientID, false, true);
	if(!pPlayer)
	{
		Chat(ClientID, "Deploy it while still alive!");
		return true;
	}

	// parsing default vote commands
	if(pPlayer->m_VotesData.DefaultVoteCommands(pCmd, Extra1, Extra2, ReasonNumber, pReason))
		return true;

	// parsing everything else
	const auto csqlReason = sqlstr::CSqlString<64>(pReason);
	return Core()->OnPlayerVoteCommand(pPlayer, pCmd, Extra1, Extra2, ReasonNumber, csqlReason.cstr());
}

int CGS::CreateBot(short BotType, int BotID, int SubID)
{
	int BotClientID = MAX_PLAYERS;
	while(m_apPlayers[BotClientID])
	{
		BotClientID++;
		if(BotClientID >= MAX_CLIENTS)
			return -1;
	}

	Server()->InitClientBot(BotClientID);
	const int AllocMemoryCell = BotClientID + m_WorldID * MAX_CLIENTS;
	m_apPlayers[BotClientID] = new(AllocMemoryCell) CPlayerBot(this, BotClientID, BotID, SubID, BotType);
	return BotClientID;
}

bool CGS::TakeItemCharacter(int ClientID)
{
	CPlayer* pPlayer = GetPlayer(ClientID, true, true);
	if(!pPlayer)
		return false;

	std::vector<CDropItem*> vDrops;
	for(CEntity* item : m_World.FindEntities(pPlayer->GetCharacter()->m_Core.m_Pos, 64, 64, CGameWorld::ENTTYPE_ITEM_DROP))
		vDrops.push_back((CDropItem*)item);

	for(const auto& pDrop : vDrops)
	{
		if(pDrop && pDrop->TakeItem(ClientID)) { return true; }
	}
	return false;
}

int CGS::GetExpMultiplier(int Experience) const
{
	return translate_to_percent_rest(Experience, (float)m_MultiplierExp);
}

bool CGS::IsWorldType(WorldType Type) const
{
	return Server()->IsWorldType(m_WorldID, Type);
}

void CGS::InitWorldzone()
{
	// initilize world type
	const char* pWorldType;
	CWorldDetail* pWorldDetail = Server()->GetWorldDetail(m_WorldID);
	if(pWorldDetail->GetType() == WorldType::Dungeon)
	{
		m_pController = new CGameControllerDungeon(this);
		pWorldType = "Dungeon";
		m_AllowedPVP = false;
	}
	else if(pWorldDetail->GetType() == WorldType::Tutorial)
	{
		m_pController = new CGameControllerTutorial(this);
		pWorldType = "Tutorial";
		m_AllowedPVP = false;
	}
	else
	{
		m_pController = new CGameControllerDefault(this);
		pWorldType = "Default";
	}

	UpdateExpMultiplier();

	const char* pStatePVP = m_AllowedPVP ? "yes" : "no";
	dbg_msg("world init", "%s(ID: %d) | %s | PvP: %s", Server()->GetWorldName(m_WorldID), m_WorldID, pWorldType, pStatePVP);
}

bool CGS::IsPlayerEqualWorld(int ClientID, int WorldID) const
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || !m_apPlayers[ClientID])
		return false;

	int PlayerWorldID = m_apPlayers[ClientID]->GetPlayerWorldID();
	return PlayerWorldID == (WorldID <= -1 ? m_WorldID : WorldID);
}

bool CGS::IsPlayersNearby(vec2 Pos, float Distance) const
{
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		if(m_apPlayers[i] && IsPlayerEqualWorld(i) && distance(Pos, m_apPlayers[i]->m_ViewPos) <= Distance)
			return true;
	}

	return false;
}

IGameServer* CreateGameServer() { return new CGS; }
