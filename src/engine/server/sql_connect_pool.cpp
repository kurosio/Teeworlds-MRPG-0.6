#include <base/system.h>
#include "sql_connect_pool.h"

// #####################################################
// THREAD POOL IMPLEMENTATION
// #####################################################
namespace
{
	ResultPtr EmptyResult()
	{
		static ResultPtr s_Empty = std::make_shared<WrapperResultSet>(nullptr, nullptr);
		return s_Empty;
	}

	constexpr int kMaxTaskRetries = 3;
	constexpr int kMaxDmlRetries = 1;
	constexpr int kBaseRetryDelayMs = 500;
	constexpr int kConnectRetryDelayMs = 500;
	constexpr int kQueueWaitTimeoutMs = 1000;

	const char* DbTypeName(DB type)
	{
		switch(type)
		{
			case DB::SELECT:
				return "SELECT";
			case DB::INSERT:
				return "INSERT";
			case DB::UPDATE:
				return "UPDATE";
			case DB::REMOVE:
				return "REMOVE";
			case DB::OTHER:
				return "OTHER";
		}
		return "UNKNOWN";
	}

	int MaxRetriesForType(DB type)
	{
		return type == DB::SELECT ? kMaxTaskRetries : kMaxDmlRetries;
	}

	double DurationMs(int64_t startTicks, int64_t endTicks)
	{
		const int64_t freq = time_freq();
		if(freq <= 0)
			return 0.0;
		return static_cast<double>(endTicks - startTicks) * 1000.0 / static_cast<double>(freq);
	}

	void CloseConnectionOnError(Connection* pConnection, const char* pContext)
	{
		if(!pConnection)
			return;
		try
		{
			pConnection->close();
		}
		catch(const SQLException& e)
		{
			dbg_msg("SQL Error", "%s: failed to close connection after error: %s", pContext, e.what());
		}
	}

std::unique_ptr<Connection>& GetSyncConnection()
	{
		thread_local std::unique_ptr<Connection> s_pSyncConnection;
		if(!s_pSyncConnection || s_pSyncConnection->isClosed())
		{
			if(s_pSyncConnection && s_pSyncConnection->isClosed())
				dbg_msg("SQL Sync", "Connection closed, reconnecting.");
			s_pSyncConnection = CConectionPool::CreateConnection();
		}
		return s_pSyncConnection;
	}
}

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

void CThreadPool::Enqueue(std::function<void(Connection*, int)> task, DB type, std::string query)
{
	EnqueueWithRetry(std::move(task), type, std::move(query), 0);
}

void CThreadPool::EnqueueWithRetry(std::function<void(Connection*, int)> task, DB type, std::string query, int retryCount)
{
	CTask newTask{std::move(task), time_get(), type, std::move(query), retryCount};
	EnqueueTask(std::move(newTask));
}

void CThreadPool::EnqueueTask(CTask&& task)
{
	size_t queuedSize = 0;
	const size_t maxQueueSize = static_cast<size_t>(g_Config.m_SvQueueMaxSize);
	const DB taskType = task.m_Type;

	{
		std::unique_lock<std::mutex> lock(m_mxQueue);
		bool warned = false;
		if(maxQueueSize > 0)
		{
			while(!m_bStop.load() && m_QueueSize.load() >= maxQueueSize)
			{
				if(!warned)
				{
					dbg_msg("SQL Worker", "Queue limit reached (%zu). Waiting to enqueue (type: %s).", maxQueueSize, DbTypeName(task.m_Type));
					warned = true;
				}
				if(m_cvCondition.wait_for(lock, std::chrono::milliseconds(kQueueWaitTimeoutMs)) == std::cv_status::timeout)
				{
					dbg_msg("SQL Worker", "Queue enqueue timed out after %dms (type: %s). Task dropped.", kQueueWaitTimeoutMs, DbTypeName(task.m_Type));
					return;
				}
			}
		}

		if(m_bStop.load())
			return;

		m_qTasks.emplace(std::move(task));
		queuedSize = m_QueueSize.fetch_add(1) + 1;
	}

	if(queuedSize > static_cast<size_t>(g_Config.m_SvQueueWarnSize))
	{
		dbg_msg("SQL Worker", "Queue size high: %zu tasks (latest type: %s).", queuedSize, DbTypeName(taskType));
	}

	// Notify one waiting worker thread that a task is available.
	m_cvCondition.notify_one();
}

void CThreadPool::EnqueueRetry(CTask&& task, const char* reason, int maxRetries)
{
	if(task.m_RetryCount >= maxRetries)
	{
		dbg_msg("SQL Worker", "%s. Dropping task after %d retries. Query: %s", reason, maxRetries, task.m_Query.c_str());
		return;
	}

	dbg_msg("SQL Worker", "%s. Retrying (attempt %d/%d). Query: %s", reason, task.m_RetryCount + 1, maxRetries, task.m_Query.c_str());
	task.m_RetryCount++;
	task.m_EnqueueTime = time_get();
	EnqueueTask(std::move(task));
}

void CThreadPool::WorkerThread()
{
	// Each worker thread has its own, persistent connection.
	// It is created once when the thread starts.
	std::unique_ptr<Connection> pConnection = CConectionPool::CreateConnection();

	// The main loop for the worker thread.
	while(true)
	{
		CTask task;
		bool hasTask = false;

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
				m_QueueSize.fetch_sub(1);
				hasTask = true;
			}
		}

		if(hasTask)
			m_cvCondition.notify_all();

		// --- Execute the task ---
		if(hasTask)
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
					const int64_t startWait = task.m_EnqueueTime;
					const double waitMs = DurationMs(startWait, time_get());
					if(waitMs > static_cast<double>(g_Config.m_SvQueueWaitWarnMs))
					{
						dbg_msg("SQL Worker", "Task waited %.2fms in queue (type: %s). Query: %s", waitMs, DbTypeName(task.m_Type), task.m_Query.c_str());
					}
					task.m_Task(pConnection.get(), task.m_RetryCount);
				}
				else
				{
					EnqueueRetry(std::move(task), "Worker has no valid connection", MaxRetriesForType(task.m_Type));
				}
			}
			catch(const SQLException& e)
			{
				const bool connectionLost = is_connection_lost(e);
				if(connectionLost)
				{
					EnqueueRetry(std::move(task), "Connection lost during task", MaxRetriesForType(task.m_Type));
				}
				else
				{
					dbg_msg("SQL Error", "Worker caught SQLException during task: %s. Connection will be reset.", e.what());
				}
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
	static std::atomic<int64_t> s_LastConnectFailure{0};
	const int64_t now = time_get();
	const int64_t lastFailure = s_LastConnectFailure.load();
	if(lastFailure > 0 && DurationMs(lastFailure, now) < kConnectRetryDelayMs)
		return nullptr;

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

		s_LastConnectFailure.store(0);
		return pConnection;
	}
	catch(const SQLException& e)
	{
		s_LastConnectFailure.store(now);
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
	// Synchronous queries run on the calling thread and reuse a thread-local connection.
	auto& pConnection = GetSyncConnection();
	if(!pConnection)
	{
		dbg_msg("SQL Error", "Sync Execute failed to create a connection.");
		return EmptyResult();
	}

	const auto runQuery = [this](Connection* pConnection)
	{
		std::unique_ptr<Statement> pStmt(pConnection->createStatement());
		std::unique_ptr<ResultSet> pResult(pStmt->executeQuery(m_Query.c_str()));
		return ResultPtr(std::make_shared<WrapperResultSet>(std::move(pStmt), std::move(pResult)));
	};

	const int64_t start = time_get();
	try
	{
		ResultPtr result = runQuery(pConnection.get());
		const double durationMs = DurationMs(start, time_get());
		if(durationMs > static_cast<double>(g_Config.m_SvSyncSelectWarnMs))
		{
			dbg_msg("SQL Warning", "Slow sync SELECT (%.2fms). Query: %s", durationMs, m_Query.c_str());
		}
		return result ? result : EmptyResult();
	}
	catch(const SQLException& e)
	{
		const double durationMs = DurationMs(start, time_get());
		if(durationMs > static_cast<double>(g_Config.m_SvSyncSelectWarnMs))
		{
			dbg_msg("SQL Warning", "Slow sync SELECT failed (%.2fms). Query: %s", durationMs, m_Query.c_str());
		}
		dbg_msg("SQL Error", "Sync SELECT failed: %s. Query: %s", e.what(), m_Query.c_str());
		if(is_connection_lost(e))
		{
			CloseConnectionOnError(pConnection.get(), "Sync SELECT");
			pConnection = nullptr;
			for(int attempt = 0; attempt < kMaxTaskRetries; ++attempt)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(kBaseRetryDelayMs * (attempt + 1)));
				auto& retryConnection = GetSyncConnection();
				if(!retryConnection)
					continue;

				try
				{
					return runQuery(retryConnection.get());
				}
				catch(const SQLException& retryError)
				{
					dbg_msg("SQL Error", "Sync SELECT retry failed: %s. Query: %s", retryError.what(), m_Query.c_str());
					if(!is_connection_lost(retryError))
						break;
					CloseConnectionOnError(retryConnection.get(), "Sync SELECT retry");
					retryConnection = nullptr;
				}
			}
		}
		return EmptyResult();
	}
}

// --- ASYNCHRONOUS SELECT ---
void CConectionPool::CResultSelect::AtExecute(CallbackResultPtr pCallbackResult)
{
	// Package the query logic into a lambda that matches the thread pool's task signature.
	auto task = std::make_shared<std::function<void(Connection*, int)>>();
	*task = [query = m_Query, cb = std::move(pCallbackResult), task](Connection* /*pConnection*/, int retryCount)
	{
		// Use a dedicated connection for async SELECT to avoid overlapping result sets on the worker connection.
		auto ownedConnection = CConectionPool::CreateConnection();
		if(!ownedConnection)
		{
			if(retryCount < kMaxTaskRetries)
			{
				dbg_msg("SQL Worker", "Async SELECT connection unavailable. Retrying (attempt %d/%d). Query: %s", retryCount + 1, kMaxTaskRetries, query.c_str());
				Database->m_pThreadPool->EnqueueWithRetry(*task, DB::SELECT, query, retryCount + 1);
				return;
			}
			dbg_msg("SQL Error", "Async SELECT failed to create a connection. Query: %s", query.c_str());
			if(cb)
				cb(nullptr);
			return;
		}
		auto sharedConnection = std::shared_ptr<Connection>(ownedConnection.release());

		try
		{
			std::unique_ptr<Statement> pStmt(sharedConnection->createStatement());
			std::unique_ptr<ResultSet> pResult(pStmt->executeQuery(query.c_str()));
			auto result = std::make_shared<WrapperResultSet>(std::move(pStmt), std::move(pResult), std::move(sharedConnection));
			if(cb)
			{
				cb(std::move(result));
			}
		}
		catch(SQLException& e)
		{
			if(is_connection_lost(e))
			{
				CloseConnectionOnError(sharedConnection.get(), "Async SELECT");
				if(retryCount == 0)
					throw;
			}

			dbg_msg("SQL Error", "Async SELECT failed: %s. Query: %s", e.what(), query.c_str());

			// Notify the caller of the failure.
			if(cb)
				cb(nullptr);
		}
	};

	// Enqueue the task for the thread pool to execute when a worker is free.
	Database->m_pThreadPool->Enqueue(*task, DB::SELECT, m_Query);
}


// --- ASYNCHRONOUS DML (INSERT/UPDATE/DELETE) ---
void CConectionPool::CResultQuery::AtExecute(CallbackUpdatePtr pCallbackResult, int DelayMilliseconds)
{
	// Create a lambda task for the DML operation.
	auto task = [query = m_Query, cb = std::move(pCallbackResult), delay = DelayMilliseconds](Connection* pConnection, int retryCount)
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
			if(is_connection_lost(e))
			{
				CloseConnectionOnError(pConnection, "Async DML");
				if(retryCount < kMaxDmlRetries)
					throw;
			}

			dbg_msg("SQL Error", "Async DML failed: %s. Query: %s", e.what(), query.c_str());
		}
	};

	Database->m_pThreadPool->Enqueue(std::move(task), m_TypeQuery, m_Query);
}
