#ifndef TEST_DB_FAKE_SERVER_H
#define TEST_DB_FAKE_SERVER_H

// Starts and stops the protocol-level fake MariaDB server shared by the DB
// tests (src/test/fake_mariadb.py) as a child process.
//
// It is a real server on a real socket: the tests therefore exercise the real
// MariaDB client of the server, including its handshake, its error handling and
// its behaviour when the server goes away. Only the storage is fake.
//
// The process control is POSIX only; on other systems Start() fails and the
// tests skip themselves instead of pretending to pass.

#include <string>
#include <vector>

class CFakeMariaDb
{
public:
	CFakeMariaDb(std::string Name, int Port, std::vector<std::string> vDatabases = {});
	~CFakeMariaDb();

	CFakeMariaDb(const CFakeMariaDb&) = delete;
	CFakeMariaDb& operator=(const CFakeMariaDb&) = delete;

	// Starts the server and waits until it listens. Blocking, seconds at most.
	bool Start();
	// Stops the server; connections of the client die with it.
	void Stop();
	bool IsRunning() const { return m_Running; }

	int Port() const { return m_Port; }
	// False when the test environment cannot run the fake server (no python3).
	bool IsAvailable() const { return m_Available; }
	const char* UnavailableReason() const { return m_UnavailableReason; }

	// A port that is free right now on 127.0.0.1, for tests that stop and
	// restart a server on the same port. 0 if none was found.
	static int FindFreePort();

private:
	std::string m_Name;
	int m_Port;
	std::vector<std::string> m_vDatabases;

	bool m_Available = false;
	bool m_Running = false;
	const char* m_UnavailableReason = "";
	int m_Pid = -1;
	int m_ReadFd = -1;

	bool Spawn();
	void Reap();
};

#endif // TEST_DB_FAKE_SERVER_H
