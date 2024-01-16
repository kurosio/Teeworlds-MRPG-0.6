/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "gamecontext.h"

#include <engine/storage.h>
#include <engine/map.h>
#include <engine/shared/config.h>

#include <game/gamecore.h>
#include <game/layers.h>

#include "worldmodes/dungeon.h"
#include "worldmodes/main.h"
#include "worldmodes/tutorial.h"

#include "core/command_processor.h"
#include "core/utilities/PathFinder.h"
#include "core/entities/loltext.h"
#include "core/entities/Items/drop_bonuses.h"
#include "core/entities/Items/drop_items.h"
#include "core/entities/laser_orbite.h"

#include "core/components/Accounts/AccountManager.h"
#include "core/components/Bots/BotManager.h"
#include "core/components/Dungeons/DungeonManager.h"
#include "core/components/Mails/MailBoxManager.h"
#include "core/components/Guilds/GuildManager.h"
#include "core/components/Houses/HouseManager.h"
#include "core/components/Quests/QuestManager.h"
#include "core/components/Skills/SkillManager.h"

#include <cstdarg>

#include "core/components/Eidolons/EidolonInfoData.h"
#include "core/components/Warehouse/WarehouseData.h"
#include "core/components/Worlds/WorldData.h"

// static data that have the same value in different objects
ska::unordered_map < std::string, int > CGS::ms_aEffects[MAX_PLAYERS];
int CGS::m_MultiplierExp = 100;

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
	pDrawBoard = nullptr;
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
			CGS* pGS = (CGS*)Instance::GetServer()->GameServer(WorldID);
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
	dbg_assert(CQuestDescription::Data().find(QuestID) != CQuestDescription::Data().end(), "invalid referring to the CItemDescription");

	return &CQuestDescription::Data()[QuestID];
}

CAttributeDescription* CGS::GetAttributeInfo(AttributeIdentifier ID) const
{
	dbg_assert(CAttributeDescription::Data().find(ID) != CAttributeDescription::Data().end(), "invalid referring to the CAttributeDescription");

	return CAttributeDescription::Data()[ID].get();
}

CWarehouse* CGS::GetWarehouse(int ID) const
{
	dbg_assert(CWarehouse::Data().find(ID) != CWarehouse::Data().end(), "invalid referring to the CWarehouse");

	return &CWarehouse::Data()[ID];
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

CFlyingPoint* CGS::CreateFlyingPoint(vec2 Pos, vec2 InitialVel, int ClientID, int FromID)
{
	return new CFlyingPoint(&m_World, Pos, InitialVel, ClientID, FromID);
}


/* #########################################################################
	EVENTS
######################################################################### */
void CGS::CreateDamage(vec2 Pos, int FromCID, int Amount, bool CritDamage, int64_t Mask)
{
	float a = 3 * 3.14159f / 2 /* + Angle */;
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
			Chat(FromCID, ":: Crit damage: {INT}p.", Amount);
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

// send a formatted message
void CGS::Chat(int ClientID, const char* pText, ...)
{
	const int Start = (ClientID < 0 ? 0 : ClientID);
	const int End = (ClientID < 0 ? MAX_PLAYERS : ClientID + 1);

	CNetMsg_Sv_Chat Msg;
	Msg.m_Team = -1;
	Msg.m_ClientID = -1;

	va_list VarArgs;
	va_start(VarArgs, pText);

	dynamic_string Buffer;
	for(int i = Start; i < End; i++)
	{
		if(m_apPlayers[i])
		{
			Server()->Localization()->Format_VL(Buffer, m_apPlayers[i]->GetLanguage(), pText, VarArgs);

			Msg.m_pMessage = Buffer.buffer();
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, i);
			Buffer.clear();
		}
	}
	va_end(VarArgs);
}

// send to an authorized player
bool CGS::ChatAccount(int AccountID, const char* pText, ...)
{
	CPlayer* pPlayer = GetPlayerByUserID(AccountID);
	if(!pPlayer)
		return false;

	CNetMsg_Sv_Chat Msg;
	Msg.m_Team = -1;
	Msg.m_ClientID = -1;

	va_list VarArgs;
	va_start(VarArgs, pText);

	dynamic_string Buffer;
	Server()->Localization()->Format_VL(Buffer, pPlayer->GetLanguage(), pText, VarArgs);

	Msg.m_pMessage = Buffer.buffer();
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, pPlayer->GetCID());
	Buffer.clear();
	va_end(VarArgs);
	return true;
}

// Send a guild a message
void CGS::ChatGuild(int GuildID, const char* pText, ...)
{
	if(GuildID <= 0)
		return;

	CNetMsg_Sv_Chat Msg;
	Msg.m_Team = -1;
	Msg.m_ClientID = -1;

	va_list VarArgs;
	va_start(VarArgs, pText);

	dynamic_string Buffer;
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		if(CPlayer* pPlayer = GetPlayer(i, true); pPlayer && pPlayer->Account()->HasGuild() && pPlayer->Account()->GetGuild()->GetID() == GuildID)
		{
			Buffer.append("Guild | ");
			Server()->Localization()->Format_VL(Buffer, m_apPlayers[i]->GetLanguage(), pText, VarArgs);

			Msg.m_pMessage = Buffer.buffer();

			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, i);
			Buffer.clear();
		}
	}
	va_end(VarArgs);
}

// Send a message in world
void CGS::ChatWorldID(int WorldID, const char* Suffix, const char* pText, ...)
{
	CNetMsg_Sv_Chat Msg;
	Msg.m_Team = -1;
	Msg.m_ClientID = -1;

	va_list VarArgs;
	va_start(VarArgs, pText);

	dynamic_string Buffer;
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		CPlayer* pPlayer = GetPlayer(i, true);
		if(!pPlayer || !IsPlayerEqualWorld(i, WorldID))
			continue;

		if(Suffix && Suffix[0] != '\0')
		{
			Buffer.append(Suffix);
			Buffer.append(" ");
		}
		Server()->Localization()->Format_VL(Buffer, pPlayer->GetLanguage(), pText, VarArgs);

		Msg.m_pMessage = Buffer.buffer();

		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, i, WorldID);
		Buffer.clear();
	}
	va_end(VarArgs);
}

// Send discord message
void CGS::ChatDiscord(int Color, const char* Title, const char* pText, ...)
{
#ifdef CONF_DISCORD
	va_list VarArgs;
	va_start(VarArgs, pText);

	dynamic_string Buffer;
	Server()->Localization()->Format_VL(Buffer, "en", pText, VarArgs);
	Server()->SendDiscordMessage(g_Config.m_SvDiscordServerChatChannel, Color, Title, Buffer.buffer());
	Buffer.clear();

	va_end(VarArgs);
#endif
}

// Send a discord message to the channel
void CGS::ChatDiscordChannel(const char* pChanel, int Color, const char* Title, const char* pText, ...)
{
#ifdef CONF_DISCORD
	va_list VarArgs;
	va_start(VarArgs, pText);

	dynamic_string Buffer;
	Server()->Localization()->Format_VL(Buffer, "en", pText, VarArgs);
	Server()->SendDiscordMessage(pChanel, Color, Title, Buffer.buffer());
	Buffer.clear();

	va_end(VarArgs);
#endif
}

// Send Motd
void CGS::Motd(int ClientID, const char* Text, ...)
{
	CPlayer* pPlayer = GetPlayer(ClientID, true);
	if(!pPlayer)
		return;

	va_list VarArgs;
	va_start(VarArgs, Text);
	dynamic_string Buffer;
	Server()->Localization()->Format_VL(Buffer, pPlayer->GetLanguage(), Text, VarArgs);

	CNetMsg_Sv_Motd Msg;
	Msg.m_pMessage = Buffer.buffer();
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);

	Buffer.clear();
	va_end(VarArgs);
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

// formatted broadcast
void CGS::Broadcast(int ClientID, BroadcastPriority Priority, int LifeSpan, const char* pText, ...)
{
	const int Start = (ClientID < 0 ? 0 : ClientID);
	const int End = (ClientID < 0 ? MAX_PLAYERS : ClientID + 1);

	va_list VarArgs;
	va_start(VarArgs, pText);
	for(int i = Start; i < End; i++)
	{
		if(m_apPlayers[i])
		{
			dynamic_string Buffer;
			Server()->Localization()->Format_VL(Buffer, m_apPlayers[i]->GetLanguage(), pText, VarArgs);
			AddBroadcast(i, Buffer.buffer(), Priority, LifeSpan);
			Buffer.clear();
		}
	}
	va_end(VarArgs);
}

// formatted world broadcast
void CGS::BroadcastWorldID(int WorldID, BroadcastPriority Priority, int LifeSpan, const char* pText, ...)
{
	va_list VarArgs;
	va_start(VarArgs, pText);
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		if(m_apPlayers[i] && IsPlayerEqualWorld(i, WorldID))
		{
			dynamic_string Buffer;
			Server()->Localization()->Format_VL(Buffer, m_apPlayers[i]->GetLanguage(), pText, VarArgs);
			AddBroadcast(i, Buffer.buffer(), Priority, LifeSpan);
			Buffer.clear();
		}
	}
	va_end(VarArgs);
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

void CGS::SendMotd(int ClientID)
{
	CNetMsg_Sv_Motd Msg;
	Msg.m_pMessage = g_Config.m_SvMotd;
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
	m_pMmoController = new CMmoController(this);
	m_pMmoController->LoadLogicWorld();

	InitZones();

	if(m_WorldID == TUTORIAL_WORLD_ID)
	{
		m_pController = new CGameControllerTutorial(this);
	}
	else if(IsDungeon()) // dungeon game controller
	{
		m_pController = new CGameControllerDungeon(this);
	}
	else // default game controller
	{
		m_pController = new CGameControllerMain(this);
	}

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

	Mmo()->OnTick();
}

// Here we use functions that can have static data or functions that don't need to be called in all worlds
void CGS::OnTickGlobal()
{
	// Check if the day enum type has changed
	if(m_DayEnumType != Server()->GetEnumTypeDay())
	{
		// Update the day enum type
		m_DayEnumType = Server()->GetEnumTypeDay();

		// Check if the day enum type is NIGHT_TYPE
		if(m_DayEnumType == NIGHT_TYPE)
		{
			// Go through each player and handle their time period
			for(int i = 0; i < MAX_PLAYERS; i++)
			{
				if(CPlayer* pPlayer = GetPlayer(i, true))
					Mmo()->HandlePlayerTimePeriod(pPlayer);
			}

			// Set the experience multiplier to a random value within the range [100, 300)
			m_MultiplierExp = 100 + maximum(20, rand() % 200);
		}
		else
		{
			// Set the experience multiplier to 100
			m_MultiplierExp = 100;
		}

		// Send the updated day information to all players
		SendDayInfo(-1);
	}

	// This code sends periodic chat messages in the game server. The messages are displayed to all players. 
	// The code executes every tick, which is determined by the server's tick speed and the specified chat message time interval.
	// Check if the current tick is a multiple of the specified chat message time interval
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
		Mmo()->ShowTopList(-1, RandomType, true, 5);
	}

	// discord status
	UpdateDiscordStatus();
}

// output of all objects
void CGS::OnSnap(int ClientID)
{
	CPlayer* pPlayer = m_apPlayers[ClientID];
	if(!pPlayer || pPlayer->GetPlayerWorldID() != GetWorldID())
		return;

	m_pController->Snap();
	for(auto& arpPlayer : m_apPlayers)
	{
		if(arpPlayer)
			arpPlayer->Snap(ClientID);
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
			// Check if the player has already sent a chat message during this server tick
			if(pPlayer->m_aPlayerTick[LastChat] > Server()->Tick())
				return;

			// Set the tick value for when the player can send another chat message
			pPlayer->m_aPlayerTick[LastChat] = Server()->Tick() + Server()->TickSpeed();

			// Check if the message contains valid UTF-8 characters
			CNetMsg_Cl_Say* pMsg = (CNetMsg_Cl_Say*)pRawMsg;
			if(!str_utf8_check(pMsg->m_pMessage))
				return;

			// If the first character is a forward slash, treat the message as a command
			char firstChar = pMsg->m_pMessage[0];
			if(firstChar == '/')
			{
				CommandProcessor()->ChatCmd(pMsg->m_pMessage, pPlayer);
			}
			// If the first character is a pound sign, send a chat message to the world
			else if(firstChar == '#')
			{
				ChatWorldID(pPlayer->GetPlayerWorldID(), "Nearby:", "'{STR}' performed an act '{STR}'.", Server()->ClientName(ClientID), pMsg->m_pMessage);
			}
			// Otherwise, send a regular chat message
			else
			{
				SendChat(ClientID, pMsg->m_Team ? CHAT_TEAM : CHAT_ALL, pMsg->m_pMessage);
			}

			// Copy the contents of pMsg->m_pMessage to pPlayer->m_aLastMsg
			str_copy(pPlayer->m_aLastMsg, pMsg->m_pMessage, sizeof(pPlayer->m_aLastMsg));
		}

		else if(MsgID == NETMSGTYPE_CL_CALLVOTE)
		{
			// Check if the player has recently voted
			if(pPlayer->m_aPlayerTick[LastVoteTry] > Server()->Tick())
				return;

			// Set a cooldown for voting
			pPlayer->m_aPlayerTick[LastVoteTry] = Server()->Tick() + (Server()->TickSpeed() / 2);

			// Checking if the vote type is "option"
			CNetMsg_Cl_CallVote* pMsg = (CNetMsg_Cl_CallVote*)pRawMsg;
			if(str_comp_nocase(pMsg->m_pType, "option") == 0)
			{
				// If the player has an active post vote list, post it
				if(pPlayer->IsActivePostVoteList())
					pPlayer->PostVoteList();

				// Finding the vote option in the player's vote list
				const auto& iter = std::find_if(m_aPlayerVotes[ClientID].begin(), m_aPlayerVotes[ClientID].end(), [pMsg](const CVoteOptions& vote)
				{
					return (str_comp_nocase(pMsg->m_pValue, vote.m_aDescription) == 0);
				});

				// If the vote option is found
				if(iter != m_aPlayerVotes[ClientID].end())
				{
					// Parsing the vote commands with the provided values
					const int InteractiveValue = string_to_number(pMsg->m_pReason, 1, 10000000);
					ParsingVoteCommands(ClientID, iter->m_aCommand, iter->m_TempID, iter->m_TempID2, InteractiveValue, pMsg->m_pReason);
				}
			}
		}

		else if(MsgID == NETMSGTYPE_CL_VOTE)
		{
			// Parse the vote items from the message using the ParseVoteOptionResult function
			const auto pMsg = (CNetMsg_Cl_Vote*)pRawMsg;
			if(pMsg->m_Vote == 1)
			{
				Server()->AppendEventKeyClick(ClientID, KEY_EVENT_VOTE_YES);
			}
			else if(pMsg->m_Vote == 0)
			{
				Server()->AppendEventKeyClick(ClientID, KEY_EVENT_VOTE_NO);
			}

			pPlayer->ParseVoteOptionResult(pMsg->m_Vote);
		}

		else if(MsgID == NETMSGTYPE_CL_SETTEAM)
		{
			// Check if the player has recently voted to change teams
			if(pPlayer->m_aPlayerTick[LastChangeTeam] > Server()->Tick())
				return;

			// Set a cooldown for change teams
			pPlayer->m_aPlayerTick[LastChangeTeam] = Server()->Tick() + Server()->TickSpeed();

			// Check if the player is not authenticated
			if(!pPlayer->IsAuthed())
			{
				// Display a broadcast message to the player informing them to register or login
				Broadcast(pPlayer->GetCID(), BroadcastPriority::MAIN_INFORMATION, 100, "Use /register <name> <pass>\nOr /login <name> <pass>.");
				return;
			}

			// Inform the player that team change is not allowed
			Broadcast(ClientID, BroadcastPriority::MAIN_INFORMATION, 100, "Team change is not allowed.");
		}

		else if(MsgID == NETMSGTYPE_CL_SETSPECTATORMODE)
		{
			return;
		}

		else if(MsgID == NETMSGTYPE_CL_CHANGEINFO)
		{
			// Check if the last info change for the player has not expired yet
			if(pPlayer->m_aPlayerTick[LastChangeInfo] > Server()->Tick())
				return;

			// Set the tick at which the next info change can occur for the player
			pPlayer->m_aPlayerTick[LastChangeInfo] = Server()->Tick() + (Server()->TickSpeed() * g_Config.m_SvInfoChangeDelay);

			// Check if the clan name and skin name passed in the message are valid UTF-8 strings
			CNetMsg_Cl_ChangeInfo* pMsg = (CNetMsg_Cl_ChangeInfo*)pRawMsg;
			if(!str_utf8_check(pMsg->m_pClan) || !str_utf8_check(pMsg->m_pSkin))
				return;

			// Set tee info
			if(pPlayer->IsAuthed())
			{
				if(str_comp(Server()->ClientName(ClientID), pMsg->m_pName) != 0)
				{
					pPlayer->m_RequestChangeNickname = true;
					Server()->SetClientNameChangeRequest(ClientID, pMsg->m_pName);
					Broadcast(ClientID, BroadcastPriority::VERY_IMPORTANT, 300,
						"Press F3 to confirm the nickname change to [{STR}]\n- After the change, you will only be able to log in with the new nickname", pMsg->m_pName);
				}
			}
			else
			{
				Server()->SetClientName(ClientID, pMsg->m_pName);
			}
			Server()->SetClientClan(ClientID, pMsg->m_pClan);
			Server()->SetClientCountry(ClientID, pMsg->m_Country);

			str_copy(pPlayer->GetTeeInfo().m_aSkinName, pMsg->m_pSkin, sizeof(pPlayer->GetTeeInfo().m_aSkinName));
			pPlayer->GetTeeInfo().m_UseCustomColor = pMsg->m_UseCustomColor;
			pPlayer->GetTeeInfo().m_ColorBody = pMsg->m_ColorBody;
			pPlayer->GetTeeInfo().m_ColorFeet = pMsg->m_ColorFeet;

			Server()->ExpireServerInfo();
		}

		else if(MsgID == NETMSGTYPE_CL_EMOTICON)
		{
			// Check if the player has already used an emoticon in the current tick
			if(pPlayer->m_aPlayerTick[LastEmote] > Server()->Tick())
				return;

			// Set the player's last emoticon tick to the current tick plus half of the tick speed
			pPlayer->m_aPlayerTick[LastEmote] = Server()->Tick() + (Server()->TickSpeed() / 2);

			// Send the received emoticon to the client with the given ClientID
			CNetMsg_Cl_Emoticon* pMsg = (CNetMsg_Cl_Emoticon*)pRawMsg;
			SendEmoticon(ClientID, pMsg->m_Emoticon);

			// Parse any skills associated with the received emoticon for the player
			Mmo()->Skills()->ParseEmoticionSkill(pPlayer, pMsg->m_Emoticon);
		}

		else if(MsgID == NETMSGTYPE_CL_KILL)
		{
			// Check if the last self-kill for the player occurred within the last half second
			if(pPlayer->m_aPlayerTick[LastSelfKill] > Server()->Tick())
				return;

			// Set the tick for the last self-kill to the current tick plus half a second
			pPlayer->m_aPlayerTick[LastSelfKill] = Server()->Tick() + (Server()->TickSpeed() / 2);

			// Broadcast a message to the client indicating that self-kill is not allowed
			Broadcast(ClientID, BroadcastPriority::MAIN_INFORMATION, 100, "Self kill is not allowed.");
			// pPlayer->KillCharacter(WEAPON_SELF);
		}

		else if(MsgID == NETMSGTYPE_CL_ISDDNETLEGACY)
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
		}
		else if(MsgID == NETMSGTYPE_CL_SHOWOTHERSLEGACY)
		{
			// empty
		}
		else if(MsgID == NETMSGTYPE_CL_SHOWOTHERS)
		{
			// empty
		}
		else if(MsgID == NETMSGTYPE_CL_SHOWDISTANCE)
		{
			// empty
		}

		//////////////////////////////////////////////////////////////////////////////////
		///////////// If the client has passed the test, the alternative is to use the client
		else if(MsgID == NETMSGTYPE_CL_ISMRPGSERVER)
		{
			// Cast the received raw message into a CNetMsg_Cl_IsMRPGServer object
			CNetMsg_Cl_IsMRPGServer* pMsg = (CNetMsg_Cl_IsMRPGServer*)pRawMsg;

			// Check if the version of the message matches the protocol version defined as CURRENT_PROTOCOL_VERSION_MRPG
			if(pMsg->m_Version != CURRENT_PROTOCOL_VERSION_MRPG)
			{
				// If the versions don't match, kick the client with a message to update their client
				Server()->Kick(ClientID, "Update client use updater or download in discord.");
				return;
			}

			// Set the client's state as an MRPG server
			Server()->SetStateClientMRPG(ClientID, true);

			// Send a check success message to the client
			CNetMsg_Sv_AfterIsMRPGServer GoodCheck;
			Server()->SendPackMsg(&GoodCheck, MSGFLAG_VITAL | MSGFLAG_FLUSH | MSGFLAG_NORECORD, ClientID);
		}
		/*else
			Mmo()->OnMessage(MsgID, pRawMsg, ClientID);*/
	}
	else
	{
		if(MsgID == NETMSGTYPE_CL_STARTINFO)
		{
			// This gives a initialize single used start info
			// Check if the player's last change info tick is not zero
			if(pPlayer->m_aPlayerTick[LastChangeInfo] != 0)
				return;

			// Set the player's last change info tick to the current server tick
			pPlayer->m_aPlayerTick[LastChangeInfo] = Server()->Tick();

			// set start infos
			if(!pPlayer->IsAuthed())
			{
				CNetMsg_Cl_StartInfo* pMsg = (CNetMsg_Cl_StartInfo*)pRawMsg;
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

				str_copy(pPlayer->Account()->m_TeeInfos.m_aSkinName, pMsg->m_pSkin, sizeof(pPlayer->Account()->m_TeeInfos.m_aSkinName));
				pPlayer->Account()->m_TeeInfos.m_UseCustomColor = pMsg->m_UseCustomColor;
				pPlayer->Account()->m_TeeInfos.m_ColorBody = pMsg->m_ColorBody;
				pPlayer->Account()->m_TeeInfos.m_ColorFeet = pMsg->m_ColorFeet;
			}

			// send clear vote options
			ClearVotes(ClientID);

			// client is ready to enter
			CNetMsg_Sv_ReadyToEnter m;
			Server()->SendPackMsg(&m, MSGFLAG_VITAL | MSGFLAG_FLUSH, ClientID);

			if(!pPlayer->IsAuthed())
			{
				Server()->ExpireServerInfo();
			}
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

	SendMotd(ClientID);
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
		Chat(-1, "{STR} entered and joined the MRPG", Server()->ClientName(ClientID));
		ChatDiscord(DC_JOIN_LEAVE, Server()->ClientName(ClientID), "connected and enter in MRPG");

		CMmoController::AsyncClientEnterMsgInfo(Server()->ClientName(ClientID), ClientID);

		SendDayInfo(ClientID);
		ShowVotesNewbieInformation(ClientID);
		return;
	}

	Mmo()->Account()->LoadAccount(pPlayer, false);
	Mmo()->SaveAccount(m_apPlayers[ClientID], SAVE_POSITION);
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

		Chat(-1, "{STR} has left the MRPG", Server()->ClientName(ClientID));
		ChatDiscord(DC_JOIN_LEAVE, Server()->ClientName(ClientID), "leave game MRPG");
		Mmo()->SaveAccount(m_apPlayers[ClientID], SAVE_POSITION);
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

void CGS::OnUpdatePlayerServerInfo(nlohmann::json* pJson, int ClientID)
{
	CPlayer* pPlayer = GetPlayer(ClientID);
	if(!pPlayer)
		return;

	CTeeInfo& TeeInfo = m_apPlayers[ClientID]->GetTeeInfo();
	(*pJson)["skin"]["name"] = TeeInfo.m_aSkinName;
	if(TeeInfo.m_UseCustomColor)
	{
		(*pJson)["skin"]["color_body"] = TeeInfo.m_ColorBody;
		(*pJson)["skin"]["color_feet"] = TeeInfo.m_ColorFeet;
	}
	(*pJson)["afk"] = false;
	(*pJson)["team"] = m_apPlayers[ClientID]->GetTeam();
}

// change the world
void CGS::PrepareClientChangeWorld(int ClientID)
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

bool CGS::IsClientReady(int ClientID) const
{
	CPlayer* pPlayer = GetPlayer(ClientID);
	return pPlayer && pPlayer->m_aPlayerTick[LastChangeInfo] > 0;
}

bool CGS::IsClientPlayer(int ClientID) const
{
	CPlayer* pPlayer = GetPlayer(ClientID);
	return pPlayer && pPlayer->GetTeam() == TEAM_SPECTATORS ? false : true;
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

// clearing all data at the exit of the client necessarily call once enough
void CGS::ClearClientData(int ClientID)
{
	Mmo()->ResetClientData(ClientID);
	m_aPlayerVotes[ClientID].clear();
	ms_aEffects[ClientID].clear();

	// clear active snap bots for player
	for(auto& pActiveSnap : DataBotInfo::ms_aDataBot)
		pActiveSnap.second.m_aVisibleActive[ClientID] = false;
}

int CGS::GetRank(int AccountID)
{
	return Mmo()->Account()->GetRank(AccountID);
}

/* #########################################################################
	CONSOLE GAMECONTEXT
######################################################################### */
void CGS::ConSetWorldTime(IConsole::IResult* pResult, void* pUserData)
{
	const int Hour = pResult->GetInteger(0);
	IServer* pServer = (IServer*)pUserData;
	pServer->SetOffsetWorldTime(Hour);
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
	const int Mail = pResult->GetInteger(4);

	IServer* pServer = (IServer*)pUserData;
	CGS* pSelf = (CGS*)pServer->GameServer(pServer->GetClientWorldID(ClientID));

	CPlayer* pPlayer = pSelf->GetPlayer(ClientID, true);
	if(pPlayer && CItemDescription::Data().find(ItemID) != CItemDescription::Data().end())
	{
		if(Mail == 0)
		{
			pPlayer->GetItem(ItemID)->Add(Value, 0, Enchant);
			return;
		}
		pSelf->SendInbox("Console", pPlayer, "The sender heavens", "Sent from console", ItemID, Value, Enchant);
	}
}

void CGS::ConDisbandGuild(IConsole::IResult* pResult, void* pUserData)
{
	IServer* pServer = (IServer*)pUserData;
	CGS* pSelf = (CGS*)pServer->GameServer(MAIN_WORLD_ID);
	const char* pGuildName = pResult->GetString(0);
	/*const int GuildID = pSelf->Mmo()->Member()->SearchGuildByName(pGuildName);

	char aBuf[256];
	if(GuildID <= 0)
	{
		str_format(aBuf, sizeof(aBuf), "\"%s\", no such guild has been found.", pGuildName);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "disbandguild", aBuf);
		return;
	}

	str_format(aBuf, sizeof(aBuf), "Guild with identifier %d and by the name of %s has been disbanded.", GuildID, pGuildName);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "disbandguild", aBuf);
	pSelf->Mmo()->Member()->DisbandGuild(GuildID);*/
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
	pSelf->Mmo()->BotsData()->ConAddCharacterBot(ClientID, pResult->GetString(1));
}

// dump dialogs for translate
void CGS::ConSyncLinesForTranslate(IConsole::IResult* pResult, void* pUserData)
{
	IServer* pServer = (IServer*)pUserData;
	CGS* pSelf = (CGS*)pServer->GameServer();

	// dump
	std::thread(&CMmoController::ConAsyncLinesForTranslate, pSelf->m_pMmoController).detach();
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
	pSelf->Mmo()->Account()->BanAccount(pPlayer, time, pResult->GetString(2));
}

void CGS::ConUnBanAcc(IConsole::IResult* pResult, void* pUserData)
{
	IServer* pServer = (IServer*)pUserData;
	CGS* pSelf = (CGS*)pServer->GameServer();

	// unban account by banid
	pSelf->Mmo()->Account()->UnBanAccount(pResult->GetInteger(0));
}

void CGS::ConBansAcc(IConsole::IResult* pResult, void* pUserData)
{
	IServer* pServer = (IServer*)pUserData;
	CGS* pSelf = (CGS*)pServer->GameServer(MAIN_WORLD_ID);

	char aBuf[1024];
	int Counter = 0;
	for(const auto& p : pSelf->Mmo()->Account()->BansAccount())
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
		pSelf->SendMotd(-1);
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

/* #########################################################################
	VOTING MMO GAMECONTEXT
######################################################################### */
void CGS::ClearVotes(int ClientID)
{
	m_aPlayerVotes[ClientID].clear();

	// send vote options
	CNetMsg_Sv_VoteClearOptions ClearMsg;
	Server()->SendPackMsg(&ClearMsg, MSGFLAG_VITAL, ClientID);
}

// add a vote
void CGS::AV(int ClientID, const char* pCmd, const char* pDesc, const int TempInt, const int TempInt2)
{
	if(ClientID < 0 || ClientID >= MAX_PLAYERS || !m_apPlayers[ClientID])
		return;

	char aBufDesc[VOTE_DESC_LENGTH];
	str_copy(aBufDesc, pDesc, sizeof(aBufDesc));
	if(const auto pReplaceIndent = "\u2014\u2014\u2014\u2014\u2014\u2014\u2014\u2014\u2014\u2014\u2014\u2014\u2014";
		aBufDesc[0] == '\0' && !m_aPlayerVotes[ClientID].empty() && str_comp(m_aPlayerVotes[ClientID].back().m_aDescription, pReplaceIndent) != 0)
		str_copy(aBufDesc, pReplaceIndent, sizeof(aBufDesc));
	else if(str_comp(m_apPlayers[ClientID]->GetLanguage(), "ru") == 0 || str_comp(m_apPlayers[ClientID]->GetLanguage(), "uk") == 0)
		str_translation_utf8_to_cp(aBufDesc);

	CVoteOptions Vote;
	str_copy(Vote.m_aDescription, aBufDesc, sizeof(Vote.m_aDescription));
	str_copy(Vote.m_aCommand, pCmd, sizeof(Vote.m_aCommand));
	Vote.m_TempID = TempInt;
	Vote.m_TempID2 = TempInt2;

	if(Vote.m_aDescription[0] != '\0')
		m_aPlayerVotes[ClientID].emplace_back(Vote);
}

// add formatted vote
void CGS::AVL(int ClientID, const char* pCmd, const char* pText, ...)
{
	if(ClientID >= 0 && ClientID < MAX_PLAYERS && m_apPlayers[ClientID])
	{
		va_list VarArgs;
		va_start(VarArgs, pText);

		dynamic_string Buffer;
		if(str_comp(pCmd, "null") != 0)
			Buffer.append("- ");

		Server()->Localization()->Format_VL(Buffer, m_apPlayers[ClientID]->GetLanguage(), pText, VarArgs);
		AV(ClientID, pCmd, Buffer.buffer());
		Buffer.clear();

		va_end(VarArgs);
	}
}

// add formatted vote with color
void CGS::AVH(int ClientID, const int HiddenID, const char* pText, ...)
{
	if(ClientID >= 0 && ClientID < MAX_PLAYERS && m_apPlayers[ClientID])
	{
		va_list VarArgs;
		va_start(VarArgs, pText);

		const bool HiddenTab = (HiddenID >= TAB_STAT) ? m_apPlayers[ClientID]->GetHiddenMenu(HiddenID) : false;
		const char* pSymbols = HiddenTab ? "\u21BA " : "\u27A4 ";

		dynamic_string Buffer;

		Buffer.append(pSymbols);
		Server()->Localization()->Format_VL(Buffer, m_apPlayers[ClientID]->GetLanguage(), pText, VarArgs);
		if(HiddenID > TAB_SETTINGS_MODULES && HiddenID < NUM_TAB_MENU) { Buffer.append(" (Press me for help)"); }

		AV(ClientID, "HIDDEN", Buffer.buffer(), HiddenID, -1);

		Buffer.clear();
		va_end(VarArgs);
	}
}

// add formatted vote as menu
void CGS::AVM(int ClientID, const char* pCmd, const int TempInt, const int HiddenID, const char* pText, ...)
{
	if(ClientID >= 0 && ClientID < MAX_PLAYERS && m_apPlayers[ClientID])
	{
		if((!m_apPlayers[ClientID]->GetHiddenMenu(HiddenID) && HiddenID > TAB_SETTINGS_MODULES) ||
			(m_apPlayers[ClientID]->GetHiddenMenu(HiddenID) && HiddenID <= TAB_SETTINGS_MODULES))
			return;

		va_list VarArgs;
		va_start(VarArgs, pText);

		dynamic_string Buffer;
		if(TempInt != NOPE) { Buffer.append("- "); }

		Server()->Localization()->Format_VL(Buffer, m_apPlayers[ClientID]->GetLanguage(), pText, VarArgs);
		AV(ClientID, pCmd, Buffer.buffer(), TempInt);
		Buffer.clear();
		va_end(VarArgs);
	}
}

// add formatted vote with multiple id's
void CGS::AVD(int ClientID, const char* pCmd, const int TempInt, const int TempInt2, const int HiddenID, const char* pText, ...)
{
	if(ClientID >= 0 && ClientID < MAX_PLAYERS && m_apPlayers[ClientID])
	{
		if((!m_apPlayers[ClientID]->GetHiddenMenu(HiddenID) && HiddenID > TAB_SETTINGS_MODULES) ||
			(m_apPlayers[ClientID]->GetHiddenMenu(HiddenID) && HiddenID <= TAB_SETTINGS_MODULES))
			return;

		va_list VarArgs;
		va_start(VarArgs, pText);

		dynamic_string Buffer;
		if(TempInt != NOPE) { Buffer.append("- "); }

		Server()->Localization()->Format_VL(Buffer, m_apPlayers[ClientID]->GetLanguage(), pText, VarArgs);
		AV(ClientID, pCmd, Buffer.buffer(), TempInt, TempInt2);
		Buffer.clear();
		va_end(VarArgs);
	}
}

void CGS::StartCustomVotes(int ClientID, int LastVoteMenu)
{
	// start without thread
	if(CPlayer* pPlayer = GetPlayer(ClientID, true))
	{
		pPlayer->m_CurrentVoteMenu = CUSTOM_MENU;
		pPlayer->m_LastVoteMenu = LastVoteMenu;
		ClearVotes(ClientID);
	}
}

void CGS::EndCustomVotes(int ClientID)
{
	CallbackUpdateVotes(this, ClientID, CUSTOM_MENU, true);
}

void CGS::CallbackUpdateVotes(CGS* pGS, int ClientID, int Menulist, bool PrepareCustom)
{
	CPlayer* pPlayer = pGS->GetPlayer(ClientID, true);
	if(!pPlayer)
		return;

	std::this_thread::sleep_for(std::chrono::milliseconds(3));
	if(Menulist == CUSTOM_MENU && PrepareCustom)
	{
		// send parsed votes
		for(auto& p : pGS->m_aPlayerVotes[ClientID])
		{
			CNetMsg_Sv_VoteOptionAdd OptionMsg;
			OptionMsg.m_pDescription = p.m_aDescription;
			pGS->Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, ClientID);
		}
		return;
	}

	// parse votes
	pPlayer->m_CurrentVoteMenu = Menulist;
	pGS->ClearVotes(ClientID);
	pGS->Mmo()->OnPlayerHandleMainMenu(ClientID, Menulist);

	// send parsed votes
	for(auto& p : pGS->m_aPlayerVotes[ClientID])
	{
		CNetMsg_Sv_VoteOptionAdd OptionMsg;
		OptionMsg.m_pDescription = p.m_aDescription;
		pGS->Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, ClientID);
	}
}

void CGS::UpdateVotes(int ClientID, int MenuList)
{
	// unfully safe
	if(m_apPlayers[ClientID])
		m_apPlayers[ClientID]->SetPostVoteListCallback(std::bind(&CallbackUpdateVotes, this, ClientID, MenuList, false));
}

// information for unauthorized players
void CGS::ShowVotesNewbieInformation(int ClientID)
{
	CPlayer* pPlayer = GetPlayer(ClientID);
	if(!pPlayer)
		return;

#define TI(_text) AVL(ClientID, "null", _text)
	TI("#### Hi, new adventurer! ####");
	TI("This server is a mmo server. You'll have to finish");
	TI("quests to continue the game. In these quests,");
	TI("you'll have to get items to give to quest npcs.");
	TI("To get a quest, you need to talk to NPCs.");
	TI("You talk to them by hammering them.");
	TI("You give these items by talking them again. ");
	TI("Hearts and Shields around you show the position");
	TI("quests' npcs. Hearts show Main quest, Shields show Others.");
	TI("Don't ask other people to give you the items,");
	TI("but you can ask for some help. Keep in mind that");
	TI("it is hard for everyone too. You can see that your shield");
	TI("(below your health bar) doesn't protect you,");
	TI("it's because it's not shield, it's mana.");
	TI("It is used for active skills, which you will need to buy");
	TI("in the future. Active skills use mana, but they use %% of mana.");
	AV(ClientID, "null");
	TI("#### The upgrades now ####");
	TI("- DMG : Damage");
	TI("- Dexterity : Shooting speed");
	TI("- Crit Dmg : Damage dealt by critical hits");
	TI("- Direct Crit Dmg : Critical chance");
	TI("- Hardness : Health");
	TI("- Lucky : Chance to avoid damage");
	TI("- Piety : Mana");
	TI("- Vampirism : Damage dealt converted into health");
	TI("All things you need is in Vote.");
	TI("Good game !");
#undef TI
}

// strong update votes variability of the data
void CGS::StrongUpdateVotes(int ClientID, int MenuList)
{
	if(m_apPlayers[ClientID] && m_apPlayers[ClientID]->m_CurrentVoteMenu == MenuList)
		UpdateVotes(ClientID, MenuList);
}

// strong update votes variability of the data
void CGS::StrongUpdateVotesForAll(int MenuList)
{
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		if(m_apPlayers[i] && m_apPlayers[i]->m_CurrentVoteMenu == MenuList)
			UpdateVotes(i, MenuList);
	}
}

// the back button adds a back button to the menu (But remember to specify the last menu ID).
void CGS::AddVotesBackpage(int ClientID)
{
	if(!m_apPlayers[ClientID])
		return;

	AV(ClientID, "null");
	AVL(ClientID, "BACK", "◀◀◀◀ Backpage ◀◀◀◀");
}

// print player statistics
void CGS::ShowVotesPlayerStats(CPlayer* pPlayer)
{
	const int ClientID = pPlayer->GetCID();
	AVH(ClientID, TAB_INFO_STAT, "Attributes & chances{STR}", IsDungeon() ? " (Sync)" : "\0");
	for(const auto& [ID, pAttribute] : CAttributeDescription::Data())
	{
		if(!pAttribute->HasDatabaseField())
			continue;

		// if upgrades are cheap, they have a division of statistics
		const int Size = pPlayer->GetAttributeSize(ID);

		// percent data TODO: extract percent attributes
		float Percent = pPlayer->GetAttributePercent(ID);
		if(Percent)
		{
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "(%0.4f%%)", Percent);
			AVM(ClientID, "null", NOPE, TAB_INFO_STAT, "\u2508\u27A4 ({INT}) - {STR} {STR}", Size, pAttribute->GetName(), aBuf);
		}
		else
		{
			AVM(ClientID, "null", NOPE, TAB_INFO_STAT, "\u2508\u27A4 ({INT}) - {STR}", Size, pAttribute->GetName());
		}
	}

	AV(ClientID, "null");
	AVM(ClientID, "null", NOPE, NOPE, "\u02D7\u02CF\u02CB \u2605 \u02CE\u02CA\u02D7 Upgrade Point's: {INT}P", pPlayer->Account()->m_Upgrade);
	AV(ClientID, "null");
}

// display information by currency
void CGS::AddVoteItemValue(int ClientID, ItemIdentifier ItemID, int HideID)
{
	if(CPlayer* pPlayer = GetPlayer(ClientID); pPlayer)
		AVM(ClientID, "null", NOPE, HideID, "You have {VAL} {STR}", pPlayer->GetItem(ItemID)->GetValue(), GetItemInfo(ItemID)->GetName());
}

// vote parsing of all functions of action methods
bool CGS::ParsingVoteCommands(int ClientID, const char* CMD, const int VoteID, const int VoteID2, int Get, const char* Text)
{
	CPlayer* pPlayer = GetPlayer(ClientID, false, true);
	if(!pPlayer)
	{
		Chat(ClientID, "Deploy it while still alive!");
		return true;
	}

	if(PPSTR(CMD, "null") == 0)
		return true;

	CreatePlayerSound(ClientID, SOUND_BODY_LAND);

	if(PPSTR(CMD, "MENU") == 0)
	{
		pPlayer->m_TempMenuValue = VoteID2;
		UpdateVotes(ClientID, VoteID);
		return true;
	}
	if(PPSTR(CMD, "ZONE_INVERT_MENU") == 0)
	{
		pPlayer->m_ZoneInvertMenu ^= true;
		StrongUpdateVotes(ClientID, pPlayer->m_CurrentVoteMenu);
		return true;
	}
	if(PPSTR(CMD, "SORTEDTOP") == 0)
	{
		pPlayer->m_aSortTabs[SORT_TOP] = VoteID;
		StrongUpdateVotes(ClientID, MENU_TOP_LIST);
		return true;
	}
	if(PPSTR(CMD, "SORTEDWIKIWORLD") == 0)
	{
		pPlayer->m_aSortTabs[SORT_GUIDE_WORLD] = VoteID;
		StrongUpdateVotes(ClientID, MENU_GUIDE_GRINDING);
		return true;
	}
	if(pPlayer->ParseVoteUpgrades(CMD, VoteID, VoteID2, Get))
		return true;

	// parsing everything else
	const CSqlString<64> FormatText = sqlstr::CSqlString<64>(Text);
	return Mmo()->OnParsingVoteCommands(pPlayer, CMD, VoteID, VoteID2, Get, FormatText.cstr());
}

/* #########################################################################
	MMO GAMECONTEXT
######################################################################### */
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

// create lol text in the world
bool CGS::CreateText(CEntity* pParent, bool Follow, vec2 Pos, vec2 Vel, int Lifespan, const char* pText)
{
	if(!IsPlayersNearby(Pos, 800))
		return false;

	CLoltext Text;
	Text.Create(&m_World, pParent, Pos, Vel, Lifespan, pText, true, Follow);
	return true;
}

// creates a particle of experience that follows the player
void CGS::CreateParticleExperience(vec2 Pos, int ClientID, int Experience, vec2 Force)
{
	CFlyingPoint* pPoint = CreateFlyingPoint(Pos, Force, ClientID);
	pPoint->Register([Experience](CFlyingPoint*, CPlayer*, CPlayer* pPlayer)
	{
		pPlayer->Account()->AddExperience(Experience);
	});
}

// gives a bonus in the position type and quantity and the number of them.
void CGS::CreateDropBonuses(vec2 Pos, int Type, int Value, int NumDrop, vec2 Force)
{
	for(int i = 0; i < NumDrop; i++)
	{
		vec2 Vel = Force;
		Vel.x += random_float(15.0f);
		Vel.y += random_float(15.0f);
		new CDropBonuses(&m_World, Pos, Vel, Type, Value);
	}
}

// lands items in the position type and quantity and their number themselves
void CGS::CreateDropItem(vec2 Pos, int ClientID, CItem DropItem, vec2 Force)
{
	if(DropItem.GetID() <= 0 || DropItem.GetValue() <= 0)
		return;

	const float Angle = angle(normalize(Force));
	new CDropItem(&m_World, Pos, Force, Angle, DropItem, ClientID);
}

// random drop of the item with percentage
void CGS::CreateRandomDropItem(vec2 Pos, int ClientID, float Chance, CItem DropItem, vec2 Force)
{
	const float RandomDrop = random_float(100.0f);
	if(RandomDrop < Chance)
		CreateDropItem(Pos, ClientID, DropItem, Force);
}

bool CGS::TakeItemCharacter(int ClientID)
{
	CPlayer* pPlayer = GetPlayer(ClientID, true, true);
	if(!pPlayer)
		return false;

	std::vector<CDropItem*> vDrops;
	for(CEntity* item : m_World.FindEntities(pPlayer->GetCharacter()->m_Core.m_Pos, 64, 64, CGameWorld::ENTTYPE_DROPITEM))
		vDrops.push_back((CDropItem*)item);

	for(const auto& pDrop : vDrops)
	{
		if(pDrop && pDrop->TakeItem(ClientID)) { return true; }
	}
	return false;
}

// send a message with or without the object using ClientID
void CGS::SendInbox(const char* pFrom, CPlayer* pPlayer, const char* Name, const char* Desc, ItemIdentifier ItemID, int Value, int Enchant)
{
	if(!pPlayer || !pPlayer->IsAuthed())
		return;

	SendInbox(pFrom, pPlayer->Account()->GetID(), Name, Desc, ItemID, Value, Enchant);
}

// send a message with or without the object using AccountID
void CGS::SendInbox(const char* pFrom, int AccountID, const char* Name, const char* Desc, ItemIdentifier ItemID, int Value, int Enchant)
{
	Mmo()->Inbox()->SendInbox(pFrom, AccountID, Name, Desc, ItemID, Value, Enchant);
}

void CGS::CreateLaserOrbite(CEntity* pEntParent, int Amount, EntLaserOrbiteType Type, float Speed, float Radius, int LaserType, int64_t Mask)
{
	if(pEntParent)
		new CLaserOrbite(&m_World, -1, pEntParent, Amount, Type, Speed, Radius, LaserType, Mask);
}

void CGS::CreateLaserOrbite(int ClientID, int Amount, EntLaserOrbiteType Type, float Speed, float Radius, int LaserType, int64_t Mask)
{
	if(const CPlayer* pPlayer = GetPlayer(ClientID, false, true); pPlayer)
		new CLaserOrbite(&m_World, ClientID, nullptr, Amount, Type, Speed, Radius, LaserType, Mask);
}

CLaserOrbite* CGS::CreateLaserOrbite(CEntity* pEntParent, int Amount, EntLaserOrbiteType Type, float Radius, int LaserType, int64_t Mask)
{
	if(pEntParent)
		return new CLaserOrbite(&m_World, -1, pEntParent, Amount, Type, 0.f, Radius, LaserType, Mask);
	return nullptr;
}

// send day information
void CGS::SendDayInfo(int ClientID)
{
	if(ClientID == -1)
	{
		switch(m_DayEnumType)
		{
			case MORNING_TYPE: Chat(-1, "Rise and shine! The sun has made its triumphant return, banishing the darkness of night. It's time to face the challenges of a brand new day."); break;
			case EVENING_TYPE: Chat(-1, "The sun has set, and the night sky has taken over."); break;
			case NIGHT_TYPE: Chat(-1, "Night has fallen!"); break;
			default:break;
		}
	}

	if(m_DayEnumType == NIGHT_TYPE)
	{
		Chat(ClientID, "Nighttime adventure has been boosted by {INT}%!", m_MultiplierExp);
	}
	else if(m_DayEnumType == MORNING_TYPE)
	{
		Chat(ClientID, "Experience is now at 100%.");
	}
}

int CGS::GetExperienceMultiplier(int Experience) const
{
	return IsDungeon() ? translate_to_percent_rest(Experience, g_Config.m_SvMultiplierExpRaidDungeon) : translate_to_percent_rest(Experience, m_MultiplierExp);
}

void CGS::InitZones()
{
	m_DungeonID = 0;
	m_AllowedPVP = false;
	m_DayEnumType = Server()->GetEnumTypeDay();

	// with mobs allow pvp zone
	for(int i = MAX_PLAYERS; i < MAX_CLIENTS; i++)
	{
		if(m_apPlayers[i] && m_apPlayers[i]->GetBotType() == TYPE_BOT_MOB && m_apPlayers[i]->GetPlayerWorldID() == m_WorldID)
		{
			m_AllowedPVP = true;
			break;
		}
	}

	// init dungeon zone
	for(const auto& [ID, Dungeon] : CDungeonData::ms_aDungeon)
	{
		if(m_WorldID == Dungeon.m_WorldID)
		{
			m_DungeonID = ID;
			m_AllowedPVP = false;
			break;
		}
	}
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
