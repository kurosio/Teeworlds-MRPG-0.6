#include <base/system.h>
#include "sql_connect_pool.h"
#include "sql_lost_query_logger.h"

#include <mutex>

namespace
{
	std::mutex s_CallbackMutex;
	std::queue<std::function<void()>> s_Callbacks;

	constexpr std::string_view LOCAL_TABLES[] = {
		"tw_groups",
		"tw_guilds_decorations",
		"tw_guilds_history",
		"tw_guilds_invites",
		"tw_houses_decorations",
		"tw_links",
	};

	char ToLower(char c)
	{
		return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
	}

	bool IsSameName(std::string_view a, std::string_view b)
	{
		if(a.size() != b.size())
			return false;
		for(size_t i = 0; i < a.size(); ++i)
		{
			if(ToLower(a[i]) != b[i])
				return false;
		}
		return true;
	}

	bool IsLocalTableName(std::string_view Table)
	{
		for(const std::string_view Name : LOCAL_TABLES)
		{
			if(IsSameName(Table, Name))
				return true;
		}
		return false;
	}

	// First identifier of a table argument: "tw_accounts_data d JOIN ..." -> "tw_accounts_data".
	std::string_view FirstTableName(const char* pTable)
	{
		std::string_view Text(pTable ? pTable : "");
		size_t i = 0;
		while(i < Text.size() && (Text[i] == ' ' || Text[i] == '\t' || Text[i] == '`'))
			++i;
		const size_t Start = i;
		while(i < Text.size() && (Text[i] != ' ' && Text[i] != '\t' && Text[i] != '`' && Text[i] != '.' && Text[i] != ';'))
			++i;
		return Text.substr(Start, i - Start);
	}
}

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
	std::unique_ptr<Connection>& GetSyncConnection(DbKind Kind)
	{
		thread_local std::unique_ptr<Connection> s_apSyncConnections[2];
		std::unique_ptr<Connection>& pSyncConnection = s_apSyncConnections[Kind == DbKind::Local ? 1 : 0];
		if(!pSyncConnection || pSyncConnection->isClosed())
		{
			pSyncConnection = CConectionPool::CreateConnection(Kind);
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
			{
				std::string Name = pResult->getString(1).c_str();
				for(char& c : Name)
					c = ToLower(c);
				vTables.emplace_back(std::move(Name));
			}
			return true;
		}
		catch(SQLException& e)
		{
			dbg_msg("SQL Error", "Failed to read the list of tables: %s", e.what());
			return false;
		}
	}

	bool HasTable(const std::vector<std::string>& vTables, std::string_view Table)
	{
		for(const std::string& Name : vTables)
		{
			if(IsSameName(Name, Table))
				return true;
		}
		return false;
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

		pResult->Detach();
		CConectionPool::PostCallback([Callback, pResult = std::move(pResult)] { Callback(pResult); });
	}

	void NotifyDml(const CallbackUpdatePtr& Callback, bool Updated)
	{
		if(!Callback)
			return;

		CConectionPool::PostCallback([Callback, Updated] { Callback(Updated); });
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

void CConectionPool::PostCallback(std::function<void()> Callback)
{
	std::lock_guard<std::mutex> Lock(s_CallbackMutex);
	s_Callbacks.push(std::move(Callback));
}

void CConectionPool::ProcessCallbacks()
{
	std::queue<std::function<void()>> Callbacks;
	{
		std::lock_guard<std::mutex> Lock(s_CallbackMutex);
		Callbacks.swap(s_Callbacks);
	}

	while(!Callbacks.empty())
	{
		try
		{
			Callbacks.front()();
		}
		catch(const std::exception& e)
		{
			dbg_msg("SQL Callback", "Failed to apply query result: %s", e.what());
		}

		Callbacks.pop();
	}
}

CThreadPool::CThreadPool(DbKind Kind, int NumThreads) : m_bStop(false), m_Kind(Kind)
{
	for(int i = 0; i < NumThreads; ++i)
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
	std::unique_ptr<Connection> pConnection = CConectionPool::CreateConnection(m_Kind);

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
					pConnection = CConectionPool::CreateConnection(m_Kind);
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

		DbSettings& Shared = m_aSettings[0];
		Shared.m_HostPort = std::string(g_Config.m_SvMySqlHost) + ":" + std::to_string(g_Config.m_SvMySqlPort);
		Shared.m_Login = g_Config.m_SvMySqlLogin;
		Shared.m_Password = g_Config.m_SvMySqlPassword;
		Shared.m_Database = g_Config.m_SvMySqlDatabaseShared;
		Shared.m_PoolSize = std::max(1, g_Config.m_SvMySqlPoolSize);
		Shared.m_UseTls = g_Config.m_SvMySqlUseTls != 0;

		// The local database is optional. Without a separate host it uses the
		// shared endpoint (one MariaDB server for both databases).
		m_bHasLocal = g_Config.m_SvMySqlLocalDatabase[0] != '\0';
		if(m_bHasLocal)
		{
			DbSettings& Local = m_aSettings[1];
			if(g_Config.m_SvMySqlLocalHost[0] != '\0')
			{
				Local.m_HostPort = std::string(g_Config.m_SvMySqlLocalHost) + ":" + std::to_string(g_Config.m_SvMySqlLocalPort);
				Local.m_Login = g_Config.m_SvMySqlLocalLogin;
				Local.m_Password = g_Config.m_SvMySqlLocalPassword;
				Local.m_UseTls = g_Config.m_SvMySqlLocalUseTls != 0;
			}
			else
			{
				Local.m_HostPort = Shared.m_HostPort;
				Local.m_Login = Shared.m_Login;
				Local.m_Password = Shared.m_Password;
				Local.m_UseTls = Shared.m_UseTls;
			}
			Local.m_Database = g_Config.m_SvMySqlLocalDatabase;
			Local.m_PoolSize = std::max(1, g_Config.m_SvMySqlLocalPoolSize);
			m_apThreadPools[1] = std::make_unique<CThreadPool>(DbKind::Local, Local.m_PoolSize);
		}
		m_apThreadPools[0] = std::make_unique<CThreadPool>(DbKind::Shared, Shared.m_PoolSize);
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
	for(auto& pPool : m_apThreadPools)
	{
		if(pPool)
			pPool->Stop();
	}
}

std::unique_ptr<Connection> CConectionPool::CreateConnection(DbKind Kind)
{
	const DbSettings& Settings = GetInstance()->m_aSettings[Kind == DbKind::Local ? 1 : 0];

	// one entry per database: a wrong name of one of them does not delay the connections to the other one
	static std::atomic<int64_t> s_aLastConnectFailure[2];
	std::atomic<int64_t>& LastConnectFailure = s_aLastConnectFailure[Kind == DbKind::Local ? 1 : 0];
	const int64_t now = time_get();
	const int64_t lastFailure = LastConnectFailure.load();
	if(lastFailure > 0 && DurationMs(lastFailure, now) < g_Config.m_SvSqlConnectRetryDelayMs)
		return nullptr;

	// MariaDB Connector/C++ 1.1.4 has shared state in its URL parser.
	// Worker threads that call connect() concurrently corrupt it and crash
	// inside UrlParser::parseHaMode(), so connector creation is serialized.
	// Queries still run in parallel after the connection exists.
	static std::mutex s_ConnectionMutex;
	std::lock_guard<std::mutex> Lock(s_ConnectionMutex);

	try
	{
		Driver* pDriver = mariadb::get_driver_instance();

		// Set connection options
		ConnectOptionsMap connection_properties;

		connection_properties["hostName"] = Settings.m_HostPort;
		connection_properties["user"] = Settings.m_Login;
		connection_properties["password"] = Settings.m_Password;
		connection_properties["schema"] = Settings.m_Database;

		// connection_properties["OPT_CHARSET_NAME"] = "utf8mb4"; // didn't find this one https://mariadb.com/docs/connectors/mariadb-connectorcpp/connect-with-mariadb-connectorcpp#optional-connection-parameters
		connection_properties["autoReconnect"] = "true";
		// MariaDB Connector/C++ expects milliseconds, not seconds. The previous
		// values (30/60) became roughly one second in Connector/C, so a bridge
		// waiting for a remote primary could time out during the handshake.
		connection_properties["connectTimeout"] = "30000";
		connection_properties["socketTimeout"] = "60000";
		connection_properties["useTls"] = Settings.m_UseTls ? "true" : "false";

		auto pConnection = std::unique_ptr<Connection>(pDriver->connect(connection_properties));

		LastConnectFailure.store(0);
		return pConnection;
	}
	catch(const SQLException& e)
	{
		LastConnectFailure.store(now);
		dbg_msg("Sql Exception", "Failed to create new SQL connection to the database '%s': %s", Settings.m_Database.c_str(), e.what());
		return nullptr;
	}
}

bool CConectionPool::IsLocalTable(std::string_view Table) const
{
	return m_bHasLocal && IsLocalTableName(Table);
}

DbKind CConectionPool::KindOfTable(const char* pTable) const
{
	return IsLocalTable(FirstTableName(pTable)) ? DbKind::Local : DbKind::Shared;
}

CThreadPool* CConectionPool::GetThreadPool(DbKind Kind) const
{
	return m_apThreadPools[Kind == DbKind::Local ? 1 : 0].get();
}

bool CConectionPool::InitDatabases()
{
	const DbSettings& Shared = m_aSettings[0];

	// structure of the shared database: a local table must not be there
	std::vector<std::string> vSharedTables;
	auto& pSharedConnection = GetSyncConnection(DbKind::Shared);
	if(!pSharedConnection || !ListTables(pSharedConnection.get(), vSharedTables))
	{
		dbg_msg("SQL Error", "The shared database '%s' is unavailable (%s): check sv_sql_host, sv_sql_port, sv_sql_login, sv_sql_password and sv_sql_database_shared.", Shared.m_Database.c_str(), Shared.m_HostPort.c_str());
		return false;
	}

	std::string InShared;
	for(const std::string_view Table : LOCAL_TABLES)
	{
		if(HasTable(vSharedTables, Table))
			InShared.append(InShared.empty() ? "" : ", ").append(Table);
	}

	if(!InShared.empty())
	{
		dbg_msg("SQL Error", "Local tables found in the shared database '%s': %s. Keep every table in one database only.", Shared.m_Database.c_str(), InShared.c_str());
		return false;
	}

	if(!m_bHasLocal)
	{
		dbg_msg("SQL", "One database '%s' (%s): %d connections.", Shared.m_Database.c_str(), Shared.m_HostPort.c_str(), Shared.m_PoolSize);
		return true;
	}

	// structure of the local database: every local table must be there
	const DbSettings& Local = m_aSettings[1];
	std::vector<std::string> vLocalTables;
	auto& pLocalConnection = GetSyncConnection(DbKind::Local);
	if(!pLocalConnection || !ListTables(pLocalConnection.get(), vLocalTables))
	{
		dbg_msg("SQL Error", "The local database '%s' is unavailable (%s): check sv_sql_local_database and the privileges of '%s'.", Local.m_Database.c_str(), Local.m_HostPort.c_str(), Local.m_Login.c_str());
		return false;
	}

	std::string Missing;
	for(const std::string_view Table : LOCAL_TABLES)
	{
		if(!HasTable(vLocalTables, Table))
			Missing.append(Missing.empty() ? "" : ", ").append(Table);
	}

	if(!Missing.empty())
	{
		dbg_msg("SQL Error", "Tables not found in the local database '%s': %s. Import mrpg_local.sql into it.", Local.m_Database.c_str(), Missing.c_str());
		return false;
	}

	dbg_msg("SQL", "Shared database '%s' (%s, %d connections), local database '%s' (%s, %d connections).",
		Shared.m_Database.c_str(), Shared.m_HostPort.c_str(), Shared.m_PoolSize,
		Local.m_Database.c_str(), Local.m_HostPort.c_str(), Local.m_PoolSize);
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
	auto& pConnection = GetSyncConnection(m_Kind);
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
				auto& retryConnection = GetSyncConnection(m_Kind);
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
	CThreadPool* pThreadPool = Database->GetThreadPool(m_Kind);
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
	CThreadPool* pThreadPool = Database->GetThreadPool(m_Kind);
	if(!pThreadPool)
	{
		dbg_msg("SQL Error", "Async DML failed to enqueue: thread pool unavailable. Query: %s", pContext->Query().c_str());
		NotifyDml(pContext->Callback(), false);
		return;
	}
	pThreadPool->Enqueue(CreateAsyncDmlTask(pContext, pThreadPool), pContext->Type(), pContext->Query());
}
