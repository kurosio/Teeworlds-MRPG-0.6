#ifndef ENGINE_RELAY_RELAY_PROTOCOL_H
#define ENGINE_RELAY_RELAY_PROTOCOL_H

#include <cstdint>
#include <cstring>
#include <string>

/*
 * Cross-server synchronization protocol.
 *
 * This file is shared by two programs:
 *  - the relay (src/relay), a standalone executable that only forwards messages;
 *  - the game server (src/engine/server/server_sync.cpp), which keeps one
 *    outgoing connection to the relay.
 *
 * A connection is a stream of frames:
 *
 *   +--------------------+---------------+---------------------------+
 *   | length: uint32 BE  | type: uint8   | payload: length - 1 bytes |
 *   +--------------------+---------------+---------------------------+
 *
 * `length` counts the type byte and the payload, so the smallest frame has
 * length 1 and no payload. Payloads are UTF-8 JSON; everything a peer reads is
 * validated here with the helpers below, because the relay is reachable by
 * anyone who knows the port.
 *
 * The relay never parses EVENT payloads: it forwards them unchanged.
 */

namespace relay
{

	enum : int
	{
		PROTOCOL_VERSION = 1,

		// 4 bytes of length prefix in front of every frame.
		FRAME_HEADER_SIZE = 4,

		// Refuse anything bigger; a localization file is far below this.
		FRAME_MAX_LENGTH = 8 * 1024 * 1024,

		// How often a connection sends PING when nothing else is going on.
		KEEPALIVE_INTERVAL_MS = 15000,

		// A connection that sent nothing for this long is dropped by the relay,
		// and a server drops a relay connection that stays silent for this long.
		CONNECTION_TIMEOUT_MS = 60000,

		// The relay drops a connection that did not finish the HELLO handshake.
		HANDSHAKE_TIMEOUT_MS = 10000,

		// Further messages are not queued for a client that does not read them;
		// such a client is dropped instead of growing the memory of the relay.
		MAX_SEND_BUFFER = 4 * 1024 * 1024,

		// Longest server name kept from a HELLO, and longest language tag kept
		// from a LOCALIZATION_HASH. Both end up in the log of the relay.
		MAX_NAME_LENGTH = 64,
		MAX_LANGUAGE_LENGTH = 16,
	};

	enum class EMsg : unsigned char
	{
		// Server -> relay, first frame of every connection.
		// {"protocol": 1, "server_id": 1, "password": "...", "name": "..."}
		HELLO = 1,
		// Relay -> server, the handshake succeeded.
		// {"server_id": 1, "relay": "...", "peers": [1, 2]}
		WELCOME = 2,
		// Relay -> server, the handshake failed and the connection is closed.
		// {"reason": "..."}
		REJECTED = 3,
		// Server <-> relay, forwarded unchanged to every other server.
		// {"id": 17, "event": "guild_create", "server": 1, "world": 0,
		//  "value": 12}
		// The payload carries the ID of the changed object only, never a copy of
		// it: the receiving server loads the object from its own database.
		EVENT = 4,
		// {"t": <milliseconds>}
		PING = 5,
		PONG = 6,

		/*
		 * Localization groundwork. Not used by the game code yet: the relay
		 * stores the hashes, but no file is requested or sent.
		 *
		 * Planned flow (TODO, see docs/server_sync.md):
		 *  1. every server sends LOCALIZATION_HASH per language file once it is
		 *     connected: {"language": "ru", "hash": "<sha256>"};
		 *  2. the relay compares the hashes and picks the newest file as the
		 *     source;
		 *  3. LOCALIZATION_REQUEST asks the source server to upload the file; the
		 *     source answers with LOCALIZATION_DATA. The payload is
		 *     {"language": "ru"} and a raw .txt file respectively;
		 *  4. the relay sends LOCALIZATION_DATA to every server whose hash
		 *     differs, and those servers replace their local file.
		 */
		LOCALIZATION_HASH = 16,
		LOCALIZATION_REQUEST = 17,
		LOCALIZATION_DATA = 18,
	};

	/*
	 * One entry per message type. The list is the single source of truth for
	 * the name and for the validity of a type byte, so a message cannot be
	 * added to EMsg without its name.
	 */
	struct SMsgInfo
	{
		EMsg Type;
		const char *pName;
	};

	inline constexpr SMsgInfo MSG_INFOS[] = {
		{EMsg::HELLO, "HELLO"},
		{EMsg::WELCOME, "WELCOME"},
		{EMsg::REJECTED, "REJECTED"},
		{EMsg::EVENT, "EVENT"},
		{EMsg::PING, "PING"},
		{EMsg::PONG, "PONG"},
		{EMsg::LOCALIZATION_HASH, "LOCALIZATION_HASH"},
		{EMsg::LOCALIZATION_REQUEST, "LOCALIZATION_REQUEST"},
		{EMsg::LOCALIZATION_DATA, "LOCALIZATION_DATA"},
	};

	inline bool IsKnownMsg(unsigned char Value)
	{
		for(const SMsgInfo &Info : MSG_INFOS)
		{
			if((unsigned char)Info.Type == Value)
				return true;
		}
		return false;
	}

	// "UNKNOWN" for a type that is not part of the protocol.
	inline const char *MsgName(EMsg Msg)
	{
		for(const SMsgInfo &Info : MSG_INFOS)
		{
			if(Info.Type == Msg)
				return Info.pName;
		}
		return "UNKNOWN";
	}

	// Appends one frame to `Out`. Returns false for an oversized payload, which
	// is the only thing a caller can get wrong: the type is an EMsg.
	inline bool BuildFrame(EMsg Type, const std::string &Payload, std::string &Out)
	{
		const size_t Length = Payload.size() + 1;
		if(Length > (size_t)FRAME_MAX_LENGTH)
			return false;

		const unsigned char aHeader[FRAME_HEADER_SIZE] = {
			(unsigned char)((Length >> 24) & 0xff),
			(unsigned char)((Length >> 16) & 0xff),
			(unsigned char)((Length >> 8) & 0xff),
			(unsigned char)(Length & 0xff),
		};
		for(const unsigned char Byte : aHeader)
			Out.push_back((char)Byte);

		Out.push_back((char)Type);
		Out.append(Payload);
		return true;
	}

	/*
	 * Splits the incoming byte stream into frames.
	 *
	 * The buffer is compacted by the next Append() instead of by every Next(),
	 * so draining a full batch of frames does not move the remainder once per
	 * frame. Any length or type that cannot be valid sets Error() and keeps it
	 * set until Reset(): the caller must close the connection.
	 */
	class CFrameReader
	{
	public:
		void Append(const void *pData, size_t Size)
		{
			if(m_Error)
				return;

			if(m_Offset > 0)
			{
				m_Buffer.erase(0, m_Offset);
				m_Offset = 0;
			}

			m_Buffer.append((const char *)pData, Size);
			// A single frame can never exceed FRAME_MAX_LENGTH, so a buffer that
			// grows past that plus one header holds corrupt data.
			if(m_Buffer.size() > (size_t)FRAME_MAX_LENGTH + FRAME_HEADER_SIZE)
				m_Error = true;
		}

		// Returns true and fills `Type`/`Payload` when a whole frame is buffered.
		bool Next(EMsg *pType, std::string *pPayload)
		{
			const size_t FrameSize = NextFrameSize();
			if(FrameSize == 0)
				return false;

			// The frame is the type byte and the payload of `Length - 1` bytes.
			const size_t Offset = m_Offset + FRAME_HEADER_SIZE;
			const size_t Length = FrameSize - FRAME_HEADER_SIZE;
			if(pType)
				*pType = static_cast<EMsg>((unsigned char)m_Buffer[Offset]);
			if(pPayload)
				pPayload->assign(m_Buffer, Offset + 1, Length - 1);

			m_Offset += FrameSize;
			return true;
		}

		bool Error() const { return m_Error; }

		void Reset()
		{
			m_Buffer.clear();
			m_Offset = 0;
			m_Error = false;
		}

	private:
		// Size of the whole frame at the read offset in bytes, 0 while it is
		// incomplete. A length or a type that cannot be valid sets the error.
		size_t NextFrameSize()
		{
			if(m_Error || m_Buffer.size() - m_Offset < (size_t)FRAME_HEADER_SIZE)
				return 0;

			const auto *pHeader = (const unsigned char *)m_Buffer.data() + m_Offset;
			const size_t Length = ((size_t)pHeader[0] << 24) | ((size_t)pHeader[1] << 16) |
				((size_t)pHeader[2] << 8) | (size_t)pHeader[3];
			if(Length < 1 || Length > (size_t)FRAME_MAX_LENGTH)
			{
				m_Error = true;
				return 0;
			}

			// Not everything of this frame has arrived yet.
			if(m_Buffer.size() - m_Offset < (size_t)FRAME_HEADER_SIZE + Length)
				return 0;

			if(!IsKnownMsg((unsigned char)m_Buffer[m_Offset + FRAME_HEADER_SIZE]))
			{
				m_Error = true;
				return 0;
			}

			return (size_t)FRAME_HEADER_SIZE + Length;
		}

		std::string m_Buffer;
		size_t m_Offset = 0;
		bool m_Error = false;
	};

} // namespace relay

#endif // ENGINE_RELAY_RELAY_PROTOCOL_H
