#include "relay.h"

#include <base/logger.h>

#include "relay_json.h"
#include "relay_stream.h"

#include <algorithm>
#include <utility>

namespace
{
	char *AddrToString(const NETADDR &Addr, char *pBuffer, int BufferSize)
	{
		// net_addr_str() does not know the BINDTYPE bitsets that the bind
		// address uses; they are all IPv4 with an empty host.
		NETADDR Printable = Addr;
		if(Printable.type != NETTYPE_IPV4 && Printable.type != NETTYPE_IPV6)
			Printable.type = NETTYPE_IPV4;

		net_addr_str(&Printable, pBuffer, BufferSize, true);
		return pBuffer;
	}
}

CRelay::~CRelay()
{
	Close();
}

int CRelay::ClientCount() const
{
	return (int)std::ranges::count_if(m_vpClients, [](const CClient *pClient) { return pClient->m_HandshakeDone; });
}

bool CRelay::PasswordMatches(const std::string &Password) const
{
	// No configured password: nothing to check. Otherwise compare the whole
	// buffer, so the duration of the check does not tell how much of a guessed
	// password was correct.
	if(m_Config.m_Password.empty())
		return true;

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
	char aBindAddr[NETADDR_MAXSTRSIZE];
	AddrToString(m_Config.m_BindAddr, aBindAddr, sizeof(aBindAddr));

	m_ListenSocket = net_tcp_create(m_Config.m_BindAddr);
	if(!m_ListenSocket)
	{
		log_error("relay", "could not create the listening socket on %s (check relay_port and bindaddr)", aBindAddr);
		return false;
	}

	if(net_tcp_listen(m_ListenSocket, m_Config.m_MaxClients) != 0)
	{
		log_error("relay", "could not listen on %s: is another relay running with the same port?", aBindAddr);
		net_tcp_close(m_ListenSocket);
		m_ListenSocket = nullptr;
		return false;
	}

	net_set_non_blocking(m_ListenSocket);

	// The families that really ended up bound: a host without IPv6 listens on
	// IPv4 only, and the log should say so.
	char aAddr[NETADDR_MAXSTRSIZE];
	NETADDR Bound = m_Config.m_BindAddr;
	const int SocketType = net_socket_type(m_ListenSocket);
	Bound.type = (SocketType & NETTYPE_IPV6) ? NETTYPE_IPV6 : NETTYPE_IPV4;
	log_info("relay", "listening on %s, max clients: %d", AddrToString(Bound, aAddr, sizeof(aAddr)), m_Config.m_MaxClients);
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

	// A snapshot of the client pointers: frames are handled before anything is
	// removed, and the drop itself is deferred to the end of the update.
	std::vector<CClient *> vpActive = m_vpClients;
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
	}

	for(CClient *pClient : vpActive)
	{
		if(pClient->IsDropping())
			continue;

		const int64_t Timeout = pClient->m_HandshakeDone ? (int64_t)relay::CONNECTION_TIMEOUT_MS : (int64_t)relay::HANDSHAKE_TIMEOUT_MS;
		if(Now - pClient->m_LastRecvTime > Timeout)
			pClient->RequestDrop("idle timeout");
	}

	for(CClient *pClient : vpActive)
	{
		if(pClient->IsDropping())
			DropClient(pClient);
	}
}

void CRelay::SendRejection(NETSOCKET Socket, const char *pReason)
{
	// Best effort: the socket is closed right away and whatever was written stays
	// in the buffer of the kernel.
	std::string Frame;
	if(relay::BuildFrame(relay::EMsg::REJECTED, relay::BuildJson({{"reason", pReason}}), Frame))
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

		if((int)m_vpClients.size() >= m_Config.m_MaxClients)
		{
			// Tell the server why instead of closing the socket silently.
			SendRejection(Socket, "the relay is full");
			log_warn("relay", "connection refused: the client limit of %d is reached", m_Config.m_MaxClients);
			continue;
		}

		net_set_non_blocking(Socket);

		auto *pClient = new CClient;
		pClient->m_Socket = Socket;
		pClient->m_LastRecvTime = relay::NowMs();

		char aAddr[NETADDR_MAXSTRSIZE];
		pClient->m_AddrStr = AddrToString(Addr, aAddr, sizeof(aAddr));

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

	// A peer that simply vanished only shows up as a timeout, which is expected
	// and not worth a log line of its own.
	if(pClient->m_DropReason != "idle timeout")
	{
		if(pClient->m_ServerID > 0)
			log_info("relay", "server %d '%s' (%s) disconnected: %s", pClient->m_ServerID, pClient->m_Name.c_str(),
				pClient->m_AddrStr.c_str(), pClient->m_DropReason.c_str());
		else
			log_info("relay", "connection %s closed: %s", pClient->m_AddrStr.c_str(), pClient->m_DropReason.c_str());
	}

	if(pClient->m_Socket)
	{
		// Deliver what is still queued (a REJECTED reason, usually). Nothing is
		// removed here, so this may run while a frame is being handled.
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
		Client.RequestDrop("protocol error");
		return;
	}

	if(Read.Closed)
	{
		Client.RequestDrop("remote end closed the connection");
		return;
	}

	if(Read.Error)
	{
		Client.RequestDrop("connection failure");
		return;
	}

	// Every complete frame of the new bytes.
	relay::EMsg Type;
	std::string Payload;
	while(!Client.IsDropping() && Client.m_Reader.Next(&Type, &Payload))
		HandleFrame(Client, Type, Payload);

	if(Client.m_Reader.Error())
		Client.RequestDrop("protocol error");
}

void CRelay::FlushSendBuffer(CClient &Client)
{
	if(Client.IsDropping())
		return;

	const relay::SStreamWrite Write = relay::Send(Client.m_Socket, Client.m_SendBuffer);
	if(!Write.Ok())
		Client.RequestDrop(Write.Closed ? "the connection is gone" : "failed to send data");
}

void CRelay::Queue(CClient &Client, relay::EMsg Type, const std::string &Payload)
{
	if(Client.IsDropping())
		return;

	if(!relay::BuildFrame(Type, Payload, Client.m_SendBuffer))
	{
		Client.RequestDrop("message too big");
		return;
	}

	if((int)Client.m_SendBuffer.size() > m_Config.m_MaxSendBuffer)
	{
		log_warn("relay", "server %d does not keep up with the traffic, dropping it", Client.m_ServerID);
		Client.RequestDrop("send buffer overflow");
		return;
	}

	// Sending right away keeps the latency low; a partial write is finished by
	// the next FlushSendBuffer() of Update().
	FlushSendBuffer(Client);
}

void CRelay::Reject(CClient &Client, const char *pReason)
{
	Queue(Client, relay::EMsg::REJECTED, relay::BuildJson({{"reason", pReason}}));
	Client.RequestDrop(pReason);
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
			Reject(Client, "handshake required");
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
		Reject(Client, "unexpected message");
		return;
	}
}

void CRelay::HandleLocalizationHash(const CClient &Client, const std::string &Payload)
{
	// Groundwork only: the hashes are collected here so that the comparison and
	// the distribution can be added without touching the protocol.
	nlohmann::json Json;
	if(!Client.m_HandshakeDone || !relay::ParseJson(Payload, Json))
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
		Client.RequestDrop("double handshake");
		return;
	}

	nlohmann::json Json;
	if(!relay::ParseJson(Payload, Json))
	{
		Reject(Client, "malformed hello");
		return;
	}

	if(relay::GetInt(Json, "protocol") != relay::PROTOCOL_VERSION)
	{
		Reject(Client, "protocol version mismatch");
		return;
	}

	if(!PasswordMatches(relay::GetString(Json, "password")))
	{
		Reject(Client, "wrong password");
		return;
	}

	const int ServerID = relay::GetInt(Json, "server_id");
	if(ServerID <= 0)
	{
		Reject(Client, "invalid server id");
		return;
	}

	if(FindClientByServerID(ServerID))
	{
		Reject(Client, "server id already connected");
		return;
	}

	Client.m_ServerID = ServerID;
	Client.m_Name = relay::GetCleanString(Json, "name", relay::MAX_NAME_LENGTH);
	Client.m_HandshakeDone = true;

	log_info("relay", "server %d '%s' (%s) connected, %d server(s) online", ServerID, Client.m_Name.c_str(), Client.m_AddrStr.c_str(), ClientCount());
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

	Queue(Client, relay::EMsg::WELCOME, relay::BuildJson({
						 {"server_id", Client.m_ServerID},
						 {"relay", m_Config.m_Name},
						 {"peers", std::move(Peers)},
					 }));
}
