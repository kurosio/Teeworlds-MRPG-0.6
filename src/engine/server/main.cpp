
#define _WIN32_WINNT 0x0501

#include <base/logger.h>
#include <base/system.h>

#include <engine/console.h>
#include <engine/engine.h>
#include <engine/server.h>
#include <engine/storage.h>

#include <engine/server/server.h>
#include <engine/server/server_logger.h>
#include <engine/shared/assertion_logger.h>

#include "multi_worlds.h"

#include <vector>

#if defined(CONF_FAMILY_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <csignal>

#include "game/server/core/rcon_processor.h"

volatile sig_atomic_t InterruptSignaled = 0;

bool IsInterrupted()
{
	return InterruptSignaled;
}

void HandleSigIntTerm(int Param)
{
	InterruptSignaled = 1;

	// Exit the next time a signal is received
	signal(SIGINT, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
}

int main(int argc, const char** argv)
{
	CCmdlineFix CmdlineFix(&argc, &argv);
	bool Silent = false;

	for(int i = 1; i < argc; i++)
	{
		if(str_comp("-s", argv[i]) == 0 || str_comp("--silent", argv[i]) == 0)
		{
			Silent = true;
#if defined(CONF_FAMILY_WINDOWS)
			ShowWindow(GetConsoleWindow(), SW_HIDE);
#endif
			break;
		}
	}

#if defined(CONF_FAMILY_WINDOWS)
	CWindowsComLifecycle WindowsComLifecycle(false);
#endif

	std::vector<std::shared_ptr<ILogger>> vpLoggers;
	std::shared_ptr<ILogger> pStdoutLogger;
	if(!Silent)
	{
		pStdoutLogger = std::shared_ptr<ILogger>(log_logger_stdout());
	}

	if(pStdoutLogger)
	{
		vpLoggers.push_back(pStdoutLogger);
	}
	std::shared_ptr<CFutureLogger> pFutureFileLogger = std::make_shared<CFutureLogger>();
	vpLoggers.push_back(pFutureFileLogger);
	std::shared_ptr<CFutureLogger> pFutureConsoleLogger = std::make_shared<CFutureLogger>();
	vpLoggers.push_back(pFutureConsoleLogger);
	std::shared_ptr<CFutureLogger> pFutureAssertionLogger = std::make_shared<CFutureLogger>();
	vpLoggers.push_back(pFutureAssertionLogger);
	log_set_global_logger(log_logger_collection(std::move(vpLoggers)).release());

	if(secure_random_init() != 0)
	{
		dbg_msg("secure", "could not initialize secure RNG");
		return -1;
	}

	signal(SIGINT, HandleSigIntTerm);
	signal(SIGTERM, HandleSigIntTerm);

#if defined(CONF_EXCEPTION_HANDLING)
	init_exception_handler();
#endif

	CConectionPool::Initilize();
	CServer* pServer = CreateServer();
	pServer->SetLoggers(pFutureFileLogger, std::move(pStdoutLogger));

	IKernel* pKernel = IKernel::Create();

	// create the components
	IEngine* pEngine = CreateEngine("MRPG", pFutureConsoleLogger, 2 * std::thread::hardware_concurrency() + 2);
	IConsole* pConsole = CreateConsole(CFGFLAG_SERVER | CFGFLAG_ECON).release();
	IStorageEngine* pStorage = CreateStorage(IStorageEngine::STORAGETYPE_SERVER, argc, argv);
	IConfigManager* pConfigManager = CreateConfigManager();

	pFutureAssertionLogger->Set(CreateAssertionLogger(pStorage, "MRPG"));
#if defined(CONF_EXCEPTION_HANDLING)
	char aBuf[IO_MAX_PATH_LENGTH];
	char aBufName[IO_MAX_PATH_LENGTH];
	char aDate[64];
	str_timestamp(aDate, sizeof(aDate));
	str_format(aBufName, sizeof(aBufName), "dumps/MRPG-Server_%s_crash_log_%s_%d_%s.RTP", CONF_PLATFORM_STRING, aDate, pid(), GIT_SHORTREV_HASH != nullptr ? GIT_SHORTREV_HASH : "");
	pStorage->GetCompletePath(IStorage::TYPE_SAVE, aBufName, aBuf, sizeof(aBuf));
	set_exception_handler_log_file(aBuf);
#endif

	{
		bool RegisterFail = false;

		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<IServer*>(pServer));
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pEngine);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pConsole);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pStorage);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pConfigManager);

		if(RegisterFail)
		{
			delete pKernel;
			return -1;
		}
	}

	pEngine->Init();
	pConfigManager->Init();
	pConsole->Init();

	// register all console commands
	pServer->RegisterCommands();
	RconProcessor::Init(pConsole, pServer); // crucial to call before config execution

	if (!pConsole->ExecuteFile(AUTOEXEC_SERVER_FILE, -1, true)) {
		pConsole->ExecuteFile(AUTOEXEC_FILE, -1, true);
	}

	if(!pServer->MultiWorlds()->LoadFromDB(pKernel))
	{
		dbg_msg("server", "failed to load worlds");
		return -1;
	}

	// Initialize console commands in sub parts
	for(int i = 0; i < pServer->MultiWorlds()->GetSizeInitilized(); i++)
		pServer->MultiWorlds()->GetWorld(i)->GameServer()->OnConsoleInit();

	// parse the command line arguments
	if(argc > 1)
	{
		pConsole->ParseArguments(argc - 1, &argv[1]);
	}

	const int Mode = g_Config.m_Logappend ? IOFLAG_APPEND : IOFLAG_WRITE;
	if(g_Config.m_Logfile[0])
	{
		IOHANDLE Logfile = pStorage->OpenFile(g_Config.m_Logfile, Mode, IStorageEngine::TYPE_SAVE_OR_ABSOLUTE);
		if(Logfile)
		{
			pFutureFileLogger->Set(log_logger_file(Logfile));
		}
		else
		{
			dbg_msg("server", "failed to open '%s' for logging", g_Config.m_Logfile);
		}
	}

	auto pServerLogger = std::make_shared<CServerLogger>(pServer);
	pEngine->SetAdditionalLogger(pServerLogger);

	// run the server
	dbg_msg("server", "starting...");
	int Ret = pServer->Run(pServerLogger.get());

	// free
	pServerLogger->OnServerDeletion();
	delete pKernel;

	secure_random_uninit();
	CConectionPool::Free();
	return Ret;
}