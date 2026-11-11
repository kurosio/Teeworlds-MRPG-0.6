#include "relay_stream.h"

namespace
{
	// Reads until the socket refuses to give more or reports a failure.
	constexpr size_t CHUNK_SIZE = 4096;
}

namespace relay
{
	int64_t NowMs()
	{
		const int64_t Freq = time_freq();
		return Freq > 0 ? time_get() * 1000 / Freq : 0;
	}

	SStreamRead Receive(NETSOCKET Socket, CFrameReader &Reader)
	{
		SStreamRead Result;
		unsigned char aChunk[CHUNK_SIZE];

		while(true)
		{
			// The socket is non-blocking, so this returns as soon as the kernel
			// has nothing more for us.
			const int Bytes = net_tcp_recv(Socket, aChunk, (int)sizeof(aChunk));
			if(Bytes > 0)
			{
				Reader.Append(aChunk, (size_t)Bytes);
				Result.Bytes += (size_t)Bytes;
				// A broken frame cannot be skipped, so there is nothing left to
				// read from this connection.
				if(Reader.Error())
					return Result;
				continue;
			}

			if(Bytes == 0)
				Result.Closed = true;
			else if(!net_would_block())
				Result.Error = true;
			return Result;
		}
	}

	SStreamWrite Send(NETSOCKET Socket, std::string &Buffer)
	{
		SStreamWrite Result;

		while(!Buffer.empty())
		{
			const int Bytes = net_tcp_send(Socket, Buffer.data(), (int)Buffer.size());
			if(Bytes > 0)
			{
				Buffer.erase(0, (size_t)Bytes);
				Result.Bytes += (size_t)Bytes;
				continue;
			}

			// Would block: the rest is written by the next tick of the caller.
			// (A send of 0 bytes means the same.)
			if(Bytes == 0 || net_would_block())
				return Result;

			// Any other failure means the peer is gone: a reset, a closed socket
			// or an unreachable host. There is nothing left to retry.
			Result.Closed = true;
			return Result;
		}

		return Result;
	}
}
