#include "sql_connect_pool.h"

#include <engine/shared/config.h>

#include <stdarg.h>

/*
	I don't see the point in using SELECT operations in the thread,
	since this will lead to unnecessary code, which may cause confusion,
	and by calculations if (SQL server / server) = localhost,
	this will not do any harm (but after the release is complete,
	it is advisable to use the Thread function with Callback)

	And in General, you should review the SQL system,
	it works (and has been tested by time and tests),
	but this implementation is not very narrowly focused

	This approach works if the old query is not executed before,
	 a new query it will create a reserve.

	 It may seem that it does not use Pool,
	 but in fact it is and is created as a reserve when running
	 <tlock>

	 Usage is performed in turn following synchronously
	 working running through each request in order

	 This pool is not asynchronous
*/
// sql pool connections mutex
std::mutex SqlConnectionLock;
// multithread mutex :: warning recursive
std::recursive_mutex SqlThreadRecursiveLock;

// #####################################################
// SQL CONNECTION POOL
// #####################################################
std::shared_ptr<CConectionPool> CConectionPool::m_Instance;
CConectionPool::CConectionPool()
{
	try
	{
		m_pDriver = get_driver_instance();

		SqlConnectionLock.lock();

		for(int i = 0; i < g_Config.m_SvMySqlPoolSize; ++i)
			this->CreateConnection();

		SqlConnectionLock.unlock();
	}
	catch(SQLException &e)
	{
		dbg_msg("Sql Exception", "%s", e.what());
		exit(0);
	}
}

CConectionPool::~CConectionPool()
{
	DisconnectConnectionHeap();
}

CConectionPool& CConectionPool::GetInstance()
{
	if (m_Instance.get() == nullptr)
		m_Instance.reset(new CConectionPool());
	return *m_Instance.get();
}

Connection* CConectionPool::CreateConnection()
{
	Connection *pConnection = nullptr;
	while(pConnection == nullptr)
	{
		try
		{
			std::string Hostname(g_Config.m_SvMySqlHost);
			Hostname.append(":" + std::to_string(g_Config.m_SvMySqlPort));
			pConnection = m_pDriver->connect(Hostname.c_str(), g_Config.m_SvMySqlLogin, g_Config.m_SvMySqlPassword);
			
			pConnection->setClientOption("OPT_CHARSET_NAME", "utf8mb4");
			pConnection->setClientOption("OPT_CONNECT_TIMEOUT", "10");
			pConnection->setClientOption("OPT_READ_TIMEOUT", "10");
			pConnection->setClientOption("OPT_WRITE_TIMEOUT", "20");
			pConnection->setClientOption("OPT_RECONNECT", "1");
			
			pConnection->setSchema(g_Config.m_SvMySqlDatabase);
		}
		catch(SQLException &e)
		{
			dbg_msg("Sql Exception", "%s", e.what());
			DisconnectConnection(pConnection);
		}
	}
	m_ConnList.push_back(pConnection);
	return pConnection;
}

Connection* CConectionPool::GetConnection()
{
	SqlConnectionLock.lock();

	Connection* pConnection;
	if(m_ConnList.empty())
	{
		pConnection = CreateConnection();

		SqlConnectionLock.unlock();
		return pConnection;
	}

	pConnection = m_ConnList.front();
	m_ConnList.pop_front();
	if(pConnection->isClosed())
	{
		delete pConnection;
		pConnection = nullptr;
		pConnection = CreateConnection();
	}

	SqlConnectionLock.unlock();
	return pConnection;
}

void CConectionPool::ReleaseConnection(Connection* pConnection)
{
	SqlConnectionLock.lock();

	if(pConnection)
	{
		m_ConnList.push_back(pConnection);
	}

	SqlConnectionLock.unlock();
}

void CConectionPool::DisconnectConnection(Connection* pConnection)
{
	if(pConnection)
	{
		try
		{
			pConnection->close();
		}
		catch(SQLException& e)
		{
			dbg_msg("Sql Exception", "%s", e.what());
		}
	}
	m_ConnList.remove(pConnection);
	delete pConnection;
	pConnection = nullptr;
}

void CConectionPool::DisconnectConnectionHeap()
{
	SqlConnectionLock.lock();

	for(auto& iconn : m_ConnList)
		DisconnectConnection(iconn);

	m_ConnList.clear();

	SqlConnectionLock.unlock();
}

// #####################################################
// INSERT SQL
// #####################################################
void CConectionPool::ID(const char *Table, const char *Buffer, ...)
{
	va_list Arguments;
	va_start(Arguments, Buffer);
	InsertFormated(0, Table, Buffer, Arguments);
	va_end(Arguments);
}

void CConectionPool::IDS(int Milliseconds, const char *Table, const char *Buffer, ...)
{
	va_list Arguments;
	va_start(Arguments, Buffer);
	InsertFormated(Milliseconds, Table, Buffer, Arguments);
	va_end(Arguments);
}

void CConectionPool::InsertFormated(int Milliseconds, const char *Table, const char *Buffer, va_list args)
{
	char aBuf[1024];
	#if defined(CONF_FAMILY_WINDOWS)
	_vsnprintf(aBuf, sizeof(aBuf), Buffer, args);
	#else
	vsnprintf(aBuf, sizeof(aBuf), Buffer, args);
	#endif
	aBuf[sizeof(aBuf) - 1] = '\0';
	std::string Query("INSERT INTO " + std::string(Table) + " " + std::string(aBuf) + ";");
	std::thread Thread([this, Query, Milliseconds]()
	{
		if(Milliseconds > 0)
			std::this_thread::sleep_for(std::chrono::milliseconds(Milliseconds));

		const char* pError = nullptr;

		SqlThreadRecursiveLock.lock();
		m_pDriver->threadInit();
		Connection* pConnection = SJK.GetConnection();
		try
		{
			std::unique_ptr<Statement> pStmt(pConnection->createStatement());
			pStmt->executeUpdate(Query.c_str());
			pStmt->close();
		}
		catch (SQLException & e)
		{
			pError = e.what();
		}
		SJK.ReleaseConnection(pConnection);
		m_pDriver->threadEnd();
		SqlThreadRecursiveLock.unlock();

		if(pError != nullptr)
			dbg_msg("SQL", "%s", pError);
	});
	Thread.detach();
}

// #####################################################
// UPDATE SQL
// #####################################################
void CConectionPool::UD(const char *Table, const char *Buffer, ...)
{
	va_list Arguments;
	va_start(Arguments, Buffer);
	UpdateFormated(0, Table, Buffer, Arguments);
	va_end(Arguments);
}

void CConectionPool::UDS(int Milliseconds, const char *Table, const char *Buffer, ...)
{
	va_list Arguments;
	va_start(Arguments, Buffer);
	UpdateFormated(Milliseconds, Table, Buffer, Arguments);
	va_end(Arguments);
}

void CConectionPool::UpdateFormated(int Milliseconds, const char *Table, const char *Buffer, va_list args)
{
	char aBuf[1024];
	#if defined(CONF_FAMILY_WINDOWS)
	_vsnprintf(aBuf, sizeof(aBuf), Buffer, args);
	#else
	vsnprintf(aBuf, sizeof(aBuf), Buffer, args);
	#endif
	aBuf[sizeof(aBuf) - 1] = '\0';
	std::string Query("UPDATE " + std::string(Table) + " SET " + std::string(aBuf) + ";");
	std::thread Thread([this, Query, Milliseconds]()
	{
		if(Milliseconds > 0)
			std::this_thread::sleep_for(std::chrono::milliseconds(Milliseconds));

		const char* pError = nullptr;

		SqlThreadRecursiveLock.lock();
		m_pDriver->threadInit();
		Connection* pConnection = SJK.GetConnection();
		try
		{
			std::unique_ptr<Statement> pStmt(pConnection->createStatement());
			pStmt->executeUpdate(Query.c_str());
			pStmt->close();
		}
		catch(SQLException& e)
		{
			pError = e.what();
		}
		SJK.ReleaseConnection(pConnection);
		m_pDriver->threadEnd();
		SqlThreadRecursiveLock.unlock();

		if(pError != nullptr)
			dbg_msg("SQL", "%s", pError);
	});
	Thread.detach();
}

// #####################################################
// DELETE SQL
// #####################################################
void CConectionPool::DD(const char *Table, const char *Buffer, ...)
{
	va_list Arguments;
	va_start(Arguments, Buffer);
	DeleteFormated(0, Table, Buffer, Arguments);
	va_end(Arguments);
}

void CConectionPool::DDS(int Milliseconds, const char *Table, const char *Buffer, ...)
{
	va_list Arguments;
	va_start(Arguments, Buffer);
	DeleteFormated(Milliseconds, Table, Buffer, Arguments);
	va_end(Arguments);
}

void CConectionPool::DeleteFormated(int Milliseconds, const char *Table, const char *Buffer, va_list args)
{
	char aBuf[256];
	#if defined(CONF_FAMILY_WINDOWS)
	_vsnprintf(aBuf, sizeof(aBuf), Buffer, args);
	#else
	vsnprintf(aBuf, sizeof(aBuf), Buffer, args);
	#endif
	aBuf[sizeof(aBuf) - 1] = '\0';
	std::string Query("DELETE FROM " + std::string(Table) + " " + std::string(aBuf) + ";");
	std::thread Thread([this, Query, Milliseconds]()
	{
		if(Milliseconds > 0)
			std::this_thread::sleep_for(std::chrono::milliseconds(Milliseconds));

		const char* pError = nullptr;

		SqlThreadRecursiveLock.lock();
		m_pDriver->threadInit();
		Connection* pConnection = SJK.GetConnection();
		try
		{
			std::unique_ptr<Statement> pStmt(pConnection->createStatement());
			pStmt->executeUpdate(Query.c_str());
			pStmt->close();
		}
		catch (SQLException& e)
		{
			pError = e.what();
		}
		SJK.ReleaseConnection(pConnection);
		m_pDriver->threadEnd();
		SqlThreadRecursiveLock.unlock();

		if(pError != nullptr)
			dbg_msg("SQL", "%s", pError);
	});
	Thread.detach();
}

// #####################################################
// SELECT SQL
// #####################################################
ResultPtr CConectionPool::SD(const char* Select, const char* Table, const char* Buffer, ...)
{
	char aBuf[1024];
	va_list VarArgs;
	va_start(VarArgs, Buffer);
#if defined(CONF_FAMILY_WINDOWS)
	_vsnprintf(aBuf, sizeof(aBuf), Buffer, VarArgs);
#else
	vsnprintf(aBuf, sizeof(aBuf), Buffer, VarArgs);
#endif
	va_end(VarArgs);
	aBuf[sizeof(aBuf) - 1] = '\0';

	const char* pError = nullptr;

	SqlThreadRecursiveLock.lock();
	std::string Query("SELECT " + std::string(Select) + " FROM " + std::string(Table) + " " + std::string(aBuf) + ";");
	m_pDriver->threadInit();
	Connection* pConnection = SJK.GetConnection();
	ResultPtr pResult = nullptr;
	try
	{
		std::unique_ptr<Statement> pStmt(pConnection->createStatement());
		pResult.reset(pStmt->executeQuery(Query.c_str()));
		pStmt->close();
	}
	catch(SQLException& e)
	{
		pError = e.what();
	}
	SJK.ReleaseConnection(pConnection);
	m_pDriver->threadEnd();
	SqlThreadRecursiveLock.unlock();

	if(pError != nullptr)
		dbg_msg("SQL", "%s", pError);

	return pResult;
}

void CConectionPool::SDT(const char* Select, const char* Table, std::function<void(ResultPtr)> func, const char* Buffer, ...)
{
	char aBuf[1024];
	va_list VarArgs;
	va_start(VarArgs, Buffer);
#if defined(CONF_FAMILY_WINDOWS)
	_vsnprintf(aBuf, sizeof(aBuf), Buffer, VarArgs);
#else
	vsnprintf(aBuf, sizeof(aBuf), Buffer, VarArgs);
#endif
	va_end(VarArgs);
	aBuf[sizeof(aBuf) - 1] = '\0';
	const std::string Query("SELECT " + std::string(Select) + " FROM " + std::string(Table) + " " + std::string(aBuf) + ";");
	std::thread Thread([this, Query, func]()
	{
		const char* pError = nullptr;

		SqlThreadRecursiveLock.lock();
		m_pDriver->threadInit();
		Connection* pConnection = SJK.GetConnection();
		try
		{
			std::unique_ptr<Statement> pStmt(pConnection->createStatement());
			ResultPtr pResult(pStmt->executeQuery(Query.c_str()));
			func(std::move(pResult));
		}
		catch(SQLException& e)
		{
			pError = e.what();
		}
		SJK.ReleaseConnection(pConnection);
		m_pDriver->threadEnd();
		SqlThreadRecursiveLock.unlock();

		if(pError != nullptr)
			dbg_msg("SQL", "%s", pError);
	});
	Thread.detach();
}
