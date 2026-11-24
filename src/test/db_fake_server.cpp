#include "db_fake_server.h"

#include <base/system.h>

#if defined(CONF_FAMILY_UNIX)
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <cstdlib>
#include <cstring>

namespace
{
	// Where the repository lives, known at compile time: the tests need the
	// path of the python fake server relative to it.
#ifndef MRPG_SOURCE_DIR
#define MRPG_SOURCE_DIR "."
#endif
	const char* FAKE_SERVER_SCRIPT = MRPG_SOURCE_DIR "/src/test/fake_mariadb_server.py";

	bool PythonAvailable()
	{
		const int Status = std::system("python3 --version > /dev/null 2>&1");
		return Status == 0;
	}
}

int CFakeMariaDb::FindFreePort()
{
#if defined(CONF_FAMILY_UNIX)
	const int Socket = socket(AF_INET, SOCK_STREAM, 0);
	if(Socket < 0)
		return 0;

	sockaddr_in Address {};
	Address.sin_family = AF_INET;
	Address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	Address.sin_port = 0;
	if(bind(Socket, reinterpret_cast<sockaddr*>(&Address), sizeof(Address)) != 0)
	{
		close(Socket);
		return 0;
	}

	socklen_t Length = sizeof(Address);
	const int Port = getsockname(Socket, reinterpret_cast<sockaddr*>(&Address), &Length) == 0
		? ntohs(Address.sin_port) : 0;
	close(Socket);
	return Port;
#else
	return 0;
#endif
}

CFakeMariaDb::CFakeMariaDb(std::string Name, int Port, std::vector<std::string> vDatabases) :
	m_Name(std::move(Name)), m_Port(Port), m_vDatabases(std::move(vDatabases))
{
#if defined(CONF_FAMILY_UNIX)
	if(!PythonAvailable())
	{
		m_UnavailableReason = "python3 is not available";
		return;
	}
	if(access(FAKE_SERVER_SCRIPT, R_OK) != 0)
	{
		m_UnavailableReason = "src/test/fake_mariadb_server.py is missing";
		return;
	}
	m_Available = true;
#else
	m_UnavailableReason = "the fake server is only started on unix";
#endif
}

CFakeMariaDb::~CFakeMariaDb()
{
	Stop();
}

bool CFakeMariaDb::Spawn()
{
#if defined(CONF_FAMILY_UNIX)
	int PipeFds[2];
	if(pipe(PipeFds) != 0)
		return false;

	// Arguments are kept alive until the child is gone.
	std::vector<std::string> vArgs;
	vArgs.push_back("python3");
	vArgs.push_back(FAKE_SERVER_SCRIPT);
	vArgs.push_back("--name");
	vArgs.push_back(m_Name);
	vArgs.push_back("--port");
	vArgs.push_back(std::to_string(m_Port));
	for(const std::string& Database : m_vDatabases)
	{
		vArgs.push_back("--database");
		vArgs.push_back(Database);
	}
	std::vector<char*> vArgv;
	for(std::string& Argument : vArgs)
		vArgv.push_back(Argument.data());
	vArgv.push_back(nullptr);

	const pid_t Pid = fork();
	if(Pid < 0)
	{
		close(PipeFds[0]);
		close(PipeFds[1]);
		return false;
	}
	if(Pid == 0)
	{
		// child: stdout carries the readiness line, stderr is visible in the log
		dup2(PipeFds[1], STDOUT_FILENO);
		close(PipeFds[0]);
		close(PipeFds[1]);
		execvp("python3", vArgv.data());
		_exit(127);
	}

	close(PipeFds[1]);
	m_Pid = Pid;
	m_ReadFd = PipeFds[0];
	return true;
#else
	return false;
#endif
}

bool CFakeMariaDb::Start()
{
	if(!m_Available || m_Running)
		return false;

	if(m_Port <= 0)
		m_Port = FindFreePort();
	if(m_Port <= 0)
		return false;

	if(!Spawn())
		return false;

	// The server prints "READY <port>" once it listens. Anything else (a python
	// traceback) means the test cannot run.
#if defined(CONF_FAMILY_UNIX)
	std::string Line;
	const int64_t Deadline = time_get() + time_freq() * 10;
	bool Ready = false;
	while(!Ready && time_get() < Deadline)
	{
		pollfd Poll {};
		Poll.fd = m_ReadFd;
		Poll.events = POLLIN;
		const int Result = poll(&Poll, 1, 500);
		if(Result <= 0)
		{
			if(m_Pid > 0 && waitpid(m_Pid, nullptr, WNOHANG) == m_Pid)
			{
				m_Pid = -1;
				m_UnavailableReason = "the fake server exited during start-up";
				break;
			}
			continue;
		}

		char Buffer[256];
		const ssize_t Bytes = read(m_ReadFd, Buffer, sizeof(Buffer) - 1);
		if(Bytes <= 0)
			break;
		Line.append(Buffer, static_cast<size_t>(Bytes));
		if(Line.find('\n') != std::string::npos)
		{
			Ready = Line.rfind("READY", 0) == 0;
			if(!Ready)
				break;
		}
	}
	close(m_ReadFd);
	m_ReadFd = -1;
	if(!Ready)
	{
		m_UnavailableReason = "the fake server did not become ready";
		Reap();
		return false;
	}
	m_Running = true;
	return true;
#else
	return false;
#endif
}

void CFakeMariaDb::Reap()
{
#if defined(CONF_FAMILY_UNIX)
	if(m_Pid <= 0)
		return;
	kill(m_Pid, SIGTERM);
	for(int Attempt = 0; Attempt < 100; ++Attempt)
	{
		if(waitpid(m_Pid, nullptr, WNOHANG) == m_Pid)
		{
			m_Pid = -1;
			return;
		}
		usleep(20000);
	}
	kill(m_Pid, SIGKILL);
	waitpid(m_Pid, nullptr, 0);
	m_Pid = -1;
#endif
}

void CFakeMariaDb::Stop()
{
	if(!m_Running)
		return;
	// The child may already be gone (a test killed it to simulate a lost server).
	Reap();
#if defined(CONF_FAMILY_UNIX)
	if(m_ReadFd >= 0)
		close(m_ReadFd);
#endif
	m_ReadFd = -1;
	m_Running = false;
}
