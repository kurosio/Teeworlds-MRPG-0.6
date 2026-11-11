#include "server_sync.h"

#include <base/system.h>
#include <engine/shared/config.h>

#include <relay/relay_json.h>
#include <relay/relay_stream.h>

#include <algorithm>
#include <cstddef>
#include <exception>

namespace
{
	// How long a non-blocking connect may take before it is given up.
	constexpr int CONNECT_TIMEOUT_MS = 10000;
	// How often the same disconnect reason is repeated while reconnecting.
	constexpr int64_t DISCONNECT_LOG_INTERVAL_MS = 60000;

	// Resolves the relay address once per host, a lookup on every reconnect is
	// wasteful. The configured port is always taken from the config.
	bool ResolveRelay(NETADDR *pAddr)
	{
		static NETADDR s_Addr;
		static char s_aHost[128];

		if(str_comp(s_aHost, g_Config.m_SvSyncRelayHost) != 0)
		{
			NETADDR Addr;
			mem_zero(&Addr, sizeof(Addr));
			if(net_host_lookup(g_Config.m_SvSyncRelayHost, &Addr, NETTYPE_ALL) != 0)
			{
				dbg_msg("server_sync", "the relay host '%s' cannot be resolved", g_Config.m_SvSyncRelayHost);
				str_copy(s_aHost, g_Config.m_SvSyncRelayHost);
				return false;
			}

			// Keep the family net_host_lookup() resolved: connect() converts
			// the address per family and cannot handle a bitset.
			if(Addr.type != NETTYPE_IPV4 && Addr.type != NETTYPE_IPV6)
				Addr.type = NETTYPE_IPV4;

			s_Addr = Addr;
			str_copy(s_aHost, g_Config.m_SvSyncRelayHost);
		}

		*pAddr = s_Addr;
		pAddr->port = (unsigned short)g_Config.m_SvSyncRelayPort;
		return true;
	}
}

CServerSync::~CServerSync()
{
	Disconnect("shutdown");
}

bool CServerSync::Enabled() const
{
	return g_Config.m_SvSyncServerID > 0 && g_Config.m_SvSyncRelayHost[0] != '\0';
}

int CServerSync::NextMessageID()
{
	const int ID = m_NextID;
	// The ID only has to be unique for a while, the receivers keep a bounded
	// cache of the last MAX_PROCESSED_CACHE ids.
	m_NextID = m_NextID >= 1000000000 ? 1 : m_NextID + 1;
	return ID;
}

bool CServerSync::IsValidEventName(const char* pEvent)
{
	if(!pEvent || !*pEvent || str_length(pEvent) > MAX_EVENT_NAME_LENGTH)
		return false;

	// The name is forwarded to the database code of the other servers, so it
	// has to stay a short printable identifier.
	for(const char* pCharacter = pEvent; *pCharacter; pCharacter++)
	{
		if((unsigned char)*pCharacter < 0x20 || *pCharacter == 0x7f)
			return false;
	}
	return true;
}

int CServerSync::Push(const char* pEvent, int WorldID, int Value)
{
	if(!Enabled() || WorldID < 0 || !IsValidEventName(pEvent))
		return 0;

	const int ID = NextMessageID();

	// Queued even while the relay is unreachable: the messages are sent as soon
	// as the connection is back, so a relay restart does not lose the events.
	const std::string Payload = relay::BuildJson({
		{"id", ID},
		{"event", pEvent},
		{"server", g_Config.m_SvSyncServerID},
		{"world", WorldID},
		{"value", Value},
	});
	if(Payload.empty())
		return 0;

	Queue(relay::EMsg::EVENT, Payload, ID);
	return ID;
}

void CServerSync::Queue(relay::EMsg Type, std::string Payload, int ID)
{
	// Bound the backlog: the relay is expected to be reachable, an endless queue
	// would only grow the memory of the server.
	if(m_Pending.size() >= MAX_PENDING)
	{
		dbg_msg("server_sync", "the relay queue is full (%d messages), dropping the message %d", (int)MAX_PENDING, ID);
		return;
	}

	m_Pending.push_back({ID, Type, std::move(Payload)});
	// The queue is only moved to the socket by FlushSocket() from OnTick(), which
	// owns the error handling. Sending from inside a receive callback would let a
	// send failure close the connection under the reader.
}

void CServerSync::QueueJson(relay::EMsg Type, const nlohmann::json& Json)
{
	std::string Payload = relay::BuildJson(Json);
	if(!Payload.empty())
		Queue(Type, std::move(Payload));
}

void CServerSync::EnsureConnection()
{
	if(!Enabled())
	{
		if(m_State != EState::OFFLINE)
			Disconnect("synchronization disabled");
		return;
	}

	if(m_State != EState::OFFLINE)
		return;

	const int64_t Now = relay::NowMs();
	if(m_LastConnectAttempt != 0 && Now - m_LastConnectAttempt < g_Config.m_SvSyncRelayReconnectMs)
		return;

	m_LastConnectAttempt = Now;

	NETADDR Addr;
	if(!ResolveRelay(&Addr))
		return;

	// The address to connect to is not an address to bind to: net_tcp_create()
	// binds the socket, and binding to the relay address or port makes the
	// connect fail. Let the system pick the local address and port.
	NETADDR BindAddr;
	mem_zero(&BindAddr, sizeof(BindAddr));
	BindAddr.type = NETTYPE_IPV4;

	NETSOCKET Socket = net_tcp_create(BindAddr);
	if(!Socket)
	{
		dbg_msg("server_sync", "could not create the socket for the relay");
		return;
	}

	// A non-blocking connect returns before the handshake is done; the socket
	// becomes writable when it is connected and readable when it failed.
	// net_tcp_connect_non_blocking() restores the blocking mode, so the socket
	// has to be switched back afterwards.
	net_tcp_connect_non_blocking(Socket, Addr);
	net_set_non_blocking(Socket);

	m_Socket = Socket;
	m_Reader.Reset();
	m_SendBuffer.clear();
	m_InFlight = 0;
	m_State = EState::CONNECTING;
	m_ConnectStartTime = Now;
	m_LastRecvTime = Now;

	// The handshake has to be the first frame of the connection, even when
	// events were already queued while the relay was unreachable.
	const std::string Payload = relay::BuildJson({
		{"protocol", relay::PROTOCOL_VERSION},
		{"server_id", g_Config.m_SvSyncServerID},
		{"password", g_Config.m_SvSyncRelayPassword},
		{"name", g_Config.m_SvName},
	});
	m_Pending.push_front({0, relay::EMsg::HELLO, Payload});
}

void CServerSync::Disconnect(const char* pReason)
{
	// A relay that is down produces one failed attempt per reconnect delay. The
	// reason is logged when the connection worked before, when it changes, and
	// once a minute otherwise, so the log stays readable.
	if(pReason && m_State != EState::OFFLINE)
	{
		const int64_t Now = relay::NowMs();
		const bool Repeat = m_State == EState::CONNECTING && m_LastDisconnectReason == pReason &&
				    Now - m_LastDisconnectLogTime < DISCONNECT_LOG_INTERVAL_MS;
		if(!Repeat)
		{
			dbg_msg("server_sync", "relay connection closed: %s", pReason);
			m_LastDisconnectLogTime = Now;
		}
		m_LastDisconnectReason = pReason;
	}

	if(m_Socket)
	{
		net_tcp_close(m_Socket);
		m_Socket = nullptr;
	}

	m_State = EState::OFFLINE;
	m_Reader.Reset();
	// The frames of the broken connection are re-sent on the next one, the
	// receivers drop the duplicates by the message id.
	m_SendBuffer.clear();
	m_InFlight = 0;
	m_Pending.erase(std::remove_if(m_Pending.begin(), m_Pending.end(), [](const SPendingMessage& Message) {
		return Message.Type == relay::EMsg::HELLO;
	}), m_Pending.end());
	m_LastRecvTime = 0;
	m_ConnectStartTime = 0;
}

void CServerSync::OnTick()
{
	// The relay is reachable by anyone, so a defect in the handling of a message
	// must never propagate into the game loop: the connection is dropped and the
	// server keeps running.
	try
	{
		UpdateConnection();
	}
	catch(const std::exception& e)
	{
		dbg_msg("server_sync", "unexpected error in the synchronization: %s", e.what());
		Disconnect("internal error");
	}
	catch(...)
	{
		dbg_msg("server_sync", "unexpected error in the synchronization");
		Disconnect("internal error");
	}
}

void CServerSync::UpdateConnection()
{
	if(!Enabled())
	{
		if(m_State != EState::OFFLINE)
			Disconnect("synchronization disabled");
		return;
	}

	EnsureConnection();
	if(m_State == EState::OFFLINE)
		return;

	// Reads also tell whether the non-blocking connect has finished.
	ReadSocket();
	if(m_State == EState::OFFLINE)
		return;

	FlushSocket();
	if(m_State == EState::OFFLINE)
		return;

	CheckTimeouts();
}

void CServerSync::CheckTimeouts()
{
	const int64_t Now = relay::NowMs();

	// A connect that neither succeeds nor fails has to be given up as well: the
	// socket may stay silent forever when the relay host does not answer.
	if(m_State == EState::CONNECTING && Now - m_ConnectStartTime > CONNECT_TIMEOUT_MS)
	{
		Disconnect("connection to the relay timed out");
		return;
	}

	if(Now - m_LastRecvTime > relay::CONNECTION_TIMEOUT_MS)
	{
		Disconnect("the relay does not answer");
		return;
	}

	// PING is cheap and also keeps a connection alive whose relay never sends
	// anything itself. The state does not matter: before the handshake the relay
	// simply ignores it.
	if(Now - m_LastKeepalive > relay::KEEPALIVE_INTERVAL_MS)
	{
		m_LastKeepalive = Now;
		QueueJson(relay::EMsg::PING, {{"t", Now}});
	}
}

void CServerSync::ReadSocket()
{
	const relay::SStreamRead Read = relay::Receive(m_Socket, m_Reader);
	if(Read.Bytes > 0)
	{
		m_LastRecvTime = relay::NowMs();
		// The relay accepted the connection; the first frame it sends is the
		// answer to the handshake, handled like any other.
		m_State = EState::ONLINE;
	}

	if(m_Reader.Error())
	{
		Disconnect("protocol error");
		return;
	}

	if(Read.Closed)
	{
		Disconnect("the relay closed the connection");
		return;
	}

	if(Read.Error)
	{
		Disconnect("connection failure");
		return;
	}

	relay::EMsg Type;
	std::string Payload;
	while(m_State != EState::OFFLINE && m_Reader.Next(&Type, &Payload))
		HandleFrame(Type, Payload);

	if(m_Reader.Error())
		Disconnect("protocol error");
}

void CServerSync::FlushSocket()
{
	if(!m_Socket)
		return;

	// Copy the messages that are not in the buffer yet, so the whole queue goes
	// out in one write. The buffer keeps the frames until they are really sent.
	while(m_InFlight < m_Pending.size())
	{
		const SPendingMessage& Message = m_Pending[m_InFlight];
		if(!relay::BuildFrame(Message.Type, Message.Payload, m_SendBuffer))
		{
			dbg_msg("server_sync", "dropping the oversized message %d", Message.ID);
			m_Pending.erase(m_Pending.begin() + (std::ptrdiff_t)m_InFlight);
			continue;
		}
		m_InFlight++;
	}

	const relay::SStreamWrite Write = relay::Send(m_Socket, m_SendBuffer);
	if(!Write.Ok())
	{
		Disconnect(Write.Closed ? "the relay is gone" : "failed to send data to the relay");
		return;
	}

	// A successful write means the TCP handshake is done; the relay answers the
	// HELLO frame on its own.
	if(Write.Bytes > 0 && m_State == EState::CONNECTING)
	{
		m_State = EState::ONLINE;
		m_LastRecvTime = relay::NowMs();
	}

	// Everything is on the wire, the messages are not needed any more.
	if(m_SendBuffer.empty() && m_InFlight > 0)
	{
		m_Pending.erase(m_Pending.begin(), m_Pending.begin() + (std::ptrdiff_t)m_InFlight);
		m_InFlight = 0;
	}
}

void CServerSync::HandleFrame(relay::EMsg Type, const std::string& Payload)
{
	switch(Type)
	{
	case relay::EMsg::WELCOME:
		HandleWelcome(Payload);
		return;
	case relay::EMsg::REJECTED:
		HandleRejected(Payload);
		return;
	case relay::EMsg::EVENT:
		HandleEvent(Payload);
		return;
	case relay::EMsg::PING:
		Queue(relay::EMsg::PONG, Payload);
		return;
	case relay::EMsg::PONG:
	case relay::EMsg::LOCALIZATION_HASH:
	case relay::EMsg::LOCALIZATION_REQUEST:
	case relay::EMsg::LOCALIZATION_DATA:
		// TODO(localization): see src/relay/relay_protocol.h. The relay compares
		// the hashes of the language files and sends the newest file to the
		// servers whose copy differs, instead of every server computing the file
		// on its own.
		return;
	case relay::EMsg::HELLO:
		Disconnect("the relay sent a handshake");
		return;
	}
}

void CServerSync::HandleWelcome(const std::string& Payload)
{
	nlohmann::json Json;
	const bool Parsed = relay::ParseJson(Payload, Json);
	const int Peers = Parsed && Json.contains("peers") && Json["peers"].is_array() ? (int)Json["peers"].size() : 0;

	dbg_msg("server_sync", "connected to the relay (server id %d, %d other server(s) online)", g_Config.m_SvSyncServerID, Peers);
	m_LastRejectReason.clear();

	// A reconnection must not keep an outdated keepalive deadline.
	m_LastKeepalive = relay::NowMs();

	AnnounceLocalizationHashes();
}

void CServerSync::AnnounceLocalizationHashes()
{
	// Groundwork for the localization distribution: every language file of this
	// server is announced by its hash, so the relay can tell the copies apart.
	// Sending the files themselves is not implemented yet (relay_protocol.h).
	if(!m_LocalizationHashProvider)
		return;

	for(const auto& [Language, Hash] : m_LocalizationHashProvider())
	{
		if(!Language.empty() && !Hash.empty())
			QueueJson(relay::EMsg::LOCALIZATION_HASH, {{"language", Language}, {"hash", Hash}});
	}
}

void CServerSync::HandleRejected(const std::string& Payload)
{
	nlohmann::json Json;
	const bool Parsed = relay::ParseJson(Payload, Json);
	const std::string Reason = Parsed ? relay::GetCleanString(Json, "reason", 128) : std::string();
	const std::string Display = Reason.empty() ? std::string("unknown") : Reason;

	if(m_LastRejectReason != Display)
	{
		dbg_msg("server_sync", "the relay rejected the connection: %s", Display.c_str());
		m_LastRejectReason = Display;
	}

	Disconnect("rejected by the relay");
}

void CServerSync::HandleEvent(const std::string& Payload)
{
	nlohmann::json Json;
	if(!relay::ParseJson(Payload, Json))
		return;

	SRequest Req;
	Req.ID = relay::GetInt(Json, "id");
	Req.ServerID = relay::GetInt(Json, "server");
	Req.WorldID = relay::GetInt(Json, "world", -1);
	Req.Event = relay::GetString(Json, "event");
	Req.Value = relay::GetInt(Json, "value");

	if(!IsValidEventName(Req.Event.c_str()) || Req.ID <= 0 || Req.WorldID < 0)
		return;

	// Our own event came back, or it was already handled.
	if(Req.ServerID == g_Config.m_SvSyncServerID || IsDuplicate(Req.ID))
		return;

	if(!m_EventCallback)
		return;

	// A throwing handler must not break the queue or cause the event to be
	// delivered again.
	try
	{
		m_EventCallback(Req.Event, Req);
	}
	catch(const std::exception& e)
	{
		dbg_msg("server_sync", "event '%s' (request %d) failed: %s", Req.Event.c_str(), Req.ID, e.what());
	}
	catch(...)
	{
		dbg_msg("server_sync", "event '%s' (request %d) failed with an unknown exception", Req.Event.c_str(), Req.ID);
	}
}

bool CServerSync::IsDuplicate(int RequestID)
{
	const auto It = std::find(m_ProcessedOrder.begin(), m_ProcessedOrder.end(), RequestID);
	if(It != m_ProcessedOrder.end())
		return true;

	// Bounded memory: the oldest ID is forgotten first. A duplicate that arrives
	// later than the cache can remember is handled by the idempotent handlers.
	if(m_ProcessedOrder.size() >= MAX_PROCESSED_CACHE)
		m_ProcessedOrder.pop_front();
	m_ProcessedOrder.push_back(RequestID);
	return false;
}
