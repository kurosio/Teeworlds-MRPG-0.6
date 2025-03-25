/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <cstdint>

#include <base/logger.h>

#include <engine/config.h>
#include <engine/console.h>
#include <engine/engine.h>
#include <engine/map.h>
#include <engine/server.h>
#include <engine/storage.h>

#include <engine/shared/compression.h>
#include <engine/shared/econ.h>
#include <engine/shared/json.h>
#include <engine/shared/http.h>
#include <engine/shared/network.h>
#include <engine/shared/packer.h>
#include <engine/shared/protocol.h>
#include <engine/shared/protocol_ex.h>
#include <engine/shared/snapshot.h>
#include <mastersrv/mastersrv.h>
#include <teeother/components/localization.h>
#include "snapshot_ids_pool.h"
#include "register.h"
#include "server.h"

#include "sqlite3/sqlite_handler.h"

#if defined(CONF_FAMILY_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#include "input_events.h"
#include "multi_worlds.h"
#include "server_ban.h"
#include "server_logger.h"
#include "geo_ip.h"

void CServer::CClient::Reset()
{
	// reset input
	for(auto& m_aInput : m_aInputs)
		m_aInput.m_GameTick = -1;
	m_CurrentInput = 0;
	mem_zero(&m_LatestInput, sizeof(m_LatestInput));

	m_Snapshots.PurgeAll();
	m_LastAckedSnapshot = -1;
	m_LastInputTick = -1;
	m_SnapRate = SNAPRATE_INIT;
	m_Score = -1;
	m_NextMapChunk = 0;
}

CServer::CServer()
{
	m_TickSpeed = SERVER_TICK_SPEED;
	m_CurrentGameTick = MIN_TICK;
	m_RunServer = UNINITIALIZED;

	m_ShiftTime = 0;
	m_LastShiftTick = 0;

	m_RconClientID = RCON_CID_SERV;
	m_RconAuthLevel = AUTHED_ADMIN;

	m_RconPasswordSet = 0;
	m_GeneratedRconPassword = 0;
	m_HeavyReload = false;

	m_ServerInfoFirstRequest = 0;
	m_ServerInfoNumRequests = 0;
	m_ServerInfoNeedsUpdate = false;

	m_pServerBan = new CServerBan;
	m_pMultiWorlds = new CMultiWorlds;
	m_pRegister = nullptr;

	Init();
}

CServer::~CServer()
{
	delete m_pRegister;
	delete m_pMultiWorlds;
	m_vBaseAccounts.clear();

	Database->DisconnectConnectionHeap();
}

IGameServer* CServer::GameServer(int WorldID) const
{
	if(!MultiWorlds()->IsValid(WorldID))
		return MultiWorlds()->GetWorld(MAIN_WORLD_ID)->GameServer();
	return MultiWorlds()->GetWorld(WorldID)->GameServer();
}

IGameServer* CServer::GameServerPlayer(int ClientID) const
{
	return GameServer(GetClientWorldID(ClientID));
}

IInputEvents* CServer::Input() const
{
	return m_pInputKeys;
}

// get the world minute
int CServer::GetMinuteGameTime() const
{
	return m_GameMinuteTime;
}

// get the world hour
int CServer::GetHourGameTime() const
{
	return m_GameHourTime;
}

// get the time offset at the beginning of the timer
int CServer::GetOffsetGameTime() const
{
	return m_ShiftTime;
}

// set the time offset at the beginning of the timer
void CServer::SetOffsetGameTime(int Hour)
{
	m_LastShiftTick = Tick();
	m_GameHourTime = clamp(Hour, 0, 23);
	m_GameMinuteTime = 0;

	if(Hour <= 0)
		m_ShiftTime = m_LastShiftTick;
	else
		m_ShiftTime = m_LastShiftTick - ((m_GameHourTime * 60) * TickSpeed());
}

// format Day
const char* CServer::GetStringTypeday() const
{
	switch(GetCurrentTypeday())
	{
		case MORNING_TYPE: return "Morning";
		case DAY_TYPE: return "Day";
		case EVENING_TYPE: return "Evening";
		default: return "Night";
	}
}

// format Day to Int
int CServer::GetCurrentTypeday() const
{
	if(m_GameHourTime >= 0 && m_GameHourTime < 6) return NIGHT_TYPE;
	if(m_GameHourTime >= 6 && m_GameHourTime < 13) return MORNING_TYPE;
	if(m_GameHourTime >= 13 && m_GameHourTime < 19) return DAY_TYPE;
	return EVENING_TYPE;
}

void CServer::SetClientName(int ClientID, const char* pName)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State < CClient::STATE_READY || !pName)
		return;

	const char* pDefaultName = "(1)";
	pName = str_utf8_skip_whitespaces(pName);
	str_copy(m_aClients[ClientID].m_aName, *pName ? pName : pDefaultName, MAX_NAME_LENGTH);

	char aPrefixName[MAX_NAME_LENGTH];
	str_format(aPrefixName, sizeof(aPrefixName), "[C]%s", m_aClients[ClientID].m_aName);
	str_copy(m_aClients[ClientID].m_aNameTransfersPrefix, aPrefixName, MAX_NAME_LENGTH);
}

void CServer::SetClientClan(int ClientID, const char* pClan)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State < CClient::STATE_READY || !pClan)
		return;

	str_copy(m_aClients[ClientID].m_aClan, pClan, sizeof(m_aClients[ClientID].m_aClan));
}

void CServer::SetClientCountry(int ClientID, int Country)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State < CClient::STATE_READY)
		return;

	m_aClients[ClientID].m_Country = Country;
}

void CServer::SetClientScore(int ClientID, int Score)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State < CClient::STATE_READY)
		return;

	m_aClients[ClientID].m_Score = Score;
}

const char* CServer::Localize(int ClientID, const char* pText)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State < CClient::STATE_READY)
		return pText;

	return m_pLocalization->Localize(m_aClients[ClientID].m_aLanguage, pText);
}

void CServer::SetClientLanguage(int ClientID, const char* pLanguage)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State < CClient::STATE_READY)
		return;

	str_copy(m_aClients[ClientID].m_aLanguage, pLanguage, sizeof(m_aClients[ClientID].m_aLanguage));
}

bool CServer::IsClientChangingWorld(int ClientID)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS)
		return false;

	return m_aClients[ClientID].m_ChangeWorld && m_aClients[ClientID].m_State >= CClient::STATE_CONNECTING && m_aClients[ClientID].m_State < CClient::STATE_INGAME;
}

const char* CServer::GetWorldName(int WorldID)
{
	if(!MultiWorlds()->IsValid(WorldID))
		return "invalid";
	return MultiWorlds()->GetWorld(WorldID)->GetName();
}

CWorldDetail* CServer::GetWorldDetail(int WorldID)
{
	if(!MultiWorlds()->IsValid(WorldID))
		return nullptr;
	return MultiWorlds()->GetWorld(WorldID)->GetDetail();
}

bool CServer::IsWorldType(int WorldID, WorldType Type) const
{
	if(!MultiWorlds()->IsValid(WorldID))
		return false;
	return MultiWorlds()->GetWorld(WorldID)->GetDetail()->GetType() == Type;
}

int CServer::GetWorldsSize() const
{
	return MultiWorlds()->GetSizeInitilized();
}

const char* CServer::GetClientLanguage(int ClientID) const
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State < CClient::STATE_READY)
		return "en";
	return m_aClients[ClientID].m_aLanguage;
}

void CServer::ChangeWorld(int ClientID, int NewWorldID)
{
	if(ClientID < 0 || ClientID >= MAX_PLAYERS || NewWorldID == m_aClients[ClientID].m_WorldID || !MultiWorlds()->IsValid(NewWorldID) || m_aClients[ClientID].m_State < CClient::STATE_READY)
		return;

	m_aClients[ClientID].m_OldWorldID = m_aClients[ClientID].m_WorldID;
	GameServer(m_aClients[ClientID].m_OldWorldID)->OnClientPrepareChangeWorld(ClientID);

	m_aClients[ClientID].m_WorldID = NewWorldID;
	GameServer(m_aClients[ClientID].m_WorldID)->OnClientPrepareChangeWorld(ClientID);

	int* pIdMap = GetIdMap(ClientID);
	memset(pIdMap, -1, sizeof(int) * VANILLA_MAX_CLIENTS);
	pIdMap[0] = ClientID;

	m_aClients[ClientID].Reset();
	m_aClients[ClientID].m_ChangeWorld = true;
	m_aClients[ClientID].m_State = CClient::STATE_CONNECTING;
	SendMap(ClientID);
}

int CServer::GetClientWorldID(int ClientID) const
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State < CClient::STATE_READY)
		return MAIN_WORLD_ID;

	return m_aClients[ClientID].m_WorldID;
}

const char* CServer::ClientContinent(int ClientID) const
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State < CClient::STATE_READY || m_aClients[ClientID].m_aContinent[0] == '\0')
		return "Unknown";

	return m_aClients[ClientID].m_aContinent;
}

const char* CServer::ClientCountryIsoCode(int ClientID) const
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State < CClient::STATE_READY || m_aClients[ClientID].m_aCountryIsoCode[0] == '\0')
		return "UN";

	return m_aClients[ClientID].m_aCountryIsoCode;
}

void CServer::Kick(int ClientID, const char* pReason)
{
	if(ClientID < 0 || ClientID >= MAX_PLAYERS || m_aClients[ClientID].m_State == CClient::STATE_EMPTY)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "invalid client id to kick");
		return;
	}
	if(m_RconClientID == ClientID)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "you can't kick yourself");
		return;
	}
	if(m_aClients[ClientID].m_Authed > m_RconAuthLevel)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "kick command denied");
		return;
	}

	m_NetServer.Drop(ClientID, pReason);
}

/*int CServer::Tick()
{
	return m_CurrentGameTick;
}*/

int64_t CServer::TickStartTime(int Tick) const
{
	return m_GameStartTime + (time_freq() * Tick) / SERVER_TICK_SPEED;
}

/*int CServer::TickSpeed()
{
	return SERVER_TICK_SPEED;
}*/

int CServer::Init()
{
	m_GameTypeday = -1;
	m_GameMinuteTime = 0;
	m_GameHourTime = 0;
	m_CurrentGameTick = 0;

	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		str_copy(m_aClients[i].m_aLanguage, "en", sizeof(m_aClients[i].m_aLanguage));
		m_aClients[i].m_State = CClient::STATE_EMPTY;
		m_aClients[i].m_aName[0] = 0;
		m_aClients[i].m_aClan[0] = 0;
		m_aClients[i].m_Country = -1;
		m_aClients[i].m_Snapshots.Init();
	}

	// Initialize the static data for multiworld identification in the store
	detail::_MultiworldIdentifiableData::Init(this);
	return 0;
}

void CServer::SendLogLine(const CLogMessage* pMessage)
{
	if(pMessage->m_Level <= IConsole::ToLogLevelFilter(g_Config.m_ConsoleOutputLevel))
	{
		SendRconLogLine(-1, pMessage);
	}
	if(pMessage->m_Level <= IConsole::ToLogLevelFilter(g_Config.m_EcOutputLevel))
	{
		m_Econ.Send(-1, pMessage->m_aLine);
	}
}

void CServer::SetRconCID(int ClientID)
{
	m_RconClientID = ClientID;
}

int CServer::GetRconCID() const
{
	return m_RconClientID;
}

int CServer::GetRconAuthLevel() const
{
	return m_RconAuthLevel;
}

int CServer::GetAuthedState(int ClientID) const
{
	return m_aClients[ClientID].m_Authed;
}

bool CServer::IsAuthed(int ClientID) const
{
	return m_aClients[ClientID].m_Authed;
}

bool CServer::IsBanned(int ClientID)
{
	return m_pServerBan->IsBanned(m_NetServer.ClientAddr(ClientID), 0, 0);
}

bool CServer::IsEmpty(int ClientID) const
{
	return m_aClients[ClientID].m_State == CClient::STATE_EMPTY;
}

int CServer::GetClientInfo(int ClientID, CClientInfo* pInfo) const
{
	dbg_assert(ClientID >= 0 && ClientID < MAX_CLIENTS, "client_id is not valid");
	dbg_assert(pInfo != 0, "info can not be null");

	if(m_aClients[ClientID].m_State == CClient::STATE_INGAME)
	{
		pInfo->m_pName = m_aClients[ClientID].m_aName;
		pInfo->m_Latency = m_aClients[ClientID].m_Latency;

		pInfo->m_GotDDNetVersion = m_aClients[ClientID].m_DDNetVersionSettled;
		pInfo->m_DDNetVersion = m_aClients[ClientID].m_DDNetVersion >= 0 ? m_aClients[ClientID].m_DDNetVersion : VERSION_VANILLA;
		if(m_aClients[ClientID].m_GotDDNetVersionPacket)
		{
			pInfo->m_pConnectionID = &m_aClients[ClientID].m_ConnectionID;
			pInfo->m_pDDNetVersionStr = m_aClients[ClientID].m_aDDNetVersionStr;
		}
		else
		{
			pInfo->m_pConnectionID = nullptr;
			pInfo->m_pDDNetVersionStr = nullptr;
		}
		return 1;
	}
	return 0;
}

void CServer::SetClientDDNetVersion(int ClientID, int DDNetVersion)
{
	dbg_assert(ClientID >= 0 && ClientID < MAX_CLIENTS, "ClientID is not valid");

	if(m_aClients[ClientID].m_State == CClient::STATE_INGAME)
	{
		m_aClients[ClientID].m_DDNetVersion = DDNetVersion;
		m_aClients[ClientID].m_DDNetVersionSettled = true;
	}
}

int CServer::GetClientVersion(int ClientID) const
{
	// Assume latest client version for server demos
	if(ClientID == SERVER_DEMO_CLIENT)
		return DDNET_VERSION_NUMBER;

	CClientInfo Info {};
	if(GetClientInfo(ClientID, &Info))
		return Info.m_DDNetVersion;
	return VERSION_NONE;
}

void CServer::GetClientAddr(int ClientID, char* pAddrStr, int Size) const
{
	if(ClientID >= 0 && ClientID < MAX_CLIENTS && m_aClients[ClientID].m_State == CClient::STATE_INGAME)
		net_addr_str(m_NetServer.ClientAddr(ClientID), pAddrStr, Size, false);
}

void CServer::SetStateClientMRPG(int ClientID, bool State)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State < CClient::STATE_READY)
		return;

	m_aClients[ClientID].m_IsClientMRPG = State;
}

bool CServer::GetStateClientMRPG(int ClientID) const
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State < CClient::STATE_READY)
		return 0;
	return m_aClients[ClientID].m_IsClientMRPG;
}


const char* CServer::ClientName(int ClientID) const
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State == CClient::STATE_EMPTY)
		return "(invalid)";

	if(m_aClients[ClientID].m_ChangeWorld && m_aClients[ClientID].m_State >= CClient::STATE_CONNECTING && m_aClients[ClientID].m_State < CClient::STATE_INGAME)
		return m_aClients[ClientID].m_aNameTransfersPrefix;

	if(m_aClients[ClientID].m_State == CClient::STATE_INGAME)
		return m_aClients[ClientID].m_aName;

	return "(connecting)";
}

const char* CServer::ClientClan(int ClientID) const
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State == CClient::STATE_EMPTY)
		return "";
	if(m_aClients[ClientID].m_State == CClient::STATE_INGAME)
		return m_aClients[ClientID].m_aClan;
	return "";
}

int CServer::ClientCountry(int ClientID) const
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State == CClient::STATE_EMPTY)
		return -1;
	if(m_aClients[ClientID].m_State == CClient::STATE_INGAME)
		return m_aClients[ClientID].m_Country;
	return -1;
}

bool CServer::ClientIngame(int ClientID) const
{
	return ClientID >= 0 && ClientID < MAX_CLIENTS && m_aClients[ClientID].m_State == CClient::STATE_INGAME;
}

int CServer::GetClientLatency(int ClientID) const
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State != CClient::STATE_INGAME)
		return 0;

	return m_aClients[ClientID].m_Latency;
}

void CServer::InitRconPasswordIfUnset()
{
	if(m_RconPasswordSet)
	{
		return;
	}

	static constexpr char VALUES[] = "ABCDEFGHKLMNPRSTUVWXYZabcdefghjkmnopqt23456789";
	static constexpr size_t NUM_VALUES = sizeof(VALUES) - 1; // Disregard the '\0'.
	static constexpr size_t PASSWORD_LENGTH = 6;
	dbg_assert(NUM_VALUES * NUM_VALUES >= 2048, "need at least 2048 possibilities for 2-character sequences");
	// With 6 characters, we get a password entropy of log(2048) * 6/2 = 33bit.

	dbg_assert(PASSWORD_LENGTH % 2 == 0, "need an even password length");
	unsigned short aRandom[PASSWORD_LENGTH / 2];
	char aRandomPassword[PASSWORD_LENGTH + 1];
	aRandomPassword[PASSWORD_LENGTH] = 0;

	secure_random_fill(aRandom, sizeof(aRandom));
	for(size_t i = 0; i < PASSWORD_LENGTH / 2; i++)
	{
		const unsigned short RandomNumber = aRandom[i] % 2048;
		aRandomPassword[2 * i + 0] = VALUES[RandomNumber / NUM_VALUES];
		aRandomPassword[2 * i + 1] = VALUES[RandomNumber % NUM_VALUES];
	}

	str_copy(g_Config.m_SvRconPassword, aRandomPassword, sizeof(g_Config.m_SvRconPassword));
	m_GeneratedRconPassword = 1;
}


static inline bool RepackMsg(const CMsgPacker* pMsg, CPacker& Packer)
{
	const int MsgId = pMsg->m_MsgID;
	Packer.Reset();

	if(MsgId < OFFSET_UUID)
	{
		Packer.AddInt((MsgId << 1) | (pMsg->m_System ? 1 : 0));
	}
	else
	{
		Packer.AddInt((0 << 1) | (pMsg->m_System ? 1 : 0)); // NETMSG_EX, NETMSGTYPE_EX
		g_UuidManager.PackUuid(MsgId, &Packer);
	}
	Packer.AddRaw(pMsg->Data(), pMsg->Size());

	return false;
}

int CServer::SendMsg(CMsgPacker* pMsg, int Flags, int ClientID, int64_t Mask, int WorldID)
{
	if(!pMsg)
		return -1;

	if(ClientID != -1 && (ClientID < 0 || ClientID >= MAX_PLAYERS || m_aClients[ClientID].m_State == CClient::STATE_EMPTY || m_aClients[ClientID].m_Quitting))
		return 0;

	CNetChunk Packet {};
	Packet.m_ClientID = ClientID;
	Packet.m_pData = pMsg->Data();
	Packet.m_DataSize = pMsg->Size();

	if(Flags & MSGFLAG_VITAL)
		Packet.m_Flags |= NETSENDFLAG_VITAL;
	if(Flags & MSGFLAG_FLUSH)
		Packet.m_Flags |= NETSENDFLAG_FLUSH;

	if(!(Flags & MSGFLAG_NOSEND))
	{
		if(ClientID == -1)
		{
			CPacker Pack {};
			if(RepackMsg(pMsg, Pack))
				return -1;

			for(int i = 0; i < MAX_PLAYERS; i++)
			{
				if(m_aClients[i].m_State == CClient::STATE_INGAME && !m_aClients[i].m_Quitting)
				{
					// skip what is not included in the mask
					if(Mask != -1 && (Mask & (int64_t)1 << i) == 0)
						continue;

					const CPacker* pPack = &Pack;
					Packet.m_pData = pPack->Data();
					Packet.m_DataSize = pPack->Size();
					if(WorldID != -1)
					{
						if(m_aClients[i].m_WorldID == WorldID)
						{
							Packet.m_ClientID = i;
							m_NetServer.Send(&Packet);
						}
						continue;
					}

					Packet.m_ClientID = i;
					m_NetServer.Send(&Packet);
				}
			}
		}
		else
		{
			CPacker Pack;
			if(RepackMsg(pMsg, Pack))
				return -1;

			Packet.m_ClientID = ClientID;
			Packet.m_pData = Pack.Data();
			Packet.m_DataSize = Pack.Size();

			if(!(Flags & MSGFLAG_NOSEND))
				m_NetServer.Send(&Packet);
		}
	}
	return 0;
}

int CServer::SendMotd(int ClientID, const char* pText)
{
	CNetMsg_Sv_Motd Msg;
	Msg.m_pMessage = pText;
	return SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CServer::DoSnapshot(int WorldID)
{
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		// client must be ingame to recive snapshots
		if(m_aClients[i].m_WorldID != WorldID || m_aClients[i].m_State != CClient::STATE_INGAME)
			continue;

		// this client is trying to recover, don't spam snapshots
		if(m_aClients[i].m_SnapRate == CClient::SNAPRATE_RECOVER && (Tick() % 50) != 0)
			continue;

		// this client is trying to recover, don't spam snapshots
		if(m_aClients[i].m_SnapRate == CClient::SNAPRATE_INIT && (Tick() % 10) != 0)
			continue;

		{
			m_SnapshotBuilder.Init();

			GameServer(WorldID)->OnSnap(i);

			// finish snapshot
			char aData[CSnapshot::MAX_SIZE];
			CSnapshot* pData = (CSnapshot*)aData; // Fix compiler warning for strict-aliasing
			int SnapshotSize = m_SnapshotBuilder.Finish(pData);
			const unsigned Crc = pData->Crc();

			// remove old snapshots
			// keep 3 seconds worth of snapshots
			m_aClients[i].m_Snapshots.PurgeUntil(m_CurrentGameTick - SERVER_TICK_SPEED * 3);

			// save the snapshot
			m_aClients[i].m_Snapshots.Add(m_CurrentGameTick, time_get(), SnapshotSize, pData, 0, nullptr);

			// find snapshot that we can perform delta against
			int DeltaTick = -1;
			const CSnapshot* pDeltashot = CSnapshot::EmptySnapshot();
			{
				int DeltashotSize = m_aClients[i].m_Snapshots.Get(m_aClients[i].m_LastAckedSnapshot, 0, &pDeltashot, 0);
				if(DeltashotSize >= 0)
					DeltaTick = m_aClients[i].m_LastAckedSnapshot;
				else
				{
					// no acked package found, force client to recover rate
					if(m_aClients[i].m_SnapRate == CClient::SNAPRATE_FULL)
						m_aClients[i].m_SnapRate = CClient::SNAPRATE_RECOVER;
				}
			}

			// create delta
			char aDeltaData[CSnapshot::MAX_SIZE];
			if(int DeltaSize = m_SnapshotDelta.CreateDelta(pDeltashot, pData, aDeltaData))
			{
				// compress it
				constexpr int MaxSize = MAX_SNAPSHOT_PACKSIZE;

				char aCompData[CSnapshot::MAX_SIZE];
				SnapshotSize = CVariableInt::Compress(aDeltaData, DeltaSize, aCompData, sizeof(aCompData));
				const int NumPackets = (SnapshotSize + MaxSize - 1) / MaxSize;

				for(int n = 0, Left = SnapshotSize; Left > 0; n++)
				{
					int Chunk = Left < MaxSize ? Left : MaxSize;
					Left -= Chunk;

					if(NumPackets == 1)
					{
						CMsgPacker Msg(NETMSG_SNAPSINGLE, true);
						Msg.AddInt(m_CurrentGameTick);
						Msg.AddInt(m_CurrentGameTick - DeltaTick);
						Msg.AddInt(Crc);
						Msg.AddInt(Chunk);
						Msg.AddRaw(&aCompData[n * MaxSize], Chunk);
						SendMsg(&Msg, MSGFLAG_FLUSH, i, -1, WorldID);
					}
					else
					{
						CMsgPacker Msg(NETMSG_SNAP, true);
						Msg.AddInt(m_CurrentGameTick);
						Msg.AddInt(m_CurrentGameTick - DeltaTick);
						Msg.AddInt(NumPackets);
						Msg.AddInt(n);
						Msg.AddInt(Crc);
						Msg.AddInt(Chunk);
						Msg.AddRaw(&aCompData[n * MaxSize], Chunk);
						SendMsg(&Msg, MSGFLAG_FLUSH, i, -1, WorldID);
					}
				}
			}
			else
			{
				CMsgPacker Msg(NETMSG_SNAPEMPTY, true);
				Msg.AddInt(m_CurrentGameTick);
				Msg.AddInt(m_CurrentGameTick - DeltaTick);
				SendMsg(&Msg, MSGFLAG_FLUSH, i, -1, WorldID);
			}
		}
	}
	GameServer(WorldID)->OnPostSnap();
}


int CServer::ClientRejoinCallback(int ClientID, void* pUser)
{
	CServer* pThis = (CServer*)pUser;

	pThis->m_aClients[ClientID].m_Authed = AUTHED_NO;
	pThis->m_aClients[ClientID].m_pRconCmdToSend = 0;
	pThis->m_aClients[ClientID].m_DDNetVersion = VERSION_NONE;
	pThis->m_aClients[ClientID].m_GotDDNetVersionPacket = false;
	pThis->m_aClients[ClientID].m_DDNetVersionSettled = false;

	pThis->m_aClients[ClientID].Reset();
	pThis->SendMap(ClientID);
	return 0;
}

int CServer::NewClientNoAuthCallback(int ClientID, void* pUser)
{
	CServer* pThis = (CServer*)pUser;

	pThis->GameServer(MAIN_WORLD_ID)->OnClearClientData(ClientID);
	pThis->m_aClients[ClientID].m_State = CClient::STATE_CONNECTING;
	pThis->m_aClients[ClientID].m_aName[0] = 0;
	pThis->m_aClients[ClientID].m_aClan[0] = 0;
	pThis->m_aClients[ClientID].m_Country = -1;
	pThis->m_aClients[ClientID].m_Score = 0;
	pThis->m_aClients[ClientID].m_Authed = AUTHED_NO;
	pThis->m_aClients[ClientID].m_AuthTries = 0;
	pThis->m_aClients[ClientID].m_pRconCmdToSend = 0;
	pThis->m_aClients[ClientID].m_DDNetVersion = VERSION_NONE;
	pThis->m_aClients[ClientID].m_GotDDNetVersionPacket = false;
	pThis->m_aClients[ClientID].m_DDNetVersionSettled = false;
	pThis->m_aClients[ClientID].Reset();

	pThis->SendCapabilities(ClientID);
	pThis->SendMap(ClientID);
	return 0;
}

int CServer::NewClientCallback(int ClientID, void* pUser)
{
	CServer* pThis = (CServer*)pUser;
	int* pIdMap = pThis->GetIdMap(ClientID);
	memset(pIdMap, -1, sizeof(int) * VANILLA_MAX_CLIENTS);
	pIdMap[0] = ClientID;

	pThis->GameServer(MAIN_WORLD_ID)->OnClearClientData(ClientID);
	str_copy(pThis->m_aClients[ClientID].m_aLanguage, "en", sizeof(pThis->m_aClients[ClientID].m_aLanguage));
	pThis->m_aClients[ClientID].m_State = CClient::STATE_AUTH;
	pThis->m_aClients[ClientID].m_aName[0] = 0;
	pThis->m_aClients[ClientID].m_aClan[0] = 0;
	pThis->m_aClients[ClientID].m_Country = -1;
	pThis->m_aClients[ClientID].m_Score = 0;
	pThis->m_aClients[ClientID].m_Authed = AUTHED_NO;
	pThis->m_aClients[ClientID].m_AuthTries = 0;
	pThis->m_aClients[ClientID].m_pRconCmdToSend = 0;
	pThis->m_aClients[ClientID].m_OldWorldID = g_Config.m_SvShowWorldWhenConnect;
	pThis->m_aClients[ClientID].m_WorldID = g_Config.m_SvShowWorldWhenConnect;
	pThis->m_aClients[ClientID].m_ChangeWorld = false;
	pThis->m_aClients[ClientID].m_ClientVersion = 0;
	pThis->m_aClients[ClientID].m_IsClientMRPG = false;
	pThis->m_aClients[ClientID].m_Quitting = false;

	pThis->m_aClients[ClientID].m_DDNetVersion = VERSION_NONE;
	pThis->m_aClients[ClientID].m_GotDDNetVersionPacket = false;
	pThis->m_aClients[ClientID].m_DDNetVersionSettled = false;
	mem_zero(&pThis->m_aClients[ClientID].m_Addr, sizeof(NETADDR));
	pThis->m_aClients[ClientID].Reset();
	return 0;
}

int CServer::DelClientCallback(int ClientID, const char* pReason, void* pUser)
{
	// THREAD_PLAYER_DATA_SAFE(ClientID)
	CServer* pThis = (CServer*)pUser;

	char aAddrStr[NETADDR_MAXSTRSIZE];
	net_addr_str(pThis->m_NetServer.ClientAddr(ClientID), aAddrStr, sizeof(aAddrStr), true);
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "client dropped. cid=%d addr=%s reason='%s'", ClientID, aAddrStr, pReason);
	pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

	// notify the mod about the drop
	if(pThis->m_aClients[ClientID].m_State >= CClient::STATE_READY || pThis->IsClientChangingWorld(ClientID))
	{
		pThis->m_aClients[ClientID].m_Quitting = true;

		for(int i = 0; i < pThis->MultiWorlds()->GetSizeInitilized(); i++)
		{
			IGameServer* pGameServer = pThis->MultiWorlds()->GetWorld(i)->GameServer();
			pGameServer->OnClientDrop(ClientID, pReason);
		}

		pThis->GameServer(MAIN_WORLD_ID)->OnClearClientData(ClientID);
		pThis->ExpireServerInfo();
	}

	pThis->m_aClients[ClientID].m_State = CClient::STATE_EMPTY;
	pThis->m_aClients[ClientID].m_aName[0] = 0;
	pThis->m_aClients[ClientID].m_aClan[0] = 0;
	pThis->m_aClients[ClientID].m_Country = -1;
	pThis->m_aClients[ClientID].m_Authed = AUTHED_NO;
	pThis->m_aClients[ClientID].m_AuthTries = 0;
	pThis->m_aClients[ClientID].m_pRconCmdToSend = nullptr;
	pThis->m_aClients[ClientID].m_OldWorldID = g_Config.m_SvShowWorldWhenConnect;
	pThis->m_aClients[ClientID].m_WorldID = g_Config.m_SvShowWorldWhenConnect;
	pThis->m_aClients[ClientID].m_ChangeWorld = false;
	pThis->m_aClients[ClientID].m_ClientVersion = 0;
	pThis->m_aClients[ClientID].m_IsClientMRPG = false;
	pThis->m_aClients[ClientID].m_Quitting = false;
	pThis->m_aClients[ClientID].m_Snapshots.PurgeAll();
	return 0;
}

void CServer::SendMapData(int ClientID, int Chunk)
{
	const int WorldID = m_aClients[ClientID].m_WorldID;
	unsigned int CurrentMapSize = MultiWorlds()->GetWorld(WorldID)->MapDetail()->GetSize();
	unsigned char* pCurrentMapData = MultiWorlds()->GetWorld(WorldID)->MapDetail()->GetData();
	const unsigned Crc = MultiWorlds()->GetWorld(WorldID)->MapDetail()->GetCrc();

	unsigned int ChunkSize = 1024 - 128;
	const unsigned int Offset = Chunk * ChunkSize;
	int Last = 0;

	// drop faulty map data requests
	if(Chunk < 0 || Offset > CurrentMapSize)
		return;

	if((Offset + ChunkSize) >= CurrentMapSize)
	{
		ChunkSize = CurrentMapSize - Offset;
		Last = 1;
	}

	CMsgPacker Msg(NETMSG_MAP_DATA, true);
	Msg.AddInt(Last);
	Msg.AddInt(Crc);
	Msg.AddInt(Chunk);
	Msg.AddInt(ChunkSize);
	Msg.AddRaw(&pCurrentMapData[Offset], ChunkSize);
	SendMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_FLUSH, ClientID);

	if(g_Config.m_Debug)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "sending chunk %d with size %d", Chunk, ChunkSize);
		Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "server", aBuf);
	}
}

void CServer::SendCapabilities(int ClientID)
{
	CMsgPacker Msg(NETMSG_CAPABILITIES, true);
	Msg.AddInt(SERVERCAP_CURVERSION); // version
	Msg.AddInt(SERVERCAPFLAG_ANYPLAYERFLAG | SERVERCAPFLAG_SYNCWEAPONINPUT); // flags
	SendMsg(&Msg, MSGFLAG_VITAL, ClientID, -1, m_aClients[ClientID].m_WorldID);
}

void CServer::SendMap(int ClientID)
{
	const int WorldID = m_aClients[ClientID].m_WorldID;
	const char* pWorldName = MultiWorlds()->GetWorld(WorldID)->GetName();
	CMapDetail* pMapDetail = MultiWorlds()->GetWorld(WorldID)->MapDetail();

	{
		SHA256_DIGEST Sha256 = pMapDetail->GetSha256();
		CMsgPacker Msg(NETMSG_MAP_DETAILS, true);
		Msg.AddString(pWorldName, 0);
		Msg.AddRaw(&Sha256.data, sizeof(Sha256.data));
		Msg.AddInt(pMapDetail->GetCrc());
		Msg.AddInt(pMapDetail->GetSize());
		Msg.AddString("", 0); // HTTPS map download URL
		SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
	}

	{
		CMsgPacker Msg(NETMSG_MAP_CHANGE, true);
		Msg.AddString(pWorldName, 0);
		Msg.AddInt(pMapDetail->GetCrc());
		Msg.AddInt(pMapDetail->GetSize());
		SendMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_FLUSH, ClientID);
	}

	m_aClients[ClientID].m_NextMapChunk = 0;
}

void CServer::SendConnectionReady(int ClientID)
{
	CMsgPacker Msg(NETMSG_CON_READY, true);
	SendMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_FLUSH, ClientID);
}

void CServer::SendRconLine(int ClientID, const char* pLine)
{
	CMsgPacker Msg(NETMSG_RCON_LINE, true);
	Msg.AddString(pLine, 512);
	SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CServer::SendRconLogLine(int ClientID, const CLogMessage* pMessage)
{
	const char* pLine = pMessage->m_aLine;
	const char* pStart = str_find(pLine, "<{");
	const char* pEnd = pStart == nullptr ? nullptr : str_find(pStart + 2, "}>");

	char aLine[512] {};
	if(pStart != nullptr && pEnd != nullptr)
	{
		str_append(aLine, pLine, pStart - pLine + 1);
		str_append(aLine, pStart + 2, pStart - pLine + pEnd - pStart - 1);
		str_append(aLine, pEnd + 2);

		pLine = aLine;
	}

	if(ClientID == -1)
	{
		for(int i = 0; i < MAX_PLAYERS; i++)
		{
			if(m_aClients[i].m_State != CClient::STATE_EMPTY && m_aClients[i].m_Authed >= AUTHED_ADMIN)
			{
				SendRconLine(i, pLine);
			}
		}
	}
	else
	{
		if(m_aClients[ClientID].m_State != CClient::STATE_EMPTY)
		{
			SendRconLine(ClientID, pLine);
		}
	}
}

void CServer::SendRconCmdAdd(const IConsole::CCommandInfo* pCommandInfo, int ClientID)
{
	if(ClientID >= MAX_PLAYERS)
		return;

	CMsgPacker Msg(NETMSG_RCON_CMD_ADD, true);
	Msg.AddString(pCommandInfo->m_pName, IConsole::TEMPCMD_NAME_LENGTH);
	Msg.AddString(pCommandInfo->m_pHelp, IConsole::TEMPCMD_HELP_LENGTH);
	Msg.AddString(pCommandInfo->m_pParams, IConsole::TEMPCMD_PARAMS_LENGTH);
	SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CServer::SendRconCmdRem(const IConsole::CCommandInfo* pCommandInfo, int ClientID)
{
	if(ClientID >= MAX_PLAYERS)
		return;

	CMsgPacker Msg(NETMSG_RCON_CMD_REM, true);
	Msg.AddString(pCommandInfo->m_pName, 256);
	SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CServer::UpdateClientRconCommands()
{
	for(int ClientID = Tick() % MAX_RCONCMD_RATIO; ClientID < MAX_PLAYERS; ClientID += MAX_RCONCMD_RATIO)
	{
		if(m_aClients[ClientID].m_State != CClient::STATE_EMPTY && m_aClients[ClientID].m_Authed)
		{
			int ConsoleAccessLevel = m_aClients[ClientID].m_Authed == AUTHED_ADMIN ? IConsole::ACCESS_LEVEL_ADMIN : IConsole::ACCESS_LEVEL_MOD;
			for(int i = 0; i < MAX_RCONCMD_SEND && m_aClients[ClientID].m_pRconCmdToSend; ++i)
			{
				SendRconCmdAdd(m_aClients[ClientID].m_pRconCmdToSend, ClientID);
				m_aClients[ClientID].m_pRconCmdToSend = m_aClients[ClientID].m_pRconCmdToSend->NextCommandInfo(ConsoleAccessLevel, CFGFLAG_SERVER);
			}
		}
	}
}

void CServer::ProcessClientPacket(CNetChunk* pPacket)
{
	int ClientID = pPacket->m_ClientID;
	CUnpacker Unpacker;
	Unpacker.Reset(pPacket->m_pData, pPacket->m_DataSize);
	CMsgPacker Packer(NETMSG_EX, true);

	// unpack msgid and system flag
	int MsgID;
	bool Sys;
	CUuid Uuid {};

	const int Result = UnpackMessageID(&MsgID, &Sys, &Uuid, &Unpacker, &Packer);
	if(Result == UNPACKMESSAGE_ERROR)
	{
		return;
	}

	if(Result == UNPACKMESSAGE_ANSWER)
	{
		SendMsg(&Packer, MSGFLAG_VITAL, ClientID);
	}

	if(Sys)
	{
		// system message
		if(MsgID == NETMSG_CLIENTVER)
		{
			if((pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && m_aClients[ClientID].m_State == CClient::STATE_PREAUTH)
			{
				CUuid* pConnectionID = (CUuid*)Unpacker.GetRaw(sizeof(*pConnectionID));
				int DDNetVersion = Unpacker.GetInt();
				const char* pDDNetVersionStr = Unpacker.GetString(CUnpacker::SANITIZE_CC);
				if(Unpacker.Error() || !str_utf8_check(pDDNetVersionStr) || DDNetVersion < 0)
				{
					return;
				}
				m_aClients[ClientID].m_ConnectionID = *pConnectionID;
				m_aClients[ClientID].m_DDNetVersion = DDNetVersion;
				str_copy(m_aClients[ClientID].m_aDDNetVersionStr, pDDNetVersionStr, sizeof(m_aClients[ClientID].m_aDDNetVersionStr));
				m_aClients[ClientID].m_DDNetVersionSettled = true;
				m_aClients[ClientID].m_GotDDNetVersionPacket = true;
				m_aClients[ClientID].m_State = CClient::STATE_AUTH;
			}
		}
		else if(MsgID == NETMSG_INFO)
		{
			if((pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && m_aClients[ClientID].m_State == CClient::STATE_AUTH)
			{
				const char* pVersion = Unpacker.GetString(CUnpacker::SANITIZE_CC);
				if(!str_utf8_check(pVersion)) { return; }
				const char* pPassword = Unpacker.GetString(CUnpacker::SANITIZE_CC);
				if(g_Config.m_Password[0] != 0 && str_comp(g_Config.m_Password, pPassword) != 0)
				{
					// wrong password
					m_NetServer.Drop(ClientID, "Wrong password");
					return;
				}

				m_aClients[ClientID].m_Version = Unpacker.GetInt();
				m_aClients[ClientID].m_State = CClient::STATE_CONNECTING;
				GameServer(MAIN_WORLD_ID)->OnClearClientData(ClientID);
				SendCapabilities(ClientID);
				SendMap(ClientID);
			}
		}
		else if(MsgID == NETMSG_REQUEST_MAP_DATA)
		{
			if((pPacket->m_Flags & NET_CHUNKFLAG_VITAL) == 0 || m_aClients[ClientID].m_State < CClient::STATE_CONNECTING)
				return;

			const int Chunk = Unpacker.GetInt();
			if(Chunk != m_aClients[ClientID].m_NextMapChunk || !g_Config.m_SvFastDownload)
			{
				SendMapData(ClientID, Chunk);
				return;
			}

			if(Chunk == 0)
			{
				for(int i = 0; i < g_Config.m_SvMapWindow; i++)
				{
					SendMapData(ClientID, i);
				}
			}

			SendMapData(ClientID, g_Config.m_SvMapWindow + m_aClients[ClientID].m_NextMapChunk);
			m_aClients[ClientID].m_NextMapChunk++;
		}
		else if(MsgID == NETMSG_READY)
		{
			if((pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && m_aClients[ClientID].m_State == CClient::STATE_CONNECTING)
			{
				if(!m_aClients[ClientID].m_ChangeWorld)
				{
					char aAddrStr[NETADDR_MAXSTRSIZE];
					char aAddrStrWithoutPort[NETADDR_MAXSTRSIZE];
					net_addr_str(m_NetServer.ClientAddr(ClientID), aAddrStr, sizeof(aAddrStr), true);
					net_addr_str(m_NetServer.ClientAddr(ClientID), aAddrStrWithoutPort, sizeof(aAddrStrWithoutPort), false);
					str_copy(m_aClients[ClientID].m_aContinent, CGeoIP::getData("continent", aAddrStrWithoutPort).c_str(), sizeof(m_aClients[ClientID].m_aContinent));
					str_copy(m_aClients[ClientID].m_aCountryIsoCode, CGeoIP::getData("country_iso_code", aAddrStrWithoutPort).c_str(), sizeof(m_aClients[ClientID].m_aCountryIsoCode));

					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "player is ready. ClientID=%d addr=%s continent=%s country_iso_code=%s",
						ClientID, aAddrStr, m_aClients[ClientID].m_aContinent, m_aClients[ClientID].m_aCountryIsoCode);
					Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "server", aBuf);

					const int WorldID = m_aClients[ClientID].m_WorldID;
					for(int i = 0; i < MultiWorlds()->GetSizeInitilized(); i++)
					{
						IGameServer* pGameServer = MultiWorlds()->GetWorld(i)->GameServer();
						pGameServer->OnClientPrepareChangeWorld(ClientID);
					}
					GameServer(WorldID)->OnClientConnected(ClientID);
				}

				m_aClients[ClientID].m_State = CClient::STATE_READY;
				SendConnectionReady(ClientID);
				SendCapabilities(ClientID);

				if(!m_aClients[ClientID].m_ChangeWorld)
				{
					ExpireServerInfo();
				}
			}
		}
		else if(MsgID == NETMSG_ENTERGAME)
		{
			const int WorldID = m_aClients[ClientID].m_WorldID;

			if((pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && m_aClients[ClientID].m_State == CClient::STATE_READY && GameServer(WorldID)->IsClientReady(ClientID))
			{
				if(!m_aClients[ClientID].m_ChangeWorld)
				{
					char aAddrStr[NETADDR_MAXSTRSIZE];
					net_addr_str(m_NetServer.ClientAddr(ClientID), aAddrStr, sizeof(aAddrStr), true);

					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "player has entered the game. ClientID=%d addr=%s", ClientID, aAddrStr);
					Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
				}

				m_aClients[ClientID].m_State = CClient::STATE_INGAME;
				GameServer(WorldID)->OnClientEnter(ClientID);

				ExpireServerInfo();
			}
		}
		else if(MsgID == NETMSG_INPUT)
		{
			int64_t TagTime;

			auto& client = m_aClients[ClientID];
			client.m_LastAckedSnapshot = Unpacker.GetInt();
			int IntendedTick = Unpacker.GetInt();
			int Size = Unpacker.GetInt();

			// check to error
			if(Unpacker.Error() || Size / 4 > MAX_INPUT_SIZE)
				return;

			// update snapshot rate
			if(client.m_LastAckedSnapshot > 0)
				client.m_SnapRate = CClient::SNAPRATE_FULL;

			// update client latency
			if(client.m_Snapshots.Get(client.m_LastAckedSnapshot, &TagTime, nullptr, nullptr) >= 0)
			{
				int64_t timeNow = time_get();
				client.m_Latency = static_cast<int>(((timeNow - TagTime) * 1000) / time_freq());
			}

			// skip old packets and add new
			if(IntendedTick > client.m_LastInputTick)
			{
				int TimeLeft = static_cast<int>(((TickStartTime(IntendedTick) - time_get()) * 1000) / time_freq());

				CMsgPacker Msgp(NETMSG_INPUTTIMING, true);
				Msgp.AddInt(IntendedTick);
				Msgp.AddInt(TimeLeft);
				SendMsg(&Msgp, 0, ClientID, -1, client.m_WorldID);
			}

			client.m_LastInputTick = IntendedTick;

			// input
			auto& currentInput = client.m_aInputs[client.m_CurrentInput];
			if(IntendedTick <= Tick())
				IntendedTick = Tick() + 1;

			currentInput.m_GameTick = IntendedTick;

			int InputSize = Size / 4;
			for(int i = 0; i < InputSize; i++)
				currentInput.m_aData[i] = Unpacker.GetInt();

			mem_copy(client.m_LatestInput.m_aData, currentInput.m_aData, InputSize * sizeof(int));

			client.m_CurrentInput = (client.m_CurrentInput + 1) % 200;

			// update new input codes
			if(client.m_State == CClient::STATE_INGAME)
			{
				const int WorldID = client.m_WorldID;
				GameServer(WorldID)->OnClientDirectInput(ClientID, client.m_LatestInput.m_aData);
				m_pInputKeys->ResetClientBlockKeys(ClientID);
			}
		}
		else if(MsgID == NETMSG_RCON_CMD)
		{
			const char* pCmd = Unpacker.GetString();
			if(!str_utf8_check(pCmd))
			{
				return;
			}

			if(Unpacker.Error() == 0 && !str_comp(pCmd, "crashmeplx"))
			{
				int Version = m_aClients[ClientID].m_DDNetVersion;
				if(GameServer()->PlayerExists(ClientID) && Version < VERSION_DDNET_OLD)
				{
					m_aClients[ClientID].m_DDNetVersion = VERSION_DDNET_OLD;
				}
			}
			else if((pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && Unpacker.Error() == 0 && m_aClients[ClientID].m_Authed)
			{
				m_RconClientID = ClientID;
				m_RconAuthLevel = m_aClients[ClientID].m_Authed;
				Console()->SetAccessLevel(m_aClients[ClientID].m_Authed == AUTHED_ADMIN ? IConsole::ACCESS_LEVEL_ADMIN : IConsole::ACCESS_LEVEL_MOD);
				Console()->ExecuteLineFlag(pCmd, CFGFLAG_SERVER, ClientID);
				Console()->SetAccessLevel(IConsole::ACCESS_LEVEL_ADMIN);
				m_RconClientID = RCON_CID_SERV;
				m_RconAuthLevel = AUTHED_ADMIN;

				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "ClientID=%d rcon='%s'", ClientID, pCmd);
				Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "server", aBuf);
			}
		}
		else if(MsgID == NETMSG_RCON_AUTH)
		{
			const char* pName = Unpacker.GetString(CUnpacker::SANITIZE_CC); // login name, now used
			const char* pPw = Unpacker.GetString(CUnpacker::SANITIZE_CC);
			if(!str_utf8_check(pPw) || !str_utf8_check(pName))
				return;

			if((pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && Unpacker.Error() == 0)
			{
				if(g_Config.m_SvRconModPassword[0] && str_comp(pPw, g_Config.m_SvRconModPassword) == 0)
					m_aClients[ClientID].m_Authed = AUTHED_MOD;
				else if(g_Config.m_SvRconPassword[0] && str_comp(pPw, g_Config.m_SvRconPassword) == 0)
					m_aClients[ClientID].m_Authed = AUTHED_ADMIN;
				else if(g_Config.m_SvRconMaxTries)
				{
					m_aClients[ClientID].m_AuthTries++;
					char aBuf[128];
					str_format(aBuf, sizeof(aBuf), "Wrong password %d/%d.", m_aClients[ClientID].m_AuthTries, g_Config.m_SvRconMaxTries);
					SendRconLine(ClientID, aBuf);
					if(m_aClients[ClientID].m_AuthTries >= g_Config.m_SvRconMaxTries)
					{
						if(!g_Config.m_SvRconBantime)
							m_NetServer.Drop(ClientID, "Too many remote console authentication tries");
						else
							m_pServerBan->BanAddr(m_NetServer.ClientAddr(ClientID), g_Config.m_SvRconBantime * 60, "Too many remote console authentication tries");
					}
				}
				else
				{
					SendRconLine(ClientID, "Wrong password.");
				}

				// authed
				if(m_aClients[ClientID].m_Authed != AUTHED_NO)
				{
					CMsgPacker Msgp(NETMSG_RCON_AUTH_STATUS, true);
					Msgp.AddInt(1); //authed
					Msgp.AddInt(1); //cmdlist
					SendMsg(&Msgp, MSGFLAG_VITAL, ClientID);

					char aBuf[256];
					char aAddrStr[NETADDR_MAXSTRSIZE];
					net_addr_str(m_NetServer.ClientAddr(ClientID), aAddrStr, sizeof(aAddrStr), true);
					if(m_aClients[ClientID].m_Authed == AUTHED_MOD)
					{
						m_aClients[ClientID].m_pRconCmdToSend = Console()->FirstCommandInfo(IConsole::ACCESS_LEVEL_MOD, CFGFLAG_SERVER);
						SendRconLine(ClientID, "Moderator authentication successful. Limited remote console access granted.");
						str_format(aBuf, sizeof(aBuf), "ClientID=%d addr=%s authed (moderator)", ClientID, aAddrStr);
						Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
					}
					else if(m_aClients[ClientID].m_Authed == AUTHED_ADMIN)
					{
						m_aClients[ClientID].m_pRconCmdToSend = Console()->FirstCommandInfo(IConsole::ACCESS_LEVEL_ADMIN, CFGFLAG_SERVER);
						SendRconLine(ClientID, "Admin authentication successful. Full remote console access granted.");
						str_format(aBuf, sizeof(aBuf), "ClientID=%d addr=%s authed (admin)", ClientID, aAddrStr);
						Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
					}
				}
			}
		}
		else if(MsgID == NETMSG_PING)
		{
			CMsgPacker Msg(NETMSG_PING_REPLY, true);
			SendMsg(&Msg, 0, ClientID, -1, m_aClients[ClientID].m_WorldID);
		}
		else if(MsgID == NETMSG_PINGEX)
		{
			CUuid* pID = (CUuid*)Unpacker.GetRaw(sizeof(*pID));
			if(Unpacker.Error())
			{
				return;
			}
			CMsgPacker Msgp(NETMSG_PONGEX, true);
			Msgp.AddRaw(pID, sizeof(*pID));
			SendMsg(&Msgp, MSGFLAG_FLUSH, ClientID, m_aClients[ClientID].m_WorldID);
		}
		else
		{
			if(g_Config.m_Debug)
			{
				char aHex[] = "0123456789ABCDEF";
				char aBuf[512];

				for(int b = 0; b < pPacket->m_DataSize && b < 32; b++)
				{
					aBuf[b * 3] = aHex[((const unsigned char*)pPacket->m_pData)[b] >> 4];
					aBuf[b * 3 + 1] = aHex[((const unsigned char*)pPacket->m_pData)[b] & 0xf];
					aBuf[b * 3 + 2] = ' ';
					aBuf[b * 3 + 3] = 0;
				}

				char aBufMsg[256];
				str_format(aBufMsg, sizeof(aBufMsg), "strange message ClientID=%d msg=%d data_size=%d", ClientID, MsgID, pPacket->m_DataSize);
				Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "server", aBufMsg);
				Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "server", aBuf);
			}
		}
	}
	else
	{
		// game message
		if((pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && m_aClients[ClientID].m_State >= CClient::STATE_READY)
		{
			const int WorldID = m_aClients[ClientID].m_WorldID;
			GameServer(WorldID)->OnMessage(MsgID, &Unpacker, ClientID);
		}
	}
}

void CServer::PumpNetwork(bool PacketWaiting)
{
	CNetChunk Packet;
	SECURITY_TOKEN ResponseToken;

	m_NetServer.Update();

	if(PacketWaiting)
	{
		ResponseToken = NET_SECURITY_TOKEN_UNKNOWN;
		while(m_NetServer.Recv(&Packet, &ResponseToken))
		{
			if(Packet.m_ClientID == -1)
			{
				if(ResponseToken == NET_SECURITY_TOKEN_UNKNOWN && m_pRegister->OnPacket(&Packet))
					continue;

				{
					int ExtraToken = 0;
					int Type = -1;
					if(Packet.m_DataSize >= (int)sizeof(SERVERBROWSE_GETINFO) + 1 &&
						mem_comp(Packet.m_pData, SERVERBROWSE_GETINFO, sizeof(SERVERBROWSE_GETINFO)) == 0)
					{
						if(Packet.m_Flags & NETSENDFLAG_EXTENDED)
						{
							Type = SERVERINFO_EXTENDED;
							ExtraToken = (Packet.m_aExtraData[0] << 8) | Packet.m_aExtraData[1];
						}
						else
							Type = SERVERINFO_VANILLA;
					}
					else if(Packet.m_DataSize >= (int)sizeof(SERVERBROWSE_GETINFO_64_LEGACY) + 1 &&
						mem_comp(Packet.m_pData, SERVERBROWSE_GETINFO_64_LEGACY, sizeof(SERVERBROWSE_GETINFO_64_LEGACY)) == 0)
					{
						Type = SERVERINFO_64_LEGACY;
					}

					if(Type != -1)
					{
						int Token = ((unsigned char*)Packet.m_pData)[sizeof(SERVERBROWSE_GETINFO)];
						Token |= ExtraToken << 8;
						SendServerInfoConnless(&Packet.m_Address, Token, Type);
					}
				}
			}
			else
				ProcessClientPacket(&Packet);
		}
	}

	m_pServerBan->Update();
	m_Econ.Update();
}

static inline int GetCacheIndex(int Type, bool SendClient)
{
	if(Type == SERVERINFO_INGAME)
		Type = SERVERINFO_VANILLA;
	else if(Type == SERVERINFO_EXTENDED_MORE)
		Type = SERVERINFO_EXTENDED;

	return Type * 2 + SendClient;
}


void CServer::CacheServerInfo(CBrowserCache* pCache, int Type, bool SendClients)
{
	pCache->Clear();

	// One chance to improve the protocol!
	CPacker p;
	char aBuf[128];

	// count the players
	int PlayerCount = 0, ClientCount = 0;
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		if(m_aClients[i].m_State != CClient::STATE_EMPTY)
		{
			if(GameServer()->IsClientPlayer(i))
				PlayerCount++;

			ClientCount++;
		}
	}

	p.Reset();

#define ADD_RAW(p, x) (p).AddRaw(x, sizeof(x))
#define ADD_INT(p, x) \
	do \
	{ \
		str_format(aBuf, sizeof(aBuf), "%d", x); \
		(p).AddString(aBuf, 0); \
	} while(0)

	p.AddString(GameServer()->Version(), 32);
	if(Type != SERVERINFO_VANILLA)
	{
		p.AddString(g_Config.m_SvName, 256);
	}
	else
	{
		if(m_NetServer.MaxClients() <= VANILLA_MAX_CLIENTS)
		{
			p.AddString(g_Config.m_SvName, 64);
		}
		else
		{
			str_format(aBuf, sizeof(aBuf), "%s [%d/%d]", g_Config.m_SvName, ClientCount, m_NetServer.MaxClients());
			p.AddString(aBuf, 64);
		}
	}
	p.AddString(g_Config.m_SvMap, 32);

	if(Type == SERVERINFO_EXTENDED)
	{
		ADD_INT(p, MultiWorlds()->GetWorld(MAIN_WORLD_ID)->MapDetail()->GetCrc());
		ADD_INT(p, MultiWorlds()->GetWorld(MAIN_WORLD_ID)->MapDetail()->GetSize());
	}

	// gametype
	p.AddString(g_Config.m_SvGamemodeName, 16);

	// flags
	ADD_INT(p, g_Config.m_Password[0] ? SERVER_FLAG_PASSWORD : 0);

	int MaxClients = m_NetServer.MaxClients();
	// How many clients the used serverinfo protocol supports, has to be tracked
	// separately to make sure we don't subtract the reserved slots from it
	int MaxClientsProtocol = MAX_PLAYERS;
	if(Type == SERVERINFO_VANILLA || Type == SERVERINFO_INGAME)
	{
		if(ClientCount >= VANILLA_MAX_CLIENTS)
		{
			if(ClientCount < MaxClients)
				ClientCount = VANILLA_MAX_CLIENTS - 1;
			else
				ClientCount = VANILLA_MAX_CLIENTS;
		}
		MaxClientsProtocol = VANILLA_MAX_CLIENTS;
		if(PlayerCount > ClientCount)
			PlayerCount = ClientCount;
	}

	ADD_INT(p, PlayerCount); // num players
	ADD_INT(p, MaxClientsProtocol); // max players
	ADD_INT(p, ClientCount); // num clients
	ADD_INT(p, MaxClientsProtocol); // max clients

	if(Type == SERVERINFO_EXTENDED)
		p.AddString("", 0); // extra info, reserved

	const void* pPrefix = p.Data();
	int PrefixSize = p.Size();

	CPacker q;
	int ChunksStored = 0;
	int PlayersStored = 0;

#define SAVE(size) \
	do \
	{ \
		pCache->AddChunk(q.Data(), size); \
		ChunksStored++; \
	} while(0)

#define RESET() \
	do \
	{ \
		q.Reset(); \
		q.AddRaw(pPrefix, PrefixSize); \
	} while(0)

	RESET();

	if(Type == SERVERINFO_64_LEGACY)
		q.AddInt(PlayersStored); // offset

	if(!SendClients)
	{
		SAVE(q.Size());
		return;
	}

	if(Type == SERVERINFO_EXTENDED)
	{
		pPrefix = "";
		PrefixSize = 0;
	}

	int Remaining;
	switch(Type)
	{
		case SERVERINFO_EXTENDED: Remaining = -1; break;
		case SERVERINFO_64_LEGACY: Remaining = 24; break;
		case SERVERINFO_VANILLA: Remaining = VANILLA_MAX_CLIENTS; break;
		case SERVERINFO_INGAME: Remaining = VANILLA_MAX_CLIENTS; break;
		default: dbg_assert(0, "caught earlier, unreachable"); return;
	}

	// Use the following strategy for sending:
	// For vanilla, send the first 16 players.
	// For legacy 64p, send 24 players per packet.
	// For extended, send as much players as possible.

	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		if(m_aClients[i].m_State != CClient::STATE_EMPTY)
		{
			if(Remaining == 0)
			{
				if(Type == SERVERINFO_VANILLA || Type == SERVERINFO_INGAME)
					break;

				// Otherwise we're SERVERINFO_64_LEGACY.
				SAVE(q.Size());
				RESET();
				q.AddInt(PlayersStored); // offset
				Remaining = 24;
			}

			if(Remaining > 0)
			{
				Remaining--;
			}

			int PreviousSize = q.Size();
			q.AddString(ClientName(i), MAX_NAME_LENGTH); // client name
			q.AddString(ClientClan(i), MAX_CLAN_LENGTH); // client clan

			ADD_INT(q, m_aClients[i].m_Country); // client country
			ADD_INT(q, m_aClients[i].m_Score); // client score
			ADD_INT(q, GameServer()->IsClientPlayer(i) ? 1 : 0); // is player?
			if(Type == SERVERINFO_EXTENDED)
				q.AddString("", 0); // extra info, reserved

			if(Type == SERVERINFO_EXTENDED)
			{
				if(q.Size() >= NET_MAX_PAYLOAD - 18) // 8 bytes for type, 10 bytes for the largest token
				{
					// Retry current player.
					i--;
					SAVE(PreviousSize);
					RESET();
					ADD_INT(q, ChunksStored);
					q.AddString("", 0); // extra info, reserved
					continue;
				}
			}
			PlayersStored++;
		}
	}

	SAVE(q.Size());
#undef SAVE
#undef RESET
#undef ADD_RAW
#undef ADD_INT
}

void CServer::ExpireServerInfo()
{
	m_ServerInfoNeedsUpdate = true;
}

void CServer::UpdateRegisterServerInfo()
{
	// count the players
	int PlayerCount = 0, ClientCount = 0;
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		if(m_aClients[i].m_State != CClient::STATE_EMPTY)
		{
			if(GameServer()->IsClientPlayer(i))
				PlayerCount++;

			ClientCount++;
		}
	}

	auto* pMainWorld = MultiWorlds()->GetWorld(MAIN_WORLD_ID);
	auto* pMapDetail = pMainWorld->MapDetail();

	char aMapSha256[SHA256_MAXSTRSIZE];
	sha256_str(MultiWorlds()->GetWorld(MAIN_WORLD_ID)->MapDetail()->GetSha256(), aMapSha256, sizeof(aMapSha256));

	nlohmann::json JsServerInfo =
	{
		{"max_clients", (int)MAX_PLAYERS},
		{"max_players", (int)MAX_PLAYERS},
		{"passworded", g_Config.m_Password[0] != '\0' ? true : false},
		{"game_type", g_Config.m_SvGamemodeName},
		{"name", g_Config.m_SvName},
		{"map",
			{
				{"name", "Multiworld"},
				{"sha256", aMapSha256},
				{"size", pMapDetail->GetSize()}
			}
		},
		{"version", GameServer()->Version()},
		{"client_score_kind", "points"},
		{"requires_login", false},
		{"clients", nlohmann::json::array()}
	};

	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		if(m_aClients[i].m_State != CClient::STATE_EMPTY)
		{
			nlohmann::json JsPlayerInfo =
			{
				{"name", ClientName(i)},
				{"clan", ClientClan(i)},
				{"country", m_aClients[i].m_Country},
				{"score", m_aClients[i].m_Score},
				{"is_player", GameServer()->IsClientPlayer(i)}
			};
			GameServer()->OnUpdateClientServerInfo(&JsPlayerInfo, i);

			JsServerInfo["clients"].push_back(JsPlayerInfo);
		}
	}

	m_pRegister->OnNewInfo(JsServerInfo.dump().c_str());
}

void CServer::UpdateServerInfo(bool Resend)
{
	if(m_RunServer == UNINITIALIZED)
		return;

	UpdateRegisterServerInfo();

	for(int i = 0; i < 3; i++)
	{
		for(int j = 0; j < 2; j++)
			CacheServerInfo(&m_aServerInfoCache[i * 2 + j], i, j);
	}

	if(Resend)
	{
		for(int i = 0; i < MAX_PLAYERS; ++i)
		{
			if(m_aClients[i].m_State != CClient::STATE_EMPTY)
			{
				SendServerInfo(m_NetServer.ClientAddr(i), -1, SERVERINFO_INGAME, false);
			}
		}
	}

	m_ServerInfoNeedsUpdate = false;
}

void CServer::SendServerInfo(const NETADDR* pAddr, int Token, int Type, bool SendClients)
{
	CPacker p;
	char aBuf[128];
	p.Reset();

	CBrowserCache* pCache = &m_aServerInfoCache[GetCacheIndex(Type, SendClients)];

#define ADD_RAW(p, x) (p).AddRaw(x, sizeof(x))
#define ADD_INT(p, x) \
	do \
	{ \
		str_from_int(x, aBuf); \
		(p).AddString(aBuf, 0); \
	} while(0)

	CNetChunk Packet;
	Packet.m_ClientID = -1;
	Packet.m_Address = *pAddr;
	Packet.m_Flags = NETSENDFLAG_CONNLESS;

	for(const auto& Chunk : pCache->m_vCache)
	{
		p.Reset();
		if(Type == SERVERINFO_EXTENDED)
		{
			if(&Chunk == &pCache->m_vCache.front())
				p.AddRaw(SERVERBROWSE_INFO_EXTENDED, sizeof(SERVERBROWSE_INFO_EXTENDED));
			else
				p.AddRaw(SERVERBROWSE_INFO_EXTENDED_MORE, sizeof(SERVERBROWSE_INFO_EXTENDED_MORE));
			ADD_INT(p, Token);
		}
		else if(Type == SERVERINFO_64_LEGACY)
		{
			ADD_RAW(p, SERVERBROWSE_INFO_64_LEGACY);
			ADD_INT(p, Token);
		}
		else if(Type == SERVERINFO_VANILLA || Type == SERVERINFO_INGAME)
		{
			ADD_RAW(p, SERVERBROWSE_INFO);
			ADD_INT(p, Token);
		}
		else
		{
			dbg_assert(false, "unknown serverinfo type");
		}

		p.AddRaw(Chunk.m_vData.data(), Chunk.m_vData.size());
		Packet.m_pData = p.Data();
		Packet.m_DataSize = p.Size();
		m_NetServer.Send(&Packet);
	}
}

bool CServer::RateLimitServerInfoConnless()
{
	bool SendClients = true;
	if(g_Config.m_SvServerInfoPerSecond)
	{
		SendClients = m_ServerInfoNumRequests <= g_Config.m_SvServerInfoPerSecond;
		const int64_t Now = Tick();

		if(Now <= m_ServerInfoFirstRequest + TickSpeed())
		{
			m_ServerInfoNumRequests++;
		}
		else
		{
			m_ServerInfoNumRequests = 1;
			m_ServerInfoFirstRequest = Now;
		}
	}

	return SendClients;
}

void CServer::SendServerInfoConnless(const NETADDR* pAddr, int Token, int Type)
{
	SendServerInfo(pAddr, Token, Type, RateLimitServerInfoConnless());
}

bool CServer::LoadMap(int ID)
{
	if(!MultiWorlds()->GetWorld(ID) || !MultiWorlds()->GetWorld(ID)->MapDetail())
		return false;

	return MultiWorlds()->GetWorld(ID)->MapDetail()->Load(m_pStorage);
}

int CServer::Run(ILogger* pLogger)
{
	if(m_RunServer == UNINITIALIZED)
		m_RunServer = RUNNING;

	// initilize instance data
	Instance::Data::g_pServer = static_cast<IServer*>(this);

	// initialize fmt_default localize function
	g_fmt_localize.use_flags(FMTFLAG_HANDLE_ARGS);
	g_fmt_localize.init(&CServer::CallbackLocalize, this);

	// initilize localization
	m_pLocalization = new CLocalization();
	if(!m_pLocalization->Init())
	{
		dbg_msg("localization", "could not initialize localization");
		return -1;
	}

	// loading maps to memory
	char aBuf[256];
	for(int i = 0; i < MultiWorlds()->GetSizeInitilized(); i++)
	{
		if(!LoadMap(i))
		{
			dbg_msg("server", "%s the map is not loaded...", MultiWorlds()->GetWorld(i)->GetPath());
			return -1;
		}
	}

	// start server
	{
		NETADDR BindAddr;
		if(g_Config.m_Bindaddr[0] == '\0')
		{
			mem_zero(&BindAddr, sizeof(BindAddr));
		}
		else if(net_host_lookup(g_Config.m_Bindaddr, &BindAddr, NETTYPE_ALL) != 0)
		{
			dbg_msg("server", "The configured bindaddr '%s' cannot be resolved", g_Config.m_Bindaddr);
			return -1;
		}
		BindAddr.type = g_Config.m_SvIpv4Only ? NETTYPE_IPV4 : NETTYPE_ALL;

		int Port = g_Config.m_SvPort;
		for(BindAddr.port = Port != 0 ? Port : 8303; !m_NetServer.Open(BindAddr, m_pServerBan, MAX_PLAYERS, g_Config.m_SvMaxClientsPerIP); BindAddr.port++)
		{
			if(Port != 0 || BindAddr.port >= 8310)
			{
				dbg_msg("server", "couldn't open socket. port %d might already be in use", BindAddr.port);
				return -1;
			}
		}

		if(Port == 0)
		{
			dbg_msg("server", "using port %d", BindAddr.port);
		}
	}

	// initilize components
	IEngine* pEngine = Kernel()->RequestInterface<IEngine>();
	IHttp* pHttp = Kernel()->RequestInterface<IHttp>();

	m_pRegister = CreateRegister(&g_Config, m_pConsole, pEngine, pHttp, m_NetServer.Address().port, m_NetServer.GetGlobalToken());
	m_NetServer.SetCallbacks(NewClientCallback, NewClientNoAuthCallback, ClientRejoinCallback, DelClientCallback, this);
	m_Econ.Init(&g_Config, Console(), m_pServerBan);
	m_pInputKeys = CreateInputKeys();

	// initialize geolite2pp
	CGeoIP::init("server_data/Country.mmdb");

	// server name
	str_format(aBuf, sizeof(aBuf), "server name is '%s'", g_Config.m_SvName);
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

	// check correcting initlized worlds
	if(!MultiWorlds()->GetSizeInitilized())
	{
		dbg_msg("server", "the worlds were not found or were not initialized");
		return -1;
	}

	// initilize game server
	for(int i = 0; i < MultiWorlds()->GetSizeInitilized(); i++)
		MultiWorlds()->GetWorld(i)->GameServer()->OnInit(i);

	// initilize nicknames
	InitBaseAccounts();

	// starting information
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "----------------------------------------------------");
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "███╗   ███╗██████╗ ██████╗  ██████╗ ");
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "████╗ ████║██╔══██╗██╔══██╗██╔════╝ ");
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "██╔████╔██║██████╔╝██████╔╝██║  ███╗");
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "██║╚██╔╝██║██╔══██╗██╔═══╝ ██║   ██║");
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "██║ ╚═╝ ██║██║  ██║██║     ╚██████╔╝");
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "╚═╝     ╚═╝╚═╝  ╚═╝╚═╝      ╚═════╝ ");
		str_format(aBuf, sizeof(aBuf), "initialized accounts: %lu", m_vBaseAccounts.size());
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		str_format(aBuf, sizeof(aBuf), "initialized worlds: %d", MultiWorlds()->GetSizeInitilized());
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		str_format(aBuf, sizeof(aBuf), "version: %s", GameServer()->NetVersion());
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

		// process pending commands
		m_pConsole->StoreCommands(false);
		m_pRegister->OnConfigChange();

		if(m_GeneratedRconPassword)
		{
			dbg_msg("server", "rcon password: '%s'", g_Config.m_SvRconPassword);
		}

		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "----------------------------------------------------");
	}

	// start game
	{
		bool NonActive = false;
		bool PacketWaiting = false;
		CServerLogger* pServerLogger = static_cast<CServerLogger*>(pLogger);

		m_GameStartTime = time_get();

		UpdateServerInfo();
		while(m_RunServer < STOPPING)
		{
			if(NonActive)
			{
				PumpNetwork(PacketWaiting);
			}

			set_new_tick();

			int64_t t = time_get();
			bool NewTicks = false;

			while(t > TickStartTime(m_CurrentGameTick + 1))
			{
				m_CurrentGameTick++;
				NewTicks = true;

				// apply new input
				for(int c = 0; c < MAX_PLAYERS; c++)
				{
					if(m_aClients[c].m_State == CClient::STATE_EMPTY)
						continue;

					for(auto& m_aInput : m_aClients[c].m_aInputs)
					{
						if(m_aInput.m_GameTick == Tick())
						{
							if(m_aClients[c].m_State == CClient::STATE_INGAME)
							{
								const int WorldID = m_aClients[c].m_WorldID;
								GameServer(WorldID)->OnClientPredictedInput(c, m_aInput.m_aData);
							}
							break;
						}
					}
				}

				// update game time
				if(Tick() % TickSpeed() == 0)
				{
					// update game typeday
					if(m_GameTypeday != GetCurrentTypeday())
					{
						m_GameTypeday = GetCurrentTypeday();
						for(int i = 0; i < MultiWorlds()->GetSizeInitilized(); i++)
						{
							IGameServer* pGameServer = MultiWorlds()->GetWorld(i)->GameServer();
							pGameServer->OnDaytypeChange(m_GameTypeday);
						}
					}

					// update game minute
					m_GameMinuteTime++;
					if(m_GameMinuteTime >= 60)
					{
						// update game hour
						m_GameHourTime++;
						if(m_GameHourTime >= 24)
						{
							m_GameHourTime = 0;
							SetOffsetGameTime(0);
						}

						// reset game minute
						m_GameMinuteTime = 0;
					}
				}

				MultiWorlds()->GetWorld(MAIN_WORLD_ID)->GameServer()->OnTickGlobal();
				for(int i = 0; i < MultiWorlds()->GetSizeInitilized(); i++)
				{
					IGameServer* pGameServer = MultiWorlds()->GetWorld(i)->GameServer();
					pGameServer->OnTick();
				}
			}

			if(NewTicks)
			{
				// Check if the server is non-active and if the current game tick has exceeded the specified number of
				// days for a hard reset, or if a heavy reload is requested.
				if((NonActive && m_CurrentGameTick > (g_Config.m_SvHardresetAfterDays * 4320000)) || m_HeavyReload)
				{
					m_CurrentGameTick = 0;
					m_GameStartTime = time_get();
					m_ServerInfoFirstRequest = 0;
					SetOffsetGameTime(0);

					// Reset localization
					if(!m_pLocalization->Reload())
					{
						log_error("server", "localization for heavy reload could not be updated.");
						return -1;
					}

					// Check if the worlds were loaded successfully
					if(!MultiWorlds()->LoadFromDB(Kernel()))
					{
						log_error("server", "interfaces for heavy reload could not be updated.");
						return -1;
					}

					// iterate through all initialized worlds
					for(int i = 0; i < MultiWorlds()->GetSizeInitilized(); i++)
					{
						// load map data for the current world
						if(!LoadMap(i))
						{
							log_error("server", "%s the map is not loaded.", MultiWorlds()->GetWorld(i)->GetPath());
							return -1;
						}
					}

					// check if heavy reload is needed
					if(m_HeavyReload)
					{
						// reload players
						for(int ClientID = 0; ClientID < MAX_PLAYERS; ClientID++)
						{
							// skip clients that are not fully connected
							if(m_aClients[ClientID].m_State <= CClient::STATE_AUTH)
								continue;

							// reset client data
							m_aClients[ClientID].Reset();
							m_aClients[ClientID].m_State = CClient::STATE_CONNECTING;
							m_aClients[ClientID].m_ChangeWorld = false;

							// send map to client
							SendMap(ClientID);
						}

						// reset heavy reload flag
						m_HeavyReload = false;
					}
					else
					{
						// initialize the game
						Init();
					}

					// Reinitialize the game context for each initialized world
					for(int i = 0; i < MultiWorlds()->GetSizeInitilized(); i++)
					{
						IGameServer* pGameServer = MultiWorlds()->GetWorld(i)->GameServer();
						pGameServer->OnInit(i);
					}

					// Update the server information
					UpdateServerInfo(true);

					// Print a message to the console indicating that a server was reloaded heavily
					Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "A server was heavy reload.");
				}
				else
				{
					// check if the server has high bandwidth or if the current game tick is even
					if(g_Config.m_SvHighBandwidth || (m_CurrentGameTick % 2) == 0)
					{
						// perform a snapshot
						for(int i = 0; i < MultiWorlds()->GetSizeInitilized(); i++)
							DoSnapshot(i);
					}

					// reset all input client keys
					m_pInputKeys->ResetInputKeys();

					// update client RCON commands
					UpdateClientRconCommands();
				}
			}

			// Perform updates
			m_pRegister->Update();
			pServerLogger->Update();

			// Check if the server info needs to be updated
			if(m_ServerInfoNeedsUpdate)
				UpdateServerInfo();

			// Check if the server is not in a non-active state
			if(!NonActive)
			{
				// Pump the network to process any waiting packets
				PumpNetwork(PacketWaiting);
			}

			// Check if the server is in a non-active state
			NonActive = std::ranges::none_of(m_aClients, [](const auto& client) { return client.m_State != CClient::STATE_EMPTY; });

			// Wait for incoming data if the server is in a non-active state
			if(NonActive)
			{
				// Wait for incoming data with a timeout of 1000000 microseconds (1 second)
				PacketWaiting = net_socket_read_wait(m_NetServer.Socket(), 1000000);
			}
			else
			{
				// Set a new tick and calculate the time until the next tick
				set_new_tick();
				auto t = time_get();
				auto x = (TickStartTime(m_CurrentGameTick + 1) - t) * 1000000 / time_freq() + 1;

				// Wait for incoming data with a timeout of x microseconds
				// If x is greater than 0, otherwise set PacketWaiting to true
				PacketWaiting = x > 0 ? net_socket_read_wait(m_NetServer.Socket(), x) : true;
			}
		}
	}

	// shutdown
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		if(m_aClients[i].m_State != CClient::STATE_EMPTY)
			Kick(i, "Server shutdown");
	}

	delete m_pInputKeys;
	m_Econ.Shutdown();
	m_NetServer.Close();
	m_pRegister->OnShutdown();
	CGeoIP::free();
	return 0;
}

// Kick a player from the server
void CServer::ConKick(IConsole::IResult* pResult, void* pUser)
{
	// Check if there are more than 1 argument
	if(pResult->NumArguments() > 1)
	{
		char aBuf[128];
		// Create the kick message with the reason provided
		str_format(aBuf, sizeof(aBuf), "Kicked (%s)", pResult->GetString(1));
		// Kick the player with the specified client ID and kick message
		((CServer*)pUser)->Kick(pResult->GetInteger(0), aBuf);
	}
	else
	{
		// Kick the player with the specified client ID and default kick message
		((CServer*)pUser)->Kick(pResult->GetInteger(0), "Kicked by console");
	}
}

// Display the status of all connected players
void CServer::ConStatus(IConsole::IResult* pResult, void* pUser)
{
	char aBuf[1024];
	char aAddrStr[NETADDR_MAXSTRSIZE];
	CServer* pThis = static_cast<CServer*>(pUser);

	// Loop through all clients
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		// Check if the client is not empty
		if(pThis->m_aClients[i].m_State != CClient::STATE_EMPTY)
		{
			// Get the address of the client as a string
			net_addr_str(pThis->m_NetServer.ClientAddr(i), aAddrStr, sizeof(aAddrStr), true);

			// Check the state of the client
			if(pThis->m_aClients[i].m_State == CClient::STATE_INGAME)
			{
				const char* pAuthStr = pThis->m_aClients[i].m_Authed == AUTHED_ADMIN ? "(Admin)" :
					pThis->m_aClients[i].m_Authed == AUTHED_MOD ? "(Mod)" : "";
				// Create the status message for an ingame client
				str_format(aBuf, sizeof(aBuf), "id=%d addr=%s client=%d name='%s' score=%d %s", i, aAddrStr,
					pThis->m_aClients[i].m_DDNetVersion, pThis->m_aClients[i].m_aName, pThis->m_aClients[i].m_Score, pAuthStr);
			}
			else
			{
				// Create the status message for a connecting client
				str_format(aBuf, sizeof(aBuf), "id=%d addr=%s connecting", i, aAddrStr);
			}
			// Print the status message to the console
			pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", aBuf);
		}
	}
}

// Shutdown the server
void CServer::ConShutdown(IConsole::IResult* pResult, void* pUser)
{
	CServer* pThis = static_cast<CServer*>(pUser);
	pThis->m_RunServer = STOPPING;
}

// Reload the server
void CServer::ConReload(IConsole::IResult* pResult, void* pUser)
{
	((CServer*)pUser)->m_HeavyReload = true;
}

// Logout the Rcon client
void CServer::ConLogout(IConsole::IResult* pResult, void* pUser)
{
	CServer* pServer = (CServer*)pUser;

	// Check if the Rcon client is valid
	if(pServer->m_RconClientID >= 0 && pServer->m_RconClientID < MAX_PLAYERS &&
		pServer->m_aClients[pServer->m_RconClientID].m_State != CClient::STATE_EMPTY)
	{
		// Reset the authentication status and tries for the Rcon client
		pServer->m_aClients[pServer->m_RconClientID].m_Authed = AUTHED_NO;
		pServer->m_aClients[pServer->m_RconClientID].m_AuthTries = 0;
		pServer->m_aClients[pServer->m_RconClientID].m_pRconCmdToSend = nullptr;

		// Send a logout successful message to the Rcon client
		pServer->SendRconLine(pServer->m_RconClientID, "Logout successful.");

		// Send an RCON_AUTH_STATUS message to the Rcon client
		{
			CMsgPacker Msgp(NETMSG_RCON_AUTH_STATUS, true);
			Msgp.AddInt(0); // authed
			Msgp.AddInt(0); // cmdlist
			pServer->SendMsg(&Msgp, MSGFLAG_VITAL, pServer->m_RconClientID);
		}

		char aBuf[32];
		str_format(aBuf, sizeof(aBuf), "ClientID=%d logged out", pServer->m_RconClientID);
		pServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
	}
}

// Function to update special server info
void CServer::ConchainSpecialInfoupdate(IConsole::IResult* pResult, void* pUserData, IConsole::FCommandCallback pfnCallback, void* pCallbackUserData)
{
	// Call the callback function with the given result and user data
	pfnCallback(pResult, pCallbackUserData);

	// Check if there are any arguments in the result
	if(pResult->NumArguments())
	{
		// Clean the white spaces in the server name
		str_clean_whitespaces(g_Config.m_SvName);
		// Update the server info
		((CServer*)pUserData)->UpdateServerInfo(true);
	}
}

// Function to update the maximum clients per IP
void CServer::ConchainMaxclientsperipUpdate(IConsole::IResult* pResult, void* pUserData, IConsole::FCommandCallback pfnCallback, void* pCallbackUserData)
{
	// Call the callback function with the given result and user data
	pfnCallback(pResult, pCallbackUserData);

	// Check if there are any arguments in the result
	if(pResult->NumArguments())
	{
		// Set the maximum clients per IP in the network server
		((CServer*)pUserData)->m_NetServer.SetMaxClientsPerIP(pResult->GetInteger(0));
	}
}

// Function to update a mod command
void CServer::ConchainModCommandUpdate(IConsole::IResult* pResult, void* pUserData, IConsole::FCommandCallback pfnCallback, void* pCallbackUserData)
{
	// Check if there are two arguments in the result
	if(pResult->NumArguments() == 2)
	{
		// Cast the user data to CServer*
		CServer* pThis = static_cast<CServer*>(pUserData);
		// Get the command info from the console
		const IConsole::CCommandInfo* pInfo = pThis->Console()->GetCommandInfo(pResult->GetString(0), CFGFLAG_SERVER, false);
		int OldAccessLevel = 0;
		// Check if the command info exists
		if(pInfo)
			OldAccessLevel = pInfo->GetAccessLevel();
		// Call the callback function with the given result and user data
		pfnCallback(pResult, pCallbackUserData);
		// Check if the command info exists and the access level has changed
		if(pInfo && OldAccessLevel != pInfo->GetAccessLevel())
		{
			// Loop through all clients
			for(int i = 0; i < MAX_PLAYERS; ++i)
			{
				// Check if the client is empty, authenticated as a moderator,
				// and the command to send is not null and greater or equal to the current command
				if(pThis->m_aClients[i].m_State == CClient::STATE_EMPTY || pThis->m_aClients[i].m_Authed != AUTHED_MOD ||
					(pThis->m_aClients[i].m_pRconCmdToSend && str_comp(pResult->GetString(0), pThis->m_aClients[i].m_pRconCmdToSend->m_pName) >= 0))
					continue;

				// Check if the old access level was admin
				if(OldAccessLevel == IConsole::ACCESS_LEVEL_ADMIN)
					pThis->SendRconCmdAdd(pInfo, i);
				else
					pThis->SendRconCmdRem(pInfo, i);
			}
		}
	}
	else
	{
		// Call the callback function with the given result and user data
		pfnCallback(pResult, pCallbackUserData);
	}
}

// Function to update the RCON password
void CServer::ConchainRconPasswordSet(IConsole::IResult* pResult, void* pUserData, IConsole::FCommandCallback pfnCallback, void* pCallbackUserData)
{
	// Call the callback function with the given result and user data
	pfnCallback(pResult, pCallbackUserData);

	// Check if there is at least one argument in the result
	if(pResult->NumArguments() >= 1)
	{
		// Set the RCON password flag to true
		static_cast<CServer*>(pUserData)->m_RconPasswordSet = 1;
	}
}

// Function to update the log level
void CServer::ConchainLoglevel(IConsole::IResult* pResult, void* pUserData, IConsole::FCommandCallback pfnCallback, void* pCallbackUserData)
{
	// Cast the user data to CServer*
	CServer* pSelf = (CServer*)pUserData;
	// Call the callback function with the given result and user data
	pfnCallback(pResult, pCallbackUserData);

	// Check if there are any arguments in the result
	if(pResult->NumArguments())
	{
		// Set the log filter of the file logger using the log level from the configuration
		pSelf->m_pFileLogger->SetFilter(CLogFilter { IConsole::ToLogLevelFilter(g_Config.m_Loglevel) });
	}
}

// Function to update the stdout output level
void CServer::ConchainStdoutOutputLevel(IConsole::IResult* pResult, void* pUserData, IConsole::FCommandCallback pfnCallback, void* pCallbackUserData)
{
	// Cast the user data to CServer*
	CServer* pSelf = (CServer*)pUserData;
	// Call the callback function with the given result and user data
	pfnCallback(pResult, pCallbackUserData);

	// Check if there are any arguments in the result and if the stdout logger exists
	if(pResult->NumArguments() && pSelf->m_pStdoutLogger)
	{
		// Set the log filter of the stdout logger using the output level from the configuration
		pSelf->m_pStdoutLogger->SetFilter(CLogFilter { IConsole::ToLogLevelFilter(g_Config.m_StdoutOutputLevel) });
	}
}

std::string CServer::CallbackLocalize(int ClientID, const char* pText, void* pUser)
{
	auto pServer = static_cast<IServer*>(pUser);
	return pServer->Localize(ClientID, pText);
}

// This function is used to register commands for the server
void CServer::RegisterCommands()
{
	// Request console and storage interfaces
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	m_pStorage = Kernel()->RequestInterface<IStorageEngine>();

	m_Http.Init(std::chrono::seconds { 2 });
	Kernel()->RegisterInterface(static_cast<IHttp*>(&m_Http), false);

	// Register console commands
	Console()->Register("kick", "i[id] ?r[reason]", CFGFLAG_SERVER, ConKick, this, "Kick player with specified id for any reason");
	Console()->Register("status", "", CFGFLAG_SERVER, ConStatus, this, "List players");
	Console()->Register("shutdown", "", CFGFLAG_SERVER, ConShutdown, this, "Shut down");
	Console()->Register("reload", "", CFGFLAG_SERVER, ConReload, this, "Reload maps and synchronize data with the database");
	Console()->Register("logout", "", CFGFLAG_SERVER, ConLogout, this, "Logout of rcon");

	// Chain console commands
	Console()->Chain("sv_name", ConchainSpecialInfoupdate, this);
	Console()->Chain("password", ConchainSpecialInfoupdate, this);
	Console()->Chain("sv_max_clients_per_ip", ConchainMaxclientsperipUpdate, this);
	Console()->Chain("mod_command", ConchainModCommandUpdate, this);
	Console()->Chain("sv_rcon_password", ConchainRconPasswordSet, this);
	Console()->Chain("loglevel", ConchainLoglevel, this);
	Console()->Chain("stdout_output_level", ConchainStdoutOutputLevel, this);

	// Initialize server ban
	m_pServerBan->InitServerBan(Console(), Storage(), this, &m_NetServer);
}

// This function initializes a client bot for the server
void CServer::InitClientBot(int ClientID)
{
	// Check if the ClientID is within valid range
	if(ClientID < MAX_PLAYERS || ClientID >= MAX_CLIENTS)
		return;

	// Prepare client data
	m_aClients[ClientID].m_State = CClient::STATE_INGAME;
	m_aClients[ClientID].m_WorldID = -1;
	m_aClients[ClientID].m_Score = 1;

	// Send a connection ready message to the client
	SendConnectionReady(ClientID);
}

// Function to get a new ID from the ID pool
int CServer::SnapNewID()
{
	// Return a new ID from the ID pool
	return m_IDPool.NewID();
}

// Function to free an ID in the ID pool
void CServer::SnapFreeID(int ID)
{
	// Free the specified ID in the ID pool
	m_IDPool.FreeID(ID);
}

// Function to create a new item in the snapshot builder
void* CServer::SnapNewItem(int Type, int ID, int Size)
{
	// Check if the ID is within the valid range
	dbg_assert(ID >= 0 && ID <= 0xffff, "incorrect id");

	// If the ID is less than 0, return nullptr
	if(ID < 0)
		return nullptr;

	// Create a new item in the snapshot builder with the specified type, ID, and size
	return m_SnapshotBuilder.NewItem(Type, ID, Size);
}

// It sets the static size of a snapshot item
void CServer::SnapSetStaticsize(int ItemType, int Size)
{
	// Call the SetStaticsize function of the m_SnapshotDelta object with the given ItemType and Size
	m_SnapshotDelta.SetStaticsize(ItemType, Size);
}

// This function returns a pointer to the starting position of the id map
// for a specific client in the server.
int* CServer::GetIdMap(int ClientID)
{
	return m_aIdMap + VANILLA_MAX_CLIENTS * ClientID;
}

// This function initializes the account nicknames for the server
void CServer::InitBaseAccounts()
{
	if(!m_vBaseAccounts.empty())
		return;

	ResultPtr pRes = Database->Execute<DB::SELECT>("ID, Nick, Rating", "tw_accounts_data");
	m_vBaseAccounts.reserve(pRes->rowsCount() + 10000);
	while(pRes->next())
	{
		const int UID = pRes->getInt("ID");
		const auto Nick = pRes->getString("Nick");
		const auto Rating = pRes->getInt("Rating");
		UpdateAccountBase(UID, Nick, Rating);
	}
}

void CServer::UpdateAccountBase(int UID, std::string Nickname, int Rating)
{
	BaseAccount account { Nickname, Rating };
	auto it = m_vBaseAccounts.find(UID);
	if(it != m_vBaseAccounts.end())
		m_vSortedRankAccounts.erase(it->second);

	m_vBaseAccounts[UID] = account;
	m_vSortedRankAccounts.insert(account);
}

const char* CServer::GetAccountNickname(int AccountID)
{
	if(m_vBaseAccounts.find(AccountID) != m_vBaseAccounts.end())
		return m_vBaseAccounts[AccountID].Nickname.c_str();

	return "Empty";
}

int CServer::GetAccountRank(int AccountID)
{
	auto it = m_vBaseAccounts.find(AccountID);
	if(it == m_vBaseAccounts.end())
		return -1;

	auto sortedIt = m_vSortedRankAccounts.find(it->second);
	if(sortedIt == m_vSortedRankAccounts.end())
		return -1;

	return std::distance(m_vSortedRankAccounts.begin(), sortedIt) + 1;
}

// This function sets the loggers for the server
void CServer::SetLoggers(std::shared_ptr<ILogger>&& pFileLogger, std::shared_ptr<ILogger>&& pStdoutLogger)
{
	// Set the file logger for the server
	m_pFileLogger = pFileLogger;

	// Set the stdout logger for the server
	m_pStdoutLogger = pStdoutLogger;
}

CServer* CreateServer() { return new CServer(); }