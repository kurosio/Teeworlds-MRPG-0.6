#ifndef ENGINE_RELAY_RELAY_STREAM_H
#define ENGINE_RELAY_RELAY_STREAM_H

#include <base/system.h>

#include <cstddef>
#include <cstdint>
#include <string>

#include "relay_protocol.h"

/*
 * Non-blocking TCP plumbing for the frame protocol of relay_protocol.h.
 *
 * Both programs speak the same stream: the relay for every game server, the
 * game server for its single connection to the relay. This file is the only
 * place that knows the error semantics of net_tcp_recv()/net_tcp_send()
 * (0 = closed, -1 = failed or would block), so a caller only has to look at the
 * returned result.
 *
 * Nothing here ever blocks or retries: a pass moves what is available and
 * returns, the caller comes back on its next tick.
 */
namespace relay
{
	// Milliseconds of the monotonic clock of the engine. Both sides measure their
	// timeouts with it.
	int64_t NowMs();

	// Result of one read pass.
	struct SStreamRead
	{
		// Bytes appended to the frame reader.
		size_t Bytes = 0;
		// The peer closed the connection in an orderly way.
		bool Closed = false;
		// The socket failed; the connection has to be closed.
		bool Error = false;

		bool Ok() const { return !Closed && !Error; }
	};

	// Appends every byte that is available right now to `Reader`.
	SStreamRead Receive(NETSOCKET Socket, CFrameReader &Reader);

	// Result of one write pass.
	struct SStreamWrite
	{
		// Bytes removed from the front of the buffer.
		size_t Bytes = 0;
		bool Closed = false;
		bool Error = false;

		bool Ok() const { return !Closed && !Error; }
	};

	// Writes as much of `Buffer` as the socket takes and removes it. `Bytes` is
	// 0 when the socket would block, which is not an error: try again later.
	SStreamWrite Send(NETSOCKET Socket, std::string &Buffer);
}

#endif // ENGINE_RELAY_RELAY_STREAM_H
