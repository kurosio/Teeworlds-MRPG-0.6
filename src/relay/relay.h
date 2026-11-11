#ifndef ENGINE_RELAY_RELAY_H
#define ENGINE_RELAY_RELAY_H

#include <base/system.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "relay_protocol.h"

class CRelayConfig
{
public:
	NETADDR m_BindAddr {};
	int m_MaxClients = 64;
	// Further messages are not queued for a client that stops reading them.
	int m_MaxSendBuffer = relay::MAX_SEND_BUFFER;
	std::string m_Password;
	std::string m_Name = "relay";
};

/*
 * The relay keeps one TCP connection per game server and forwards every EVENT
 * frame to all the other connected servers. It stores no game data and does not
 * interpret event payloads, so a restart only costs the connections.
 *
 * Update() is the whole driver of a single-threaded event loop: accept, read,
 * write, drop. Nothing is deleted while a frame is being handled — a client only
 * asks to be dropped (RequestDrop()) and Update() performs the removal.
 */
class CRelay
{
public:
	CRelay() = default;
	CRelay(const CRelay &) = delete;
	CRelay &operator=(const CRelay &) = delete;
	~CRelay();

	// Opens the listening socket. Returns false and logs the reason on failure.
	bool Open(const CRelayConfig &Config);

	// Accepts, reads, writes and drops dead connections once.
	// The caller sleeps between the calls.
	void Update();

	void Close();

	int ClientCount() const;
	int MaxClients() const { return m_Config.m_MaxClients; }

private:
	struct CClient
	{
		NETSOCKET m_Socket = nullptr;
		std::string m_AddrStr;
		relay::CFrameReader m_Reader;
		std::string m_SendBuffer;
		std::string m_DropReason;
			int64_t m_LastRecvTime = 0;
		int m_ServerID = 0;
		std::string m_Name;
		bool m_HandshakeDone = false;

		bool Online() const { return m_HandshakeDone && !IsDropping(); }
		bool IsDropping() const { return !m_DropReason.empty(); }

		void RequestDrop(const char *pReason)
		{
			if(m_DropReason.empty())
				m_DropReason = pReason;
		}
	};

	void AcceptClients();
	void ReadClient(CClient &Client);
	// Writes what is buffered; requests a drop when the connection is gone.
	void FlushSendBuffer(CClient &Client);
	void DropClient(CClient *pClient);
	void Broadcast(const CClient &From, relay::EMsg Type, const std::string &Payload);
	void HandleFrame(CClient &Client, relay::EMsg Type, const std::string &Payload);
	void HandleHello(CClient &Client, const std::string &Payload);
	void HandleLocalizationHash(const CClient &Client, const std::string &Payload);
	// Queues the answer to a successful handshake.
	void SendWelcome(CClient &Client);
	void Queue(CClient &Client, relay::EMsg Type, const std::string &Payload);
	void Reject(CClient &Client, const char *pReason);
	static void SendRejection(NETSOCKET Socket, const char *pReason);

	bool PasswordMatches(const std::string &Password) const;
	CClient *FindClientByServerID(int ServerID) const;

	CRelayConfig m_Config;
	NETSOCKET m_ListenSocket = nullptr;
	std::vector<CClient *> m_vpClients;
	// Last localization hash reported by every server, per language. Groundwork
	// for the localization distribution, see relay_protocol.h.
	std::map<std::string, std::map<int, std::string>> m_LocalizationHashes;
};

#endif // ENGINE_RELAY_RELAY_H
