#include <base/system.h>

#include "sql_connect_pool.h"

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
std::atomic_flag g_atomic_lock;

// #####################################################
// SQL CONNECTION POOL
// #####################################################
CConectionPool::CConectionPool()
{
	try
	{
		m_pDriver = get_driver_instance();
		for(int i = 0; i < g_Config.m_SvMySqlPoolSize; ++i)
			this->CreateConnection();
	}
	catch (SQLException& e)
	{
		dbg_msg("Sql Exception", "%s", e.what());
		exit(0);
	}
}

CConectionPool::~CConectionPool()
{
	DisconnectConnectionHeap();
	m_pDriver = nullptr;
}

void CConectionPool::DisconnectConnectionHeap()
{
	g_atomic_lock.test_and_set(std::memory_order_acquire);
	while(!m_ConnList.empty())
	{
		try
		{
			m_ConnList.front()->close();
			m_ConnList.pop_front();
		}
		catch(SQLException& e)
		{
			dbg_msg("Sql Exception", "%s", e.what());
			m_ConnList.pop_front();
		}
	}
	g_atomic_lock.clear(std::memory_order_release);
}

Connection* CConectionPool::CreateConnection()
{
	Connection* pConnection = nullptr;
	while (pConnection == nullptr)
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
		catch (SQLException& e)
		{
			dbg_msg("Sql Exception", "%s", e.what());
			DisconnectConnection(pConnection);
		}
	}

	g_atomic_lock.test_and_set(std::memory_order_acquire);
	m_ConnList.push_back(pConnection);
	g_atomic_lock.clear(std::memory_order_release);
	return pConnection;
}

Connection* CConectionPool::GetConnection()
{
	Connection* pConnection;
	if(m_ConnList.empty())
	{
		pConnection = CreateConnection();
		return pConnection;
	}

	g_atomic_lock.test_and_set(std::memory_order_acquire);
	pConnection = m_ConnList.front();
	m_ConnList.pop_front();
	g_atomic_lock.clear(std::memory_order_relaxed);

	if (pConnection->isClosed())
	{
		delete pConnection;
		pConnection = CreateConnection();
	}

	return pConnection;
}

void CConectionPool::ReleaseConnection(Connection* pConnection)
{
	if(pConnection)
	{
		g_atomic_lock.test_and_set(std::memory_order_acquire);
		m_ConnList.push_back(pConnection);
		g_atomic_lock.clear(std::memory_order_release);
	}
}

void CConectionPool::DisconnectConnection(Connection* pConnection)
{
	try
	{
		if(pConnection)
		{
			pConnection->close();
		}
	}
	catch (SQLException& e)
	{
		dbg_msg("Sql Exception", "%s", e.what());
	}

	g_atomic_lock.test_and_set(std::memory_order_acquire);
	m_ConnList.remove(pConnection);
	delete pConnection;
	g_atomic_lock.clear(std::memory_order_release);
}