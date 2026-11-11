/*
 * MRPG relay: a standalone retransmitter for the cross-server synchronization.
 *
 * Every game server keeps one outgoing connection to this program (see
 * sv_sync_relay_* in the server config). A message produced on one game server
 * is forwarded here to all the other connected game servers, which initialize
 * the data locally from the database by the ID in the event. The relay stores
 * nothing and does not interpret event payloads.
 *
 * Usage:
 *   mmoteeworlds_relay [-f relay.cfg] ["relay_port 8383" "relay_password secret" ...]
 *
 * The console treats every command line argument as a whole config line, so an
 * override with an argument has to be quoted.
 */
#include <base/logger.h>
#include <base/system.h>

#include <engine/config.h>
#include <engine/console.h>
#include <engine/kernel.h>
#include <engine/storage.h>

#include <engine/shared/config.h>

#include "relay.h"

#include <chrono>
#include <csignal>
#include <exception>
#include <thread>

#if defined(CONF_FAMILY_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace
{
	volatile sig_atomic_t g_InterruptSignaled = 0;

	void HandleSigIntTerm(int Param)
	{
		(void)Param;
		g_InterruptSignaled = 1;

		// Exit the next time a signal is received.
		signal(SIGINT, SIG_DFL);
		signal(SIGTERM, SIG_DFL);
	}

	CRelay *g_pRelay = nullptr;

	void ConStatus(IConsole::IResult *pResult, void *pUserData)
	{
		(void)pResult;
		(void)pUserData;
		if(!g_pRelay)
			return;

		log_info("relay", "%d of %d server(s) connected", g_pRelay->ClientCount(), g_pRelay->MaxClients());
	}

	void ConShutdown(IConsole::IResult *pResult, void *pUserData)
	{
		(void)pResult;
		(void)pUserData;
		g_InterruptSignaled = 1;
	}

	bool ResolveBindAddr(const char *pBindAddr, NETADDR *pAddr)
	{
		// net_addr_from_str() reads a literal address and wants IPv6 in brackets.
		// A bare address like `::1` is wrapped here, because net_host_lookup()
		// would split it at the first colon and fail on the empty host.
		char aAddr[NETADDR_MAXSTRSIZE];
		if(pBindAddr[0] != '[' && str_find(pBindAddr, ":") != nullptr)
			str_format(aAddr, sizeof(aAddr), "[%s]", pBindAddr);
		else
			str_copy(aAddr, pBindAddr);

		if(net_addr_from_str(pAddr, aAddr) != 0)
		{
			if(net_host_lookup(pBindAddr, pAddr, NETTYPE_ALL) != 0)
				return false;
		}

		// Only the family of the resolved address makes sense: "::" cannot be
		// combined with IPv4 and "0.0.0.0" cannot be combined with IPv6.
		// net_host_lookup() may also answer with a bitset of families, in which
		// case both are kept.
		if(pAddr->type != NETTYPE_IPV4 && pAddr->type != NETTYPE_IPV6)
			pAddr->type = NETTYPE_ALL;
		return true;
	}

	bool BuildBindAddr(NETADDR *pAddr)
	{
		mem_zero(pAddr, sizeof(*pAddr));
		if(g_Config.m_Bindaddr[0] != '\0' && !ResolveBindAddr(g_Config.m_Bindaddr, pAddr))
		{
			log_error("relay", "the bind address '%s' cannot be resolved", g_Config.m_Bindaddr);
			return false;
		}

		// No bindaddr: all interfaces, both families.
		if(g_Config.m_Bindaddr[0] == '\0')
			pAddr->type = NETTYPE_ALL;

		pAddr->port = (unsigned short)g_Config.m_RelayPort;
		return true;
	}
}

int main(int argc, const char **argv) // ignore_convention
{
	CCmdlineFix CmdlineFix(&argc, &argv);

#if defined(CONF_FAMILY_WINDOWS)
	ShowWindow(GetConsoleWindow(), SW_HIDE);
#endif

	log_set_global_logger_default();
	net_init();

#if !defined(CONF_FAMILY_WINDOWS)
	// A game server may disappear at any moment; writing to its closed socket
	// must not kill the relay.
	signal(SIGPIPE, SIG_IGN);
#endif
	signal(SIGINT, HandleSigIntTerm);
	signal(SIGTERM, HandleSigIntTerm);

	if(secure_random_init() != 0)
	{
		log_error("relay", "could not initialize the secure RNG");
		return -1;
	}

	IKernel *pKernel = IKernel::Create();
	IStorageEngine *pStorage = CreateStorage(IStorageEngine::STORAGETYPE_BASIC, argc, argv);
	auto pConsole = CreateConsole(CFGFLAG_MASTER);
	IConfigManager *pConfigManager = CreateConfigManager();

	// pConsole is owned by the unique_ptr below, so the kernel must not delete it
	// on shutdown as well: that double free crashed the relay on every exit.
	const bool RegisterFail =
		!pKernel->RegisterInterface(pStorage) ||
		!pKernel->RegisterInterface(pConsole.get(), false) ||
		!pKernel->RegisterInterface(pConfigManager);

	if(RegisterFail)
	{
		delete pKernel;
		return -1;
	}

	pConfigManager->Init();
	pConsole->Init();

	pConsole->Register("status", "", CFGFLAG_MASTER, ConStatus, nullptr, "List the connected game servers");
	pConsole->Register("shutdown", "", CFGFLAG_MASTER, ConShutdown, nullptr, "Shut the relay down");

	if(!pConsole->ExecuteFile("relay.cfg", -1, true))
	{
		pConsole->ExecuteFile("autoexec.cfg", -1, true);
	}

	if(argc > 1)
		pConsole->ParseArguments(argc - 1, &argv[1]);

	NETADDR BindAddr;
	if(!BuildBindAddr(&BindAddr))
	{
		delete pKernel;
		return -1;
	}

	CRelayConfig Config;
	Config.m_BindAddr = BindAddr;
	Config.m_MaxClients = g_Config.m_RelayMaxClients;
	Config.m_Password = g_Config.m_RelayPassword;
	Config.m_Name = g_Config.m_RelayName;

	CRelay Relay;
	g_pRelay = &Relay;
	if(!Relay.Open(Config))
	{
		g_pRelay = nullptr;
		delete pKernel;
		return -1;
	}

	log_info("relay", "MRPG relay started, protocol version %d", relay::PROTOCOL_VERSION);

	while(!g_InterruptSignaled)
	{
		// The relay parses data of anyone who can reach the port. A defect in the
		// handling of a malformed message has to cost that connection at most, not
		// the whole relay with every server on it.
		try
		{
			Relay.Update();
		}
		catch(const std::exception &Exception)
		{
			log_error("relay", "unhandled error while processing the connections: %s", Exception.what());
		}
		catch(...)
		{
			log_error("relay", "unhandled error while processing the connections");
		}

		// 5 ms keeps the relay responsive without busy waiting when idle.
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}

	log_info("relay", "shutting down");
	Relay.Close();
	g_pRelay = nullptr;

	delete pKernel;
	secure_random_uninit();
	return 0;
}
