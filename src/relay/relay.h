#ifndef ENGINE_RELAY_RELAY_H
#define ENGINE_RELAY_RELAY_H

#include <base/system.h>

#include <engine/shared/config.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "relay_protocol.h"

// Why a connection goes away. DROP_NONE means "keep it": the client is fine and
// the reason is never reported.
enum EDropReason
{
	DROP_NONE = 0,
	// The peer vanished or closed the connection. Expected, not logged.
	DROP_REMOTE_CLOSED,
	DROP_IDLE_TIMEOUT,
	// Everything below is a defect of a peer and is logged with its text.
	DROP_CONNECTION_FAILED,
	DROP_PROTOCOL_ERROR,
	DROP_DOUBLE_HANDSHAKE,
	DROP_HANDSHAKE_REQUIRED,
	DROP_UNEXPECTED_MESSAGE,
	DROP_MESSAGE_TOO_BIG,
	DROP_SEND_BUFFER_OVERFLOW,
	// The handshake failed. The text is also sent to the game server.
	DROP_WRONG_PASSWORD,
	DROP_PROTOCOL_MISMATCH,
	DROP_MALFORMED_HELLO,
	DROP_INVALID_SERVER_ID,
	DROP_SERVER_ID_TAKEN,
	DROP_RELAY_FULL,
	DROP_COUNT,
};

/*
 * Address the relay listens on, from the `bindaddr` and `relay_port` settings.
 * An empty `pBindAddr` means every interface of both families; otherwise any
 * name or literal address of one family is accepted, `::1` as well as `[::1]`.
 * Returns false when the address cannot be resolved.
 */
bool ParseBindAddr(const char *pBindAddr, int Port, NETADDR *pOut);

/*
 * The console commands the relay accepts. Its own settings (relay_port and
 * friends) are console settings, but the log file is a setting of the game that
 * the engine defines for client and server; without those flags `logfile` and
 * `logappend` in relay.cfg are refused with "No such command" and the relay
 * cannot be given a log to write to.
 */
constexpr int CONSOLE_FLAGS = CFGFLAG_MASTER | CFGFLAG_CLIENT | CFGFLAG_SERVER;

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
 * asks to be dropped (RequestDrop()) and Update() performs the removal. The
 * reason of the drop is kept as a small value instead of a string, so the log
 * line and the REJECTED frame cannot drift apart and "do not log this" does not
 * mean comparing strings.
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
		EDropReason m_DropReason = DROP_NONE;
		int64_t m_LastRecvTime = 0;
		int m_ServerID = 0;
		std::string m_Name;
		bool m_HandshakeDone = false;

		bool Online() const { return m_HandshakeDone && !IsDropping(); }
		bool IsDropping() const { return m_DropReason != DROP_NONE; }

		void RequestDrop(EDropReason Reason)
		{
			if(m_DropReason == DROP_NONE)
				m_DropReason = Reason;
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
	// Answers a failed handshake with a REJECTED frame and drops the client.
	void Reject(CClient &Client, EDropReason Reason);
	// Refuses an accepted socket that never became a client.
	static void SendRejection(NETSOCKET Socket, EDropReason Reason);
	// Text of the reason for the log line and the REJECTED frame.
	static const char *DropReasonName(EDropReason Reason);

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
