#include <base/system.h>
#include "sql_connect_pool.h"
#include "sql_lost_query_logger.h"
#include "sql_table_routing.h"

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

	void LogLostQuery(const char* reason, DB type, const std::string& query)
	{
		CSqlLostQueryLogger::Instance().LogLostQuery(reason, DbTypeName(type), query);
	}

	int MaxRetriesForType(DB type)
	{
		return type == DB::SELECT ? g_Config.m_SvSqlSelectMaxRetries : g_Config.m_SvSqlDmlMaxRetries;
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

	void DrainStatementResults(Statement* pStmt)
	{
		if(!pStmt)
			return;

		try
		{
			if(auto* pResult = pStmt->getResultSet())
				std::unique_ptr<ResultSet> pAutoResult(pResult);

			while(pStmt->getMoreResults())
			{
				if(auto* pResult = pStmt->getResultSet())
					std::unique_ptr<ResultSet> pAutoResult(pResult);
			}
		}
		catch(const SQLException& e)
		{
			dbg_msg("SQL Error", "Failed to drain statement results: %s", e.what());
		}
	}

	// synchronous queries reuse one connection per calling thread and database.
	std::unique_ptr<Connection>& GetSyncConnection(bool Local)
	{
		thread_local std::unique_ptr<Connection> s_apSyncConnections[2];
		std::unique_ptr<Connection>& pSyncConnection = s_apSyncConnections[Local ? 1 : 0];
		if(!pSyncConnection || pSyncConnection->isClosed())
		{
			pSyncConnection = CConectionPool::CreateConnection(Local);
		}
		return pSyncConnection;
	}

	// reads the lower case names of all tables of the database the connection is opened to.
	bool ListTables(Connection* pConnection, std::vector<std::string>& vTables)
	{
		try
		{
			std::unique_ptr<Statement> pStmt(pConnection->createStatement());
			std::unique_ptr<ResultSet> pResult(pStmt->executeQuery("SHOW TABLES;"));
			while(pResult->next())
				vTables.emplace_back(sqlroute::ToLowerName(pResult->getString(1).c_str()));
			return true;
		}
		catch(SQLException& e)
		{
			dbg_msg("SQL Error", "Failed to read the list of tables: %s", e.what());
			return false;
		}
	}

	template<typename TCallback>
	class CAsyncQueryContext
	{
		std::string m_Query;
		DB m_Type;
		TCallback m_Callback;
		int m_DelayMilliseconds;

	public:
		CAsyncQueryContext(std::string Query, DB Type, TCallback Callback, int DelayMilliseconds = 0)
			: m_Query(std::move(Query)), m_Type(Type), m_Callback(std::move(Callback)), m_DelayMilliseconds(DelayMilliseconds)
		{
		}

		const std::string& Query() const { return m_Query; }
		DB Type() const { return m_Type; }
		int DelayMilliseconds() const { return m_DelayMilliseconds; }

		const TCallback& Callback() const { return m_Callback; }
	};

	void NotifySelect(const CallbackResultPtr& Callback, ResultPtr pResult)
	{
		if(!Callback)
			return;

		Callback(std::move(pResult));
	}

	void NotifyDml(const CallbackUpdatePtr& Callback, bool Updated)
	{
		if(!Callback)
			return;

		Callback(Updated);
	}

	using CAsyncSelectContext = CAsyncQueryContext<CallbackResultPtr>;
	using CAsyncDmlContext = CAsyncQueryContext<CallbackUpdatePtr>;

	std::function<void(Connection*, int)> CreateAsyncSelectTask(const std::shared_ptr<CAsyncSelectContext>& pContext, CThreadPool* pThreadPool)
	{
		return [pContext, pThreadPool](Connection* pConnection, int RetryCount)
		{
			if(!pConnection)
			{
				if(RetryCount < g_Config.m_SvSqlSelectMaxRetries)
				{
					dbg_msg("SQL Worker", "Async SELECT connection unavailable. Retrying (attempt %d/%d). Query: %s", RetryCount + 1, g_Config.m_SvSqlSelectMaxRetries, pContext->Query().c_str());
					if(pThreadPool)
						pThreadPool->EnqueueWithRetry(CreateAsyncSelectTask(pContext, pThreadPool), pContext->Type(), pContext->Query(), RetryCount + 1);
					return;
				}

				LogLostQuery("async select connection unavailable", pContext->Type(), pContext->Query());
				dbg_msg("SQL Error", "Async SELECT failed to create a connection. Query: %s", pContext->Query().c_str());
				NotifySelect(pContext->Callback(), EmptyResult());
				return;
			}

			try
			{
				std::unique_ptr<Statement> pStmt(pConnection->createStatement());
				std::unique_ptr<ResultSet> pResult(pStmt->executeQuery(pContext->Query().c_str()));
				auto result = std::make_shared<WrapperResultSet>(std::move(pStmt), std::move(pResult));
				NotifySelect(pContext->Callback(), std::move(result));
			}
			catch(SQLException& e)
			{
				if(is_connection_lost(e))
				{
					CloseConnectionOnError(pConnection, "Async SELECT");
					if(RetryCount < g_Config.m_SvSqlSelectMaxRetries)
						throw;
				}

				dbg_msg("SQL Error", "Async SELECT failed (code=%d, SQLSTATE=%s): %s. Query: %s", e.getErrorCode(), e.getSQLStateCStr() ? e.getSQLStateCStr() : "unknown", e.what(), pContext->Query().c_str());
				LogLostQuery("async select failed", pContext->Type(), pContext->Query());
				NotifySelect(pContext->Callback(), EmptyResult());
			}
		};
	}

	std::function<void(Connection*, int)> CreateAsyncDmlTask(const std::shared_ptr<CAsyncDmlContext>& pContext, CThreadPool* pThreadPool)
	{
		return [pContext, pThreadPool](Connection* pConnection, int RetryCount)
		{
			if(pContext->DelayMilliseconds() > 0)
				std::this_thread::sleep_for(std::chrono::milliseconds(pContext->DelayMilliseconds()));

			if(!pConnection)
			{
				if(RetryCount < g_Config.m_SvSqlDmlMaxRetries)
				{
					dbg_msg("SQL Worker", "Async DML connection unavailable. Retrying (attempt %d/%d). Query: %s", RetryCount + 1, g_Config.m_SvSqlDmlMaxRetries, pContext->Query().c_str());
					if(pThreadPool)
						pThreadPool->EnqueueWithRetry(CreateAsyncDmlTask(pContext, pThreadPool), pContext->Type(), pContext->Query(), RetryCount + 1);
					return;
				}

				LogLostQuery("async dml connection unavailable", pContext->Type(), pContext->Query());
				dbg_msg("SQL Error", "Async DML failed to create a connection. Query: %s", pContext->Query().c_str());
				NotifyDml(pContext->Callback(), false);
				return;
			}

			try
			{
				std::unique_ptr<Statement> pStmt(pConnection->createStatement());
				const bool hasResultSet = pStmt->execute(pContext->Query().c_str());
				const bool hasUpdated = !hasResultSet && pStmt->getUpdateCount() > 0;
				if(hasResultSet)
					DrainStatementResults(pStmt.get());

				NotifyDml(pContext->Callback(), hasUpdated);
			}
			catch(SQLException& e)
			{
				if(is_connection_lost(e))
				{
					CloseConnectionOnError(pConnection, "Async DML");
					if(RetryCount < g_Config.m_SvSqlDmlMaxRetries)
					{
						if(pThreadPool)
							pThreadPool->EnqueueWithRetry(CreateAsyncDmlTask(pContext, pThreadPool), pContext->Type(), pContext->Query(), RetryCount + 1);
						return;
					}
					LogLostQuery("async dml retries exhausted", pContext->Type(), pContext->Query());
				}
				else
				{
					LogLostQuery("async dml failed", pContext->Type(), pContext->Query());
				}

				dbg_msg("SQL Error", "Async DML failed: %s. Query: %s", e.what(), pContext->Query().c_str());
				NotifyDml(pContext->Callback(), false);
			}
		};
	}
}

CThreadPool::CThreadPool(size_t numThreads, bool Local) : m_bStop(false), m_Local(Local)
{
	for(size_t i = 0; i < numThreads; ++i)
	{
		m_vWorkers.emplace_back([this] { this->WorkerThread(); });
	}
}

CThreadPool::~CThreadPool()
{
	Stop();
}

void CThreadPool::Stop()
{
	// Set the stop flag to signal threads to exit their loops.
	m_bStop.store(true);

	// Notify all waiting threads to wake them up.
	m_cvCondition.notify_all();

	// Join all threads to ensure they finish cleanly. A second call finds nothing to join.
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
	const size_t maxQueueSize = static_cast<size_t>(g_Config.m_SvSqlQueueMaxSize);
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
				if(m_cvCondition.wait_for(lock, std::chrono::milliseconds(g_Config.m_SvSqlQueueWaitTimeoutMs)) == std::cv_status::timeout)
				{
					dbg_msg("SQL Worker", "Queue enqueue waited %dms and is still full (type: %s). Continuing to wait.", g_Config.m_SvSqlQueueWaitTimeoutMs, DbTypeName(task.m_Type));
				}
			}
		}

		if(m_bStop.load())
			return;

		m_qTasks.emplace(std::move(task));
		queuedSize = m_QueueSize.fetch_add(1) + 1;
	}

	if(queuedSize > static_cast<size_t>(g_Config.m_SvSqlQueueWarnSize))
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
		LogLostQuery(reason, task.m_Type, task.m_Query);
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
	std::unique_ptr<Connection> pConnection = CConectionPool::CreateConnection(m_Local);

	// The main loop for the worker thread.
	while(true)
	{
		CTask task;
		bool hasTask = false;

		// Lock the queue to safely access it.
		{
			std::unique_lock<std::mutex> lock(m_mxQueue);

			// Wait until there's a task or the pool is stopping.
			// The lambda prevents spurious wakeups.
			m_cvCondition.wait_for(lock, std::chrono::seconds(60), [this]{ return m_bStop || !m_qTasks.empty(); });

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
					pConnection = CConectionPool::CreateConnection(m_Local);
				}

				if(pConnection)
				{
					// Execute the actual task (a lambda containing the query logic).
					// We pass the raw connection pointer to the task.
					const int64_t startWait = task.m_EnqueueTime;
					const double waitMs = DurationMs(startWait, time_get());
					if(waitMs > static_cast<double>(g_Config.m_SvSqlQueueWaitWarnMs))
					{
						dbg_msg("SQL Worker", "Task waited %.2fms in queue (type: %s). Query: %s", waitMs, DbTypeName(task.m_Type), task.m_Query.c_str());
					}
					const int64_t execStart = time_get();
					task.m_Task(pConnection.get(), task.m_RetryCount);
					const double execMs = DurationMs(execStart, time_get());
					if(execMs > static_cast<double>(g_Config.m_SvSqlQueueWaitWarnMs))
					{
						// Distinguish "queue was full" (wait log above) from
						// "the query itself was slow" in production logs.
						dbg_msg("SQL Worker", "Task executed in %.2fms (type: %s). Query: %s", execMs, DbTypeName(task.m_Type), task.m_Query.c_str());
					}
				}
				else
				{
					EnqueueRetry(std::move(task), "Worker has no valid connection", MaxRetriesForType(task.m_Type));
				}
			}
			catch(SQLException& e)
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
				LogLostQuery("worker caught std::exception, task dropped", task.m_Type, task.m_Query);
			}
		}
		// NOTE: an idle timeout must NOT close the worker connection.
		// Reconnecting goes through the bridge to the remote primary
		// (auth + Connector/C++ init: SET + SELECT @@... round trips over
		// the WAN, ~100ms each), and that cost is paid by the first task
		// of the next burst while every task queued behind it ages --
		// this was the main source of ">1s in queue" warnings after a
		// quiet minute. A connection that really died is detected by
		// isClosed() above (and by the task retry logic) and only then
		// re-created.
	}
}


// #####################################################
// SQL CONNECTION POOL (QUERY MANAGER) IMPLEMENTATION
// #####################################################
CConectionPool::CConectionPool()
{
	try
	{
		m_pDriver = mariadb::get_driver_instance();

		// the same name twice (or no local name) means one database with all tables
		if(str_comp(g_Config.m_SvMySqlDatabaseLocal, g_Config.m_SvMySqlDatabaseShared) != 0)
			m_LocalDatabase = g_Config.m_SvMySqlDatabaseLocal;

		const int ThreadCount = std::max(2, g_Config.m_SvMySqlPoolSize);
		if(m_LocalDatabase.empty())
		{
			m_SharedThreads = ThreadCount;
		}
		else
		{
			// The pool is divided between the two databases. The shared one has most
			// of the tables and is used more often, so it gets the larger part.
			m_LocalThreads = std::max(1, ThreadCount / 3);
			m_SharedThreads = std::max(1, ThreadCount - m_LocalThreads);
			m_pLocalThreadPool = std::make_unique<CThreadPool>(m_LocalThreads, true);
		}
		m_pThreadPool = std::make_unique<CThreadPool>(m_SharedThreads);
	}
	catch(const SQLException& e)
	{
		dbg_msg("Sql Exception", "Failed to initialize SQL Driver: %s", e.what());
		throw;
	}
}

CConectionPool::~CConectionPool()
{
	// A callback that runs while one pool finishes its queue may still add a query
	// for the other pool, so both are stopped before any of them is destroyed.
	if(m_pThreadPool)
		m_pThreadPool->Stop();
	if(m_pLocalThreadPool)
		m_pLocalThreadPool->Stop();
}

std::unique_ptr<Connection> CConectionPool::CreateConnection(bool Local)
{
	// both databases use the same server and account, only the name differs
	const char* pDatabaseName = Local ? g_Config.m_SvMySqlDatabaseLocal : g_Config.m_SvMySqlDatabaseShared;

	// one entry per database: a wrong name of one of them does not delay the connections to the other one
	static std::atomic<int64_t> s_aLastConnectFailure[2];
	std::atomic<int64_t>& LastConnectFailure = s_aLastConnectFailure[Local ? 1 : 0];
	const int64_t now = time_get();
	const int64_t lastFailure = LastConnectFailure.load();
	if(lastFailure > 0 && DurationMs(lastFailure, now) < g_Config.m_SvSqlConnectRetryDelayMs)
		return nullptr;

	try
	{
		Driver* pDriver = mariadb::get_driver_instance();

		std::string Hostname(g_Config.m_SvMySqlHost);
		Hostname.append(":" + std::to_string(g_Config.m_SvMySqlPort));

		// Set connection options
		ConnectOptionsMap connection_properties;

		connection_properties["hostName"] = Hostname;
		connection_properties["user"] = g_Config.m_SvMySqlLogin;
		connection_properties["password"] = g_Config.m_SvMySqlPassword;
		connection_properties["schema"] = pDatabaseName;

		// connection_properties["OPT_CHARSET_NAME"] = "utf8mb4"; // didn't find this one https://mariadb.com/docs/connectors/mariadb-connector-cpp/connect-with-mariadb-connectorcpp#optional-connection-parameters
		connection_properties["autoReconnect"] = "true";
		// MariaDB Connector/C++ expects milliseconds, not seconds. The previous
		// values (30/60) became roughly one second in Connector/C, so a bridge
		// waiting for a remote primary could time out during the handshake.
		connection_properties["connectTimeout"] = "30000";
		connection_properties["socketTimeout"] = "60000";
		connection_properties["useTls"] = g_Config.m_SvMySqlUseTls ? "true" : "false";

		auto pConnection = std::unique_ptr<Connection>(pDriver->connect(connection_properties));

		LastConnectFailure.store(0);
		return pConnection;
	}
	catch(const SQLException& e)
	{
		LastConnectFailure.store(now);
		dbg_msg("Sql Exception", "Failed to create new SQL connection to the database '%s': %s", pDatabaseName, e.what());
		return nullptr;
	}
}

bool CConectionPool::IsLocalTable(std::string_view Table) const
{
	return !m_LocalDatabase.empty() && m_SharedTablesLoaded.load() && !sqlroute::HasName(m_vSharedTables, Table);
}

bool CConectionPool::IsLocalQuery(const std::string& Query) const
{
	// one database, or the structure of the shared one is not read yet (InitDatabases)
	if(m_LocalDatabase.empty() || !m_SharedTablesLoaded.load())
		return false;

	// the shared database does not have the table of the query: it is a table of the local database
	return !sqlroute::UsesTableOf(Query, m_vSharedTables);
}

CThreadPool* CConectionPool::GetThreadPool(bool Local) const
{
	return Local ? m_pLocalThreadPool.get() : m_pThreadPool.get();
}

bool CConectionPool::InitDatabases()
{
	const bool TwoDatabases = !m_LocalDatabase.empty();
	const char* pShared = g_Config.m_SvMySqlDatabaseShared;

	// structure of the shared database: its tables choose the database of every query
	std::vector<std::string> vSharedTables;
	auto& pSharedConnection = GetSyncConnection(false);
	if(!pSharedConnection || !ListTables(pSharedConnection.get(), vSharedTables))
	{
		dbg_msg("SQL Error", "The shared database '%s' is unavailable (%s:%d): check sv_sql_host, sv_sql_port, sv_sql_login, sv_sql_password and sv_sql_database_shared.",
			pShared, g_Config.m_SvMySqlHost, g_Config.m_SvMySqlPort);
		return false;
	}

	if(!m_SharedTablesLoaded.load())
	{
		m_vSharedTables = vSharedTables;
		m_SharedTablesLoaded.store(true);
	}

	// the local database holds the tables that the shared one does not have
	std::vector<std::string> vLocalTables;
	if(TwoDatabases)
	{
		auto& pLocalConnection = GetSyncConnection(true);
		if(!pLocalConnection || !ListTables(pLocalConnection.get(), vLocalTables))
		{
			dbg_msg("SQL Error", "The local database '%s' is unavailable: check sv_sql_database_local and the privileges of '%s'.", m_LocalDatabase.c_str(), g_Config.m_SvMySqlLogin);
			return false;
		}
	}

	// A table in both databases: the queries would use the one of the shared
	// database and the rows of the local one would be ignored.
	std::string Both;
	for(const std::string& Name : vLocalTables)
	{
		if(sqlroute::HasName(vSharedTables, Name))
			Both.append(Both.empty() ? "" : ", ").append(Name);
	}

	if(!Both.empty())
	{
		dbg_msg("SQL Error", "Tables found in both databases '%s' and '%s': %s. Keep every table in one database only.", pShared, m_LocalDatabase.c_str(), Both.c_str());
		return false;
	}

	// Without these tables the server would start with no guilds and no house
	// owners, so it must not start at all.
	std::string Missing;
	for(const auto& Table : sqlroute::LOCAL_DUMP_TABLES)
	{
		if(!sqlroute::HasName(vSharedTables, Table) && !sqlroute::HasName(vLocalTables, Table))
			Missing.append(Missing.empty() ? "" : ", ").append(Table);
	}

	if(!Missing.empty())
	{
		if(TwoDatabases)
		{
			dbg_msg("SQL Error", "Tables found neither in the shared database '%s' nor in the local database '%s': %s. Import mrpg_local.sql into the local database.", pShared, m_LocalDatabase.c_str(), Missing.c_str());
		}
		else
		{
			dbg_msg("SQL Error", "The database '%s' has no tables: %s. Import mrpg_local.sql into it, or set sv_sql_database_local if they are stored in another database.", pShared, Missing.c_str());
		}
		return false;
	}

	if(!TwoDatabases)
	{
		dbg_msg("SQL", "One database '%s' (%s:%d): %d connections.", pShared, g_Config.m_SvMySqlHost, g_Config.m_SvMySqlPort, m_SharedThreads);
		return true;
	}

	dbg_msg("SQL", "Shared database '%s', local database '%s' (%s:%d).", pShared, m_LocalDatabase.c_str(), g_Config.m_SvMySqlHost, g_Config.m_SvMySqlPort);
	dbg_msg("SQL", "The pool is divided: %d connections to the shared database (%d tables), %d to the local one (%d tables).",
		m_SharedThreads, static_cast<int>(vSharedTables.size()), m_LocalThreads, static_cast<int>(vLocalTables.size()));
	return true;
}

// #####################################################
// QUERY EXECUTION METHOD IMPLEMENTATIONS
// #####################################################

// --- SYNCHRONOUS SELECT ---
[[nodiscard]] ResultPtr CConectionPool::CResultSelect::Execute() const
{
	// Synchronous queries run on the calling thread and reuse a thread-local
	// connection to the database of the query.
	const bool Local = Database->IsLocalQuery(m_Query);
	auto& pConnection = GetSyncConnection(Local);
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
		if(durationMs > static_cast<double>(g_Config.m_SvSqlSyncSelectWarnMs))
		{
			dbg_msg("SQL Warning", "Slow sync SELECT (%.2fms). Query: %s", durationMs, m_Query.c_str());
		}
		return result ? result : EmptyResult();
	}
	catch(SQLException& e)
	{
		const double durationMs = DurationMs(start, time_get());
		if(durationMs > static_cast<double>(g_Config.m_SvSqlSyncSelectWarnMs))
		{
			dbg_msg("SQL Warning", "Slow sync SELECT failed (%.2fms). Query: %s", durationMs, m_Query.c_str());
		}
		dbg_msg("SQL Error", "Sync SELECT failed (code=%d, SQLSTATE=%s): %s. Query: %s", e.getErrorCode(), e.getSQLStateCStr() ? e.getSQLStateCStr() : "unknown", e.what(), m_Query.c_str());
		if(is_connection_lost(e))
		{
			CloseConnectionOnError(pConnection.get(), "Sync SELECT");
			pConnection = nullptr;
			for(int attempt = 0; attempt < g_Config.m_SvSqlSelectMaxRetries; ++attempt)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(g_Config.m_SvSqlRetryBaseDelayMs * (attempt + 1)));
				auto& retryConnection = GetSyncConnection(Local);
				if(!retryConnection)
					continue;

				try
				{
					return runQuery(retryConnection.get());
				}
				catch(SQLException& retryError)
				{
					dbg_msg("SQL Error", "Sync SELECT retry failed (code=%d, SQLSTATE=%s): %s. Query: %s", retryError.getErrorCode(), retryError.getSQLStateCStr() ? retryError.getSQLStateCStr() : "unknown", retryError.what(), m_Query.c_str());
					if(!is_connection_lost(retryError))
						break;
					CloseConnectionOnError(retryConnection.get(), "Sync SELECT retry");
					retryConnection = nullptr;
				}
			}
			LogLostQuery("sync select retries exhausted", DB::SELECT, m_Query);
		}
		return EmptyResult();
	}
}

// --- ASYNCHRONOUS SELECT ---
void CConectionPool::CResultSelect::AtExecute(CallbackResultPtr pCallbackResult)
{
	auto pContext = std::make_shared<CAsyncSelectContext>(m_Query, DB::SELECT, std::move(pCallbackResult));
	CThreadPool* pThreadPool = Database->GetThreadPool(Database->IsLocalQuery(m_Query));
	if(!pThreadPool)
	{
		dbg_msg("SQL Error", "Async SELECT failed to enqueue: thread pool unavailable. Query: %s", pContext->Query().c_str());
		NotifySelect(pContext->Callback(), EmptyResult());
		return;
	}
	pThreadPool->Enqueue(CreateAsyncSelectTask(pContext, pThreadPool), pContext->Type(), pContext->Query());
}


// --- ASYNCHRONOUS DML (INSERT/UPDATE/DELETE) ---
void CConectionPool::CResultQuery::AtExecute(CallbackUpdatePtr pCallbackResult, int DelayMilliseconds)
{
	auto pContext = std::make_shared<CAsyncDmlContext>(m_Query, m_TypeQuery, std::move(pCallbackResult), DelayMilliseconds);
	CThreadPool* pThreadPool = Database->GetThreadPool(Database->IsLocalQuery(m_Query));
	if(!pThreadPool)
	{
		dbg_msg("SQL Error", "Async DML failed to enqueue: thread pool unavailable. Query: %s", pContext->Query().c_str());
		NotifyDml(pContext->Callback(), false);
		return;
	}
	pThreadPool->Enqueue(CreateAsyncDmlTask(pContext, pThreadPool), pContext->Type(), pContext->Query());
}
