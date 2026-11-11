#include "relay.h"

#include <base/logger.h>

#include "relay_json.h"
#include "relay_stream.h"

#include <algorithm>
#include <utility>

namespace
{
	/*
	 * Text of every EDropReason, in the order of the enum. The handshake
	 * reasons are the ones a game server reports to its admin, so they stay short
	 * and descriptive; the rest only reach the log of the relay.
	 */
	const char *const s_apDropReasons[DROP_COUNT] = {
		"",                           // DROP_NONE
		"the connection was closed",   // DROP_REMOTE_CLOSED
		"idle timeout",                // DROP_IDLE_TIMEOUT
		"connection failure",          // DROP_CONNECTION_FAILED
		"protocol error",              // DROP_PROTOCOL_ERROR
		"double handshake",            // DROP_DOUBLE_HANDSHAKE
		"handshake required",          // DROP_HANDSHAKE_REQUIRED
		"unexpected message",          // DROP_UNEXPECTED_MESSAGE
		"message too big",             // DROP_MESSAGE_TOO_BIG
		"send buffer overflow",        // DROP_SEND_BUFFER_OVERFLOW
		"wrong password",              // DROP_WRONG_PASSWORD
		"protocol version mismatch",   // DROP_PROTOCOL_MISMATCH
		"malformed hello",             // DROP_MALFORMED_HELLO
		"invalid server id",           // DROP_INVALID_SERVER_ID
		"server id already connected", // DROP_SERVER_ID_TAKEN
		"the relay is full",           // DROP_RELAY_FULL
	};
	// A peer that simply vanished only shows up as a timeout or as a closed
	// socket. Both are expected, so they do not get a log line of their own.
	bool IsExpectedDrop(EDropReason Reason)
	{
		return Reason == DROP_REMOTE_CLOSED || Reason == DROP_IDLE_TIMEOUT;
	}

	// net_addr_str() only prints complete addresses; the bitsets of a bind
	// address ("any interface", "both families") are printed as IPv4.
	void AddrToString(const NETADDR &Addr, char *pBuffer, int BufferSize)
	{
		NETADDR Printable = Addr;
		if(Printable.type != NETTYPE_IPV4 && Printable.type != NETTYPE_IPV6)
			Printable.type = NETTYPE_IPV4;

		net_addr_str(&Printable, pBuffer, BufferSize, true);
	}
}

bool ParseBindAddr(const char *pBindAddr, int Port, NETADDR *pOut)
{
	mem_zero(pOut, sizeof(*pOut));
	pOut->port = (unsigned short)Port;

	// No bindaddr: all interfaces of both families.
	if(pBindAddr[0] == '\0')
	{
		pOut->type = NETTYPE_ALL;
		return true;
	}

	// net_addr_from_str() wants IPv6 in brackets, and net_host_lookup() would
	// split a bare `::1` at the first colon.
	char aAddr[NETADDR_MAXSTRSIZE];
	if(pBindAddr[0] != '[' && str_find(pBindAddr, ":") != nullptr)
		str_format(aAddr, sizeof(aAddr), "[%s]", pBindAddr);
	else
		str_copy(aAddr, pBindAddr);

	if(net_addr_from_str(pOut, aAddr) != 0 && net_host_lookup(pBindAddr, pOut, NETTYPE_ALL) != 0)
		return false;

	// Only one family can be bound to a concrete address: "::" cannot be
	// combined with IPv4 and "0.0.0.0" cannot be combined with IPv6. The
	// lookup may also answer a host name with the address of either family.
	if(pOut->type != NETTYPE_IPV4 && pOut->type != NETTYPE_IPV6)
		pOut->type = NETTYPE_IPV4;

	// The port of the settings wins over a port in the address.
	pOut->port = (unsigned short)Port;
	return true;
}

CRelay::~CRelay()
{
	Close();
}

const char *CRelay::DropReasonName(EDropReason Reason)
{
	return s_apDropReasons[Reason];
}

int CRelay::ClientCount() const
{
	return (int)std::ranges::count_if(m_vpClients, [](const CClient *pClient) { return pClient->m_HandshakeDone; });
}

bool CRelay::PasswordMatches(const std::string &Password) const
{
	if(m_Config.m_Password.empty())
		return true;

	// Compare the whole buffer, so the duration of the check does not tell how
	// much of a guessed password was correct.
	const size_t Size = std::max(Password.size(), m_Config.m_Password.size());
	unsigned char Result = (unsigned char)(Password.size() != m_Config.m_Password.size());
	for(size_t i = 0; i < Size; i++)
	{
		const char a = i < Password.size() ? Password[i] : 0;
		const char b = i < m_Config.m_Password.size() ? m_Config.m_Password[i] : 0;
		Result |= (unsigned char)(a ^ b);
	}
	return Result == 0;
}

CRelay::CClient *CRelay::FindClientByServerID(int ServerID) const
{
	const auto It = std::ranges::find_if(m_vpClients, [ServerID](const CClient *pClient) {
		return pClient->m_HandshakeDone && pClient->m_ServerID == ServerID;
	});
	return It != m_vpClients.end() ? *It : nullptr;
}

bool CRelay::Open(const CRelayConfig &Config)
{
	Close();
	m_Config = Config;

	// The reason a socket cannot be created (address in use, no permission for the
	// port, address not on this host) is logged by the engine itself; the message
	// here names the address, so the two lines can be tied together.
	char aAddr[NETADDR_MAXSTRSIZE];
	AddrToString(m_Config.m_BindAddr, aAddr, sizeof(aAddr));

	m_ListenSocket = net_tcp_create(m_Config.m_BindAddr);
	if(!m_ListenSocket)
	{
		log_error("relay", "could not create the listening socket on %s (check relay_port and bindaddr)", aAddr);
		return false;
	}

	if(net_tcp_listen(m_ListenSocket, m_Config.m_MaxClients) != 0)
	{
		log_error("relay", "could not listen on %s: is another relay running with the same port?", aAddr);
		net_tcp_close(m_ListenSocket);
		m_ListenSocket = nullptr;
		return false;
	}

	net_set_non_blocking(m_ListenSocket);

	// A host without IPv6 listens on IPv4 only, and the log should say so. The
	// socket type is a bitset, so it is masked down to the families it can be.
	constexpr int BothFamilies = NETTYPE_IPV4 | NETTYPE_IPV6;
	const int Families = net_socket_type(m_ListenSocket) & BothFamilies;
	const char *pFamilies = Families == BothFamilies ? "IPv4 and IPv6" :
		Families == NETTYPE_IPV6 ? "IPv6 only" :
					   "IPv4 only";

	NETADDR Bound = m_Config.m_BindAddr;
	Bound.type = (Families & NETTYPE_IPV6) ? NETTYPE_IPV6 : NETTYPE_IPV4;
	AddrToString(Bound, aAddr, sizeof(aAddr));

	log_info("relay", "listening on %s (%s), max clients: %d", aAddr, pFamilies, m_Config.m_MaxClients);
	return true;
}

void CRelay::Close()
{
	for(CClient *pClient : m_vpClients)
	{
		if(pClient->m_Socket)
			net_tcp_close(pClient->m_Socket);
		delete pClient;
	}
	m_vpClients.clear();
	m_LocalizationHashes.clear();

	if(m_ListenSocket)
	{
		net_tcp_close(m_ListenSocket);
		m_ListenSocket = nullptr;
	}
}

void CRelay::Update()
{
	if(!m_ListenSocket)
		return;

	AcceptClients();

	// A client that is being dropped stays in this list until the end of the
	// update: its pointer is still valid, so a frame handler may reach it.
	const std::vector<CClient *> vpActive = m_vpClients;
	const int64_t Now = relay::NowMs();

	for(CClient *pClient : vpActive)
	{
		if(!pClient->IsDropping())
			ReadClient(*pClient);
	}

	for(CClient *pClient : vpActive)
	{
		if(!pClient->IsDropping())
			FlushSendBuffer(*pClient);

		if(pClient->IsDropping())
			continue;

		const int64_t Timeout = pClient->m_HandshakeDone ? (int64_t)relay::CONNECTION_TIMEOUT_MS : (int64_t)relay::HANDSHAKE_TIMEOUT_MS;
		if(Now - pClient->m_LastRecvTime > Timeout)
			pClient->RequestDrop(DROP_IDLE_TIMEOUT);
	}

	for(CClient *pClient : vpActive)
	{
		if(pClient->IsDropping())
			DropClient(pClient);
	}
}

void CRelay::SendRejection(NETSOCKET Socket, EDropReason Reason)
{
	// Best effort: the socket is closed right away and whatever was written stays
	// in the buffer of the kernel.
	std::string Frame;
	if(relay::BuildFrame(relay::EMsg::REJECTED, relay::BuildJson({{"reason", DropReasonName(Reason)}}), Frame))
		net_tcp_send(Socket, Frame.data(), (int)Frame.size());
	net_tcp_close(Socket);
}

void CRelay::AcceptClients()
{
	while(true)
	{
		NETSOCKET Socket = nullptr;
		NETADDR Addr;
		if(net_tcp_accept(m_ListenSocket, &Socket, &Addr) <= 0 || !Socket)
			return;

		// Before anything is written to it: a peer that never reads its answer
		// must not be able to block the whole relay.
		net_set_non_blocking(Socket);

		if((int)m_vpClients.size() >= m_Config.m_MaxClients)
		{
			// Tell the server why instead of closing the socket silently.
			SendRejection(Socket, DROP_RELAY_FULL);
			log_warn("relay", "connection refused: the client limit of %d is reached", m_Config.m_MaxClients);
			continue;
		}

		auto *pClient = new CClient;
		pClient->m_Socket = Socket;
		pClient->m_LastRecvTime = relay::NowMs();

		char aAddr[NETADDR_MAXSTRSIZE];
		AddrToString(Addr, aAddr, sizeof(aAddr));
		pClient->m_AddrStr = aAddr;

		m_vpClients.push_back(pClient);
		log_info("relay", "connection from %s", pClient->m_AddrStr.c_str());
	}
}

void CRelay::DropClient(CClient *pClient)
{
	const auto It = std::ranges::find(m_vpClients, pClient);
	if(It == m_vpClients.end())
		return;

	m_vpClients.erase(It);

	if(!IsExpectedDrop(pClient->m_DropReason))
	{
		if(pClient->m_ServerID > 0)
			log_info("relay", "server %d '%s' (%s) disconnected: %s", pClient->m_ServerID, pClient->m_Name.c_str(),
				pClient->m_AddrStr.c_str(), DropReasonName(pClient->m_DropReason));
		else
			log_info("relay", "connection %s closed: %s", pClient->m_AddrStr.c_str(), DropReasonName(pClient->m_DropReason));
	}

	if(pClient->m_Socket)
	{
		// Deliver what is still queued (a REJECTED reason, usually).
		relay::Send(pClient->m_Socket, pClient->m_SendBuffer);

		net_tcp_close(pClient->m_Socket);
		pClient->m_Socket = nullptr;
	}

	delete pClient;
}

void CRelay::ReadClient(CClient &Client)
{
	const relay::SStreamRead Read = relay::Receive(Client.m_Socket, Client.m_Reader);
	if(Read.Bytes > 0)
		Client.m_LastRecvTime = relay::NowMs();

	if(Client.m_Reader.Error())
	{
		Client.RequestDrop(DROP_PROTOCOL_ERROR);
		return;
	}

	if(Read.Closed)
	{
		Client.RequestDrop(DROP_REMOTE_CLOSED);
		return;
	}

	if(Read.Error)
	{
		Client.RequestDrop(DROP_CONNECTION_FAILED);
		return;
	}

	// Every complete frame of the new bytes.
	relay::EMsg Type;
	std::string Payload;
	while(!Client.IsDropping() && Client.m_Reader.Next(&Type, &Payload))
		HandleFrame(Client, Type, Payload);

	if(Client.m_Reader.Error())
		Client.RequestDrop(DROP_PROTOCOL_ERROR);
}

void CRelay::FlushSendBuffer(CClient &Client)
{
	if(Client.IsDropping())
		return;

	const relay::SStreamWrite Write = relay::Send(Client.m_Socket, Client.m_SendBuffer);
	if(Write.Closed)
		Client.RequestDrop(DROP_REMOTE_CLOSED);
}

void CRelay::Queue(CClient &Client, relay::EMsg Type, const std::string &Payload)
{
	if(Client.IsDropping())
		return;

	if(!relay::BuildFrame(Type, Payload, Client.m_SendBuffer))
	{
		Client.RequestDrop(DROP_MESSAGE_TOO_BIG);
		return;
	}

	if((int)Client.m_SendBuffer.size() > m_Config.m_MaxSendBuffer)
	{
		log_warn("relay", "server %d does not keep up with the traffic, dropping it", Client.m_ServerID);
		Client.RequestDrop(DROP_SEND_BUFFER_OVERFLOW);
		return;
	}

	// Sending right away keeps the latency low; a partial write is finished by
	// the next FlushSendBuffer() of Update().
	FlushSendBuffer(Client);
}

void CRelay::Reject(CClient &Client, EDropReason Reason)
{
	Queue(Client, relay::EMsg::REJECTED, relay::BuildJson({{"reason", DropReasonName(Reason)}}));
	Client.RequestDrop(Reason);
}

void CRelay::Broadcast(const CClient &From, relay::EMsg Type, const std::string &Payload)
{
	for(CClient *pClient : m_vpClients)
	{
		if(pClient != &From && pClient->Online())
			Queue(*pClient, Type, Payload);
	}
}

void CRelay::HandleFrame(CClient &Client, relay::EMsg Type, const std::string &Payload)
{
	switch(Type)
	{
	case relay::EMsg::HELLO:
		HandleHello(Client, Payload);
		return;
	case relay::EMsg::PING:
		Queue(Client, relay::EMsg::PONG, Payload);
		return;
	case relay::EMsg::PONG:
		// The receive time was updated while reading, nothing else to do.
		return;
	case relay::EMsg::EVENT:
		if(!Client.m_HandshakeDone)
		{
			Reject(Client, DROP_HANDSHAKE_REQUIRED);
			return;
		}
		// The payload is forwarded unchanged: the relay does not look into it.
		Broadcast(Client, relay::EMsg::EVENT, Payload);
		return;
	case relay::EMsg::LOCALIZATION_HASH:
		HandleLocalizationHash(Client, Payload);
		return;
	case relay::EMsg::LOCALIZATION_REQUEST:
	case relay::EMsg::LOCALIZATION_DATA:
		// TODO(localization): compare the collected hashes, ask one server for
		// the newest file and send it to the servers whose hash differs.
		log_warn("relay", "%s is not implemented yet", relay::MsgName(Type));
		return;
	case relay::EMsg::WELCOME:
	case relay::EMsg::REJECTED:
		Reject(Client, DROP_UNEXPECTED_MESSAGE);
		return;
	}
}

void CRelay::HandleLocalizationHash(const CClient &Client, const std::string &Payload)
{
	// Groundwork only: the hashes are collected here so that the comparison and
	// the distribution can be added without touching the protocol.
	if(!Client.m_HandshakeDone)
		return;

	nlohmann::json Json;
	if(!relay::ParseJson(Payload, Json))
		return;

	const std::string Language = relay::GetCleanString(Json, "language", relay::MAX_LANGUAGE_LENGTH);
	const std::string Hash = relay::GetCleanString(Json, "hash", 64);
	if(Language.empty() || Hash.empty())
		return;

	m_LocalizationHashes[Language][Client.m_ServerID] = Hash;
	log_info("relay", "localization hash of server %d: language '%s' hash %s", Client.m_ServerID, Language.c_str(), Hash.c_str());
}

void CRelay::HandleHello(CClient &Client, const std::string &Payload)
{
	if(Client.m_HandshakeDone)
	{
		Client.RequestDrop(DROP_DOUBLE_HANDSHAKE);
		return;
	}

	nlohmann::json Json;
	if(!relay::ParseJson(Payload, Json))
	{
		Reject(Client, DROP_MALFORMED_HELLO);
		return;
	}

	if(relay::GetInt(Json, "protocol") != relay::PROTOCOL_VERSION)
	{
		Reject(Client, DROP_PROTOCOL_MISMATCH);
		return;
	}

	if(!PasswordMatches(relay::GetString(Json, "password")))
	{
		Reject(Client, DROP_WRONG_PASSWORD);
		return;
	}

	const int ServerID = relay::GetInt(Json, "server_id");
	if(ServerID <= 0)
	{
		Reject(Client, DROP_INVALID_SERVER_ID);
		return;
	}

	if(FindClientByServerID(ServerID))
	{
		Reject(Client, DROP_SERVER_ID_TAKEN);
		return;
	}

	Client.m_ServerID = ServerID;
	Client.m_Name = relay::GetCleanString(Json, "name", relay::MAX_NAME_LENGTH);
	Client.m_HandshakeDone = true;

	log_info("relay", "server %d '%s' (%s) connected, %d server(s) online", ServerID, Client.m_Name.c_str(),
		Client.m_AddrStr.c_str(), ClientCount());
	SendWelcome(Client);
}

void CRelay::SendWelcome(CClient &Client)
{
	nlohmann::json Peers = nlohmann::json::array();
	for(const CClient *pOther : m_vpClients)
	{
		if(pOther != &Client && pOther->m_HandshakeDone)
			Peers.push_back(pOther->m_ServerID);
	}

	const nlohmann::json Welcome = {
		{"server_id", Client.m_ServerID},
		{"relay", m_Config.m_Name},
		{"peers", std::move(Peers)},
	};
	Queue(Client, relay::EMsg::WELCOME, relay::BuildJson(Welcome));
}
