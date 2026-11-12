#ifndef ENGINE_SERVER_SERVER_SYNC_H
#define ENGINE_SERVER_SERVER_SYNC_H

#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include <relay/relay_protocol.h>

#include <teeother/tools/nlohmann_json.h>

/*
 * Cross-server synchronization: the game server side of the relay.
 *
 * Every game server keeps one outgoing TCP connection to the relay
 * (mmoteeworlds_relay, see src/relay). An event pushed on one server is forwarded
 * by the relay to all the other servers, which load the data themselves (from
 * their database, by the ID in the event).
 *
 * The connection never blocks the game thread: OnTick() moves bytes in and out
 * of a non-blocking socket and returns immediately. Messages survive a broken or
 * missing connection, they are sent as soon as it exists.
 *
 * Typical use, from a game component:
 *   Sync->Push(syncevents::GUILD_CREATE, GS()->GetWorldID(), GuildID);
 *
 * Configured with sv_sync_server_id, sv_sync_relay_host, sv_sync_relay_port and
 * sv_sync_relay_password. Without a host or with the ID 0 the synchronization is
 * disabled and Push() returns 0.
 */
/*
 * Names of the events the game sends through the relay. They are the shared
 * vocabulary between the servers: the sender and the OnSyncEvent() handler of the
 * receivers have to use the same string.
 */
namespace syncevents
{
	inline constexpr const char *GUILD_CREATE = "guild_create";
	inline constexpr const char *GUILD_DISBAND = "guild_disband";
	inline constexpr const char *GUILD_UPDATE = "guild_update";
	inline constexpr const char *AUCTION_SLOT_CREATE = "auction_slot_create";
	inline constexpr const char *AUCTION_SLOT_REMOVE = "auction_slot_remove";
	inline constexpr const char *ACCOUNT_SESSION_TAKEOVER = "account_session_takeover";
	inline constexpr const char *ACCOUNT_SESSION_TAKEOVER_ACK_PREFIX = "account_session_takeover_ack:";
}

class CServerSync
{
	// Bounded memory: the queue of unsent messages, the cache of handled message
	// IDs of the other servers, and the accepted length of an event name.
	static constexpr size_t MAX_PENDING = 10000;
	static constexpr size_t MAX_PROCESSED_CACHE = 8192;
	static constexpr int MAX_EVENT_NAME_LENGTH = 64;

public:
	// One event of another server, already validated by the sync.
	struct SRequest
	{
		int ID {};
		int ServerID {};
		int WorldID {};
		std::string Event {};
		int Value {};
	};

	using EventCallback = std::function<void(const std::string& EventName, const SRequest&)>;

	// Groundwork for the localization distribution (see relay_protocol.h): the
	// game provides the SHA-256 of every language file, the sync announces them
	// to the relay after the handshake. Without a provider nothing is announced.
	using LocalizationHashProvider = std::function<std::vector<std::pair<std::string, std::string>>()>;

	static CServerSync* GetInstance()
	{
		static CServerSync s_Instance;
		return &s_Instance;
	}

	CServerSync(const CServerSync&) = delete;
	CServerSync& operator=(const CServerSync&) = delete;

	// Called with every event of another server, on the game thread.
	void SetEventCallback(EventCallback Fn) { m_EventCallback = std::move(Fn); }
	void SetLocalizationHashProvider(LocalizationHashProvider Fn) { m_LocalizationHashProvider = std::move(Fn); }

	// Queues an event for the other game servers and returns its ID, or 0 when
	// the event was not queued (disabled, invalid name or the queue is full).
	// Without a relay connection the event is kept until the reconnection.
	int Push(const char* pEvent, int WorldID, int Value);

	// Connects, sends and receives the pending messages once.
	void OnTick();

	bool IsConnected() const { return m_State == EState::ONLINE; }

	// Number of messages waiting for a working connection.
	size_t PendingCount() const { return m_Pending.size(); }

private:
	enum class EState
	{
		OFFLINE,
		CONNECTING,
		ONLINE,
	};

	// A message that has not completely reached the relay yet.
	struct SPendingMessage
	{
		int ID {};
		relay::EMsg Type {};
		std::string Payload {};
	};

	CServerSync() = default;
	~CServerSync();

	bool Enabled() const;
	static bool IsValidEventName(const char* pEvent);

	// Connection lifecycle: one of them ends in Disconnect().
	void UpdateConnection();
	void EnsureConnection();
	void CheckTimeouts();
	void Disconnect(const char* pReason);
	void ReadSocket();
	void FlushSocket();

	// Frames of the relay.
	void HandleFrame(relay::EMsg Type, const std::string& Payload);
	void HandleWelcome(const std::string& Payload);
	void HandleRejected(const std::string& Payload);
	void HandleEvent(const std::string& Payload);
	void AnnounceLocalizationHashes();

	// Outgoing messages and the deduplication of the incoming ones.
	void Queue(relay::EMsg Type, std::string Payload, int ID = 0);
	void QueueJson(relay::EMsg Type, const nlohmann::json& Json);
	int NextMessageID();
	// Returns true when the message was already handled. Otherwise it is added
	// to the bounded cache.
	bool IsDuplicate(int RequestID);

	EventCallback m_EventCallback;
	LocalizationHashProvider m_LocalizationHashProvider;

	EState m_State = EState::OFFLINE;
	NETSOCKET m_Socket = nullptr;
	relay::CFrameReader m_Reader;

	// Outgoing: m_Pending is the queue, the front m_InFlight messages are already
	// copied into m_SendBuffer. They are only removed from the queue once the
	// buffer is on the wire, so a failed connection keeps the events.
	std::deque<SPendingMessage> m_Pending;
	std::string m_SendBuffer;
	size_t m_InFlight = 0;

	// Timers of the connection, all in milliseconds of relay::NowMs().
	int64_t m_LastConnectAttempt = 0;
	int64_t m_ConnectStartTime = 0;
	int64_t m_LastRecvTime = 0;
	int64_t m_LastKeepalive = 0;

	int m_NextID = 1;

	// Set when the relay rejected the handshake, to log the reason only once.
	std::string m_LastRejectReason;
	// Last logged disconnect reason, to keep a down relay from flooding the log.
	std::string m_LastDisconnectReason;
	int64_t m_LastDisconnectLogTime = 0;

	// IDs of the handled messages of the other servers, oldest first.
	std::deque<int> m_ProcessedOrder;
};

// The game has exactly one connection: Sync->Push(...), Sync->OnTick().
#define Sync CServerSync::GetInstance()

#endif // ENGINE_SERVER_SERVER_SYNC_H
