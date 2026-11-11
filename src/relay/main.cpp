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
 *   mmoteeworlds_relay [-s] [-f relay.cfg] ["relay_port 8383" "relay_password secret" ...]
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
#include <iterator>
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

	bool IsSilentRequested(int argc, const char **argv)
	{
		for(int i = 1; i < argc; i++)
		{
			if(str_comp("-s", argv[i]) == 0 || str_comp("--silent", argv[i]) == 0)
				return true;
		}
		return false;
	}

#if defined(CONF_FAMILY_WINDOWS)
	/*
	 * Whether this process is the only one using its console. A console that is
	 * shared with the shell belongs to the user: hiding it would hide the window
	 * of their shell as well, and the relay started from a batch file would
	 * flash the whole window of the script instead of just its own.
	 */
	bool OwnsConsole()
	{
		const HWND pConsole = GetConsoleWindow();
		if(!pConsole)
			return false;

		DWORD aProcesses[2];
		return GetConsoleProcessList(aProcesses, (DWORD)std::size(aProcesses)) <= 1;
	}
#endif

	/*
	 * The log destinations are only known once the config is read, but the
	 * messages of the start up are worth keeping as well: everything is
	 * collected by the future logger until then.
	 *
	 * A file from `logfile` is what makes a relay started in the background
	 * diagnosable: its console window is closed right away (see above), so
	 * without one there is no way to see why the relay refused a server.
	 */
	void InitLogger(CFutureLogger *pFutureLogger)
	{
		std::vector<std::shared_ptr<ILogger>> vpLoggers;
		vpLoggers.emplace_back(log_logger_stdout());

		IOHANDLE File = nullptr;
		if(g_Config.m_Logfile[0] != '\0')
			File = io_open(g_Config.m_Logfile, g_Config.m_Logappend ? IOFLAG_APPEND : IOFLAG_WRITE);
		if(File)
			vpLoggers.emplace_back(log_logger_file(File));

		pFutureLogger->Set(log_logger_collection(std::move(vpLoggers)));

		if(g_Config.m_Logfile[0] != '\0')
		{
			if(File)
				log_info("relay", "logging to '%s'", g_Config.m_Logfile);
			else
				log_error("relay", "cannot open '%s' for logging, logging to the console only", g_Config.m_Logfile);
		}
	}

	// The loop of the relay: one update, one short sleep, until a signal or the
	// `shutdown` command stops it.
	void Run(CRelay &Relay)
	{
		while(!g_InterruptSignaled)
		{
			// The relay parses data of anyone who can reach the port. A defect in
			// the handling of a malformed message has to cost that connection at
			// most, not the whole relay with every server on it.
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
	}
}

int main(int argc, const char **argv) // ignore_convention
{
	CCmdlineFix CmdlineFix(&argc, &argv);

	[[maybe_unused]] const bool Silent = IsSilentRequested(argc, argv);

#if defined(CONF_FAMILY_WINDOWS)
	// The relay of a server runs in the background: its console only exists
	// because a console program gets one, so the window is hidden before
	// anything is written to it. A console shared with a shell belongs to the
	// user and stays, and `-s` hides it even then.
	if(Silent || OwnsConsole())
		ShowWindow(GetConsoleWindow(), SW_HIDE);
#endif

	// Never freed on purpose: the global logger outlives main() and is used by
	// the flush that runs at exit, see the comment at log_set_global_logger().
	// It collects the messages of the start up until the config names the log
	// destinations, at the end of which the real loggers are attached.
	auto *pFutureLogger = new CFutureLogger();
	log_set_global_logger(pFutureLogger);

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
	auto pConsole = CreateConsole(CONSOLE_FLAGS);
	IConfigManager *pConfigManager = CreateConfigManager();

	// The console is owned by the unique_ptr above, so the kernel must not delete
	// it on shutdown as well: that double free crashed the relay on every exit.
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

	// A relay.cfg next to the binary is the normal case; autoexec.cfg is the
	// fallback of the console, and both are optional here (LogFailure = false).
	if(!pConsole->ExecuteFile("relay.cfg", -1, false))
		pConsole->ExecuteFile("autoexec.cfg", -1, false);

	// The command line overrides the file, one whole config line per argument.
	if(argc > 1)
		pConsole->ParseArguments(argc - 1, &argv[1]);

	// The settings are complete now, so the log destinations are known.
	InitLogger(pFutureLogger);

	NETADDR BindAddr;
	if(!ParseBindAddr(g_Config.m_Bindaddr, g_Config.m_RelayPort, &BindAddr))
	{
		delete pKernel;
		return -1;
	}

	CRelayConfig Config;
	Config.m_BindAddr = BindAddr;
	Config.m_MaxClients = g_Config.m_RelayMaxClients;
	Config.m_Password = g_Config.m_RelayPassword;
	Config.m_Name = g_Config.m_RelayName;

	// The console has to outlive the relay: the kernel deletes it last.
	CRelay Relay;
	g_pRelay = &Relay;
	if(!Relay.Open(Config))
	{
		g_pRelay = nullptr;
		delete pKernel;
		return -1;
	}

	log_info("relay", "MRPG relay started, protocol version %d", relay::PROTOCOL_VERSION);

	Run(Relay);

	log_info("relay", "shutting down");
	Relay.Close();
	g_pRelay = nullptr;

	delete pKernel;
	secure_random_uninit();
	return 0;
}
