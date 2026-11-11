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
			const int Bytes = net_tcp_recv(Socket, aChunk, (int)sizeof(aChunk));
			if(Bytes > 0)
			{
				Reader.Append(aChunk, (size_t)Bytes);
				Result.Bytes += (size_t)Bytes;
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

			// Unreachable peer. Only a would-block is not an error and is retried
			// by the next tick of the caller.
			if(Bytes == 0 || net_would_block())
				return Result;

			// On Linux and Windows a failed send already means closed.
			Result.Closed = true;
			return Result;
		}

		return Result;
	}
}
