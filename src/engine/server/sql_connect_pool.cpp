#include <base/system.h>
#include "sql_connect_pool.h"

// #####################################################
// THREAD POOL IMPLEMENTATION
// #####################################################
CThreadPool::CThreadPool(size_t numThreads) : m_bStop(false)
{
	for(size_t i = 0; i < numThreads; ++i)
	{
		m_vWorkers.emplace_back([this] { this->WorkerThread(); });
	}
}

CThreadPool::~CThreadPool()
{
	// Set the stop flag to signal threads to exit their loops.
	m_bStop.store(true);

	// Notify all waiting threads to wake them up.
	m_cvCondition.notify_all();

	// Join all threads to ensure they finish cleanly before the destructor exits.
	for(std::thread& worker : m_vWorkers)
	{
		if(worker.joinable())
			worker.join();
	}
}

void CThreadPool::Enqueue(std::function<void(Connection*)> task)
{
	// Lock the queue to safely push a new task.
	{
		std::unique_lock<std::mutex> lock(m_mxQueue);
		m_qTasks.push(std::move(task));
	}
	// Notify one waiting worker thread that a task is available.
	m_cvCondition.notify_one();
}

void CThreadPool::WorkerThread()
{
	// Each worker thread has its own, persistent connection.
	// It is created once when the thread starts.
	std::unique_ptr<Connection> pConnection = CConectionPool::CreateConnection();

	// The main loop for the worker thread.
	while(true)
	{
		std::function<void(Connection*)> task;

		// Lock the queue to safely access it.
		bool timedOut = false;
		{
			std::unique_lock<std::mutex> lock(m_mxQueue);

			// Wait until there's a task or the pool is stopping.
			// The lambda prevents spurious wakeups.
			if(!m_cvCondition.wait_for(lock, std::chrono::seconds(60), [this]{ return m_bStop || !m_qTasks.empty(); }))
				timedOut = true;

			// If stopping and no tasks are left, exit the thread's loop.
			if(m_bStop && m_qTasks.empty())
				return;

			// Get the next task from the queue.
			if(!m_qTasks.empty())
			{
				task = std::move(m_qTasks.front());
				m_qTasks.pop();
			}
		}

		// --- Execute the task ---
		if(task)
		{
			try
			{
				// Check if the connection is dead. If so, try to reconnect.
				if(!pConnection || pConnection->isClosed())
				{
					dbg_msg("SQL Worker", "Connection is dead, reconnecting for task...");
					pConnection = CConectionPool::CreateConnection();
				}

				if(pConnection)
				{
					// Execute the actual task (a lambda containing the query logic).
					// We pass the raw connection pointer to the task.
					task(pConnection.get());
				}
				else
				{
					dbg_msg("SQL Error", "Worker has no valid connection. Task skipped.");
				}
			}
			catch(const SQLException& e)
			{
				dbg_msg("SQL Error", "Worker caught SQLException during task: %s. Connection will be reset.", e.what());
				pConnection = nullptr;
			}
			catch(const std::exception& e)
			{
				dbg_msg("Error", "Worker caught std::exception during task: %s", e.what());
			}
		}
		else if(timedOut)
		{
			if(pConnection && !pConnection->isClosed())
			{
				try
				{
					std::unique_ptr<Statement> pStmt(pConnection->createStatement());
					pStmt->execute("SELECT 1");
				}
				catch(const SQLException& e)
				{
					dbg_msg("SQL Worker", "Keep-alive ping failed: %s. Connection will be reset.", e.what());
					pConnection = nullptr;
				}
			}
		}

	}
}


// #####################################################
// SQL CONNECTION POOL (QUERY MANAGER) IMPLEMENTATION
// #####################################################
CConectionPool::CConectionPool()
{
	try
	{
		m_pDriver = get_driver_instance();
		const auto thread_count = std::max(2u, std::thread::hardware_concurrency());
		m_pThreadPool = std::make_unique<CThreadPool>(thread_count);
	}
	catch(const SQLException& e)
	{
		dbg_msg("Sql Exception", "Failed to initialize SQL Driver: %s", e.what());
		throw;
	}
}

CConectionPool::~CConectionPool()
{
	// The unique_ptr will automatically call the CThreadPool destructor,
	// which will cleanly stop and join all worker threads.
}

std::unique_ptr<Connection> CConectionPool::CreateConnection()
{
	try
	{
		Driver* pDriver = get_driver_instance();
		std::string Hostname(g_Config.m_SvMySqlHost);
		Hostname.append(":" + std::to_string(g_Config.m_SvMySqlPort));
		auto pConnection = std::unique_ptr<Connection>(pDriver->connect(Hostname.c_str(), g_Config.m_SvMySqlLogin, g_Config.m_SvMySqlPassword));

		// Set connection options
		constexpr bool reconnect = true;
		constexpr int  connect_timeout = 10;
		constexpr int  read_timeout = 30;
		constexpr int  write_timeout = 30;
		pConnection->setClientOption("OPT_CHARSET_NAME", "utf8mb4");
		pConnection->setClientOption("OPT_RECONNECT", &reconnect);
		pConnection->setClientOption("OPT_CONNECT_TIMEOUT", &connect_timeout);
		pConnection->setClientOption("OPT_READ_TIMEOUT", &read_timeout);
		pConnection->setClientOption("OPT_WRITE_TIMEOUT", &write_timeout);
		pConnection->setSchema(g_Config.m_SvMySqlDatabase);

		return pConnection;
	}
	catch(const SQLException& e)
	{
		dbg_msg("Sql Exception", "Failed to create new SQL connection: %s", e.what());
		return nullptr;
	}
}

// #####################################################
// QUERY EXECUTION METHOD IMPLEMENTATIONS
// #####################################################

// --- SYNCHRONOUS SELECT ---
[[nodiscard]] ResultPtr CConectionPool::CResultSelect::Execute() const
{
	// Synchronous queries run on the calling thread and use a temporary, single-use connection.
	// This is simple and avoids complex synchronization with the thread pool.
	auto pConnection = CConectionPool::CreateConnection();
	if(!pConnection)
	{
		dbg_msg("SQL Error", "Sync Execute failed to create a connection.");
		return nullptr;
	}

	try
	{
		// Statement and ResultSet are managed by unique_ptr and the returned shared_ptr.
		std::unique_ptr<Statement> pStmt(pConnection->createStatement());
		ResultSet* rawResult = pStmt->executeQuery(m_Query.c_str());
		return std::make_shared<WrapperResultSet>(rawResult);
	}
	catch(const SQLException& e)
	{
		dbg_msg("SQL Error", "Sync SELECT failed: %s. Query: %s", e.what(), m_Query.c_str());
		return nullptr;
	}
}

// --- ASYNCHRONOUS SELECT ---
void CConectionPool::CResultSelect::AtExecute(CallbackResultPtr pCallbackResult)
{
	// Package the query logic into a lambda that matches the thread pool's task signature.
	auto task = [query = m_Query, cb = std::move(pCallbackResult)](Connection* pConnection)
	{
		// This code will run inside a worker thread using its dedicated connection.
		try
		{
			std::unique_ptr<Statement> pStmt(pConnection->createStatement());
			ResultSet* rawResult = pStmt->executeQuery(query.c_str());

			auto result = std::make_shared<WrapperResultSet>(rawResult);
			if(cb)
			{
				cb(std::move(result));
			}
		}
		catch(SQLException& e)
		{
			dbg_msg("SQL Error", "Async SELECT failed: %s. Query: %s", e.what(), query.c_str());

			// Notify the caller of the failure.
			if(cb)
				cb(nullptr);

			// Rethrow to trigger reconnect in the worker
			if(is_connection_lost(e))
				throw;
		}
	};

	// Enqueue the task for the thread pool to execute when a worker is free.
	Database->m_pThreadPool->Enqueue(std::move(task));
}


// --- ASYNCHRONOUS DML (INSERT/UPDATE/DELETE) ---
void CConectionPool::CResultQuery::AtExecute(CallbackUpdatePtr pCallbackResult, int DelayMilliseconds)
{
	// Create a lambda task for the DML operation.
	auto task = [query = m_Query, cb = std::move(pCallbackResult), delay = DelayMilliseconds](Connection* pConnection)
	{
		if(delay > 0)
			std::this_thread::sleep_for(std::chrono::milliseconds(delay));

		try
		{
			std::unique_ptr<Statement> pStmt(pConnection->createStatement());
			pStmt->execute(query.c_str());

			if(cb)
			{
				cb();
			}
		}
		catch(SQLException& e)
		{
			dbg_msg("SQL Error", "Async DML failed: %s. Query: %s", e.what(), query.c_str());

			// Rethrow to trigger reconnect in the worker
			if(is_connection_lost(e))
				throw;
		}
	};

	Database->m_pThreadPool->Enqueue(std::move(task));
}