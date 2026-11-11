/*
 * Test client for the cross-server synchronization, used by
 * scripts/test_server_sync.py. It is a small program around the real
 * CServerSync, so the test covers the game server side of the protocol and not
 * only the relay.
 *
 * Usage:
 *   test_server_sync listen <port> <server_id> <seconds>
 *   test_server_sync push <port> <server_id> <event> [value]
 *
 * "listen" prints every event it receives until the given time has passed,
 * "push" connects, waits for the handshake and sends one event.
 * The exit code is 0 when the connection was established.
 */
#include <base/logger.h>
#include <base/system.h>

#include <engine/config.h>
#include <engine/shared/config.h>

#include <engine/server/server_sync.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

namespace
{
	void Tick(int Times)
	{
		for(int i = 0; i < Times; i++)
		{
			Sync->OnTick();
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}

	void Configure(int Port, int ServerID)
	{
		str_copy(g_Config.m_SvSyncRelayHost, "127.0.0.1");
		g_Config.m_SvSyncRelayPort = Port;
		g_Config.m_SvSyncServerID = ServerID;
		str_copy(g_Config.m_SvName, "test-server");
		g_Config.m_SvSyncRelayReconnectMs = 200;
	}

	bool WaitConnected(int Milliseconds)
	{
		for(int i = 0; i < Milliseconds / 10; i++)
		{
			Sync->OnTick();
			if(Sync->IsConnected())
				return true;
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		return Sync->IsConnected();
	}

	int Listen(int Port, int ServerID, int Seconds)
	{
		Configure(Port, ServerID);
		Sync->SetEventCallback([](const std::string& EventName, const CServerSync::SRequest& Req) {
			printf("EVENT %s server=%d world=%d value=%d\n", EventName.c_str(), Req.ServerID, Req.WorldID, Req.Value);
			fflush(stdout);
		});

		if(!WaitConnected(5000))
		{
			printf("NOT CONNECTED\n");
			fflush(stdout);
			return 1;
		}

		printf("CONNECTED\n");
		fflush(stdout);

		Tick(Seconds * 100);
		return 0;
	}

	int Push(int Port, int ServerID, const char* pEvent, int Value)
	{
		Configure(Port, ServerID);

		// Pushed before the connection exists: the message has to wait in the
		// queue and be delivered after the handshake.
		const int ID = Sync->Push(pEvent, 0, Value);
		printf("QUEUED id=%d pending=%d\n", ID, (int)Sync->PendingCount());
		fflush(stdout);

		if(!WaitConnected(5000))
		{
			printf("NOT CONNECTED pending=%d\n", (int)Sync->PendingCount());
			fflush(stdout);
			return 1;
		}

		printf("CONNECTED\n");
		fflush(stdout);
		Tick(50);
		// Everything queued has to be on the wire by now.
		printf("DONE pending=%d\n", (int)Sync->PendingCount());
		fflush(stdout);
		return 0;
	}
}

int main(int argc, const char** argv) // ignore_convention
{
	log_set_global_logger_default();
	net_init();
	secure_random_init();

	if(argc < 4)
	{
		printf("usage: %s listen <port> <server_id> <seconds>\n", argv[0]);
		printf("       %s push <port> <server_id> <event> [value]\n", argv[0]);
		return 2;
	}

	const char* pMode = argv[1];
	const int Port = atoi(argv[2]);
	const int ServerID = atoi(argv[3]);

	int Result;
	if(str_comp(pMode, "listen") == 0 && argc >= 5)
		Result = Listen(Port, ServerID, atoi(argv[4]));
	else if(str_comp(pMode, "push") == 0 && argc >= 5)
		Result = Push(Port, ServerID, argv[4], argc >= 6 ? atoi(argv[5]) : 0);
	else
	{
		printf("unknown mode\n");
		return 2;
	}

	secure_random_uninit();
	return Result;
}
