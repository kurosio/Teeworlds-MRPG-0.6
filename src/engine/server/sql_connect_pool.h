#ifndef ENGINE_SERVER_SQL_CONNECT_POOL_H
#define ENGINE_SERVER_SQL_CONNECT_POOL_H

// SECTION: Includes and Basic Setup
// =================================================================

#include <mariadb/conncpp/Driver.hpp>
#include <mariadb/conncpp/Exception.hpp>
#include <mariadb/conncpp/ResultSet.hpp>
#include <mariadb/conncpp/ResultSetMetaData.hpp>
#include <mariadb/conncpp/Statement.hpp>

#include <atomic>
#include <condition_variable>
#include <cstdarg>
#include <cstring>
#include <memory>
#include <queue>
#include <string>
#include <string_view>
#include <vector>

using namespace sql;
class CConectionPool; // Forward declare


// SECTION: Enums and Helper Functions
// =================================================================

enum class DB
{
	SELECT = 0,
	INSERT,
	UPDATE,
	REMOVE,
};

// Which of the two connections a query uses.
enum class DbKind
{
	Shared = 0,
	Local = 1,
};

// Connection settings of one database.
struct DbSettings
{
	std::string m_HostPort; // "host:port"
	std::string m_Login;
	std::string m_Password;
	std::string m_Database;
	int m_PoolSize = 1;
	bool m_UseTls = false;
};

// Helper to check for lost connection errors by error code or SQLSTATE.
// SQLSTATE '08xxx' indicates a connection exception.
inline bool is_connection_lost(SQLException& e)
{
	const int code = e.getErrorCode();
	const char* state = e.getSQLStateCStr();
	return code == 2006 || code == 2013 || code == 2014 || (state && std::strncmp(state, "08", 2) == 0);
}


// SECTION: WrapperResultSet Class
// =================================================================

// Synchronous results own their connector Statement/ResultSet. Async results are
// detached on the worker and own only copied values when delivered to the game.
class WrapperResultSet
{
public:
	WrapperResultSet(std::unique_ptr<Statement> stmt, std::unique_ptr<ResultSet> res, std::shared_ptr<Connection> connection = nullptr)
		: m_pStmt(std::move(stmt))
		, m_pResult(std::move(res))
		, m_pConnection(std::move(connection))
	{
	}

	// Delete copy semantics, allow move
	WrapperResultSet(const WrapperResultSet&) = delete;
	WrapperResultSet& operator=(const WrapperResultSet&) = delete;
	WrapperResultSet(WrapperResultSet&&) = default;
	WrapperResultSet& operator=(WrapperResultSet&&) = default;
	explicit operator bool() const { return m_Detached || m_pResult != nullptr; }

	// Copy on the SQL worker before its connection is reused. No connector objects
	// cross into the game thread (even a buffered ResultSet owns a Statement).
	void Detach()
	{
		if(!m_pResult)
			return;

		auto* pMeta = m_pResult->getMetaData();
		while(m_pResult->next())
		{
			auto& Row = m_Rows.emplace_back();
			for(unsigned int i = 1; i <= pMeta->getColumnCount(); ++i)
				Row.emplace_back(pMeta->getColumnLabel(i).c_str(), m_pResult->getString(i).c_str());
		}
		m_Detached = true;
		m_pResult.reset();
		m_pStmt.reset();
		m_pConnection.reset();
	}

	// --- Full accessor implementations ---
	bool getBoolean(const SQLString& column) const 
	{ 
		return m_Detached ? std::strtoll(Value(column).c_str(), nullptr, 10) != 0 : m_pResult ? m_pResult->getBoolean(column) : false; 
	}
	int getInt(const SQLString& column) const 
	{
		return m_Detached ? static_cast<int>(std::strtoll(Value(column).c_str(), nullptr, 10)) : m_pResult ? m_pResult->getInt(column) : 0; 
	}
	unsigned int getUInt(const SQLString& column) const 
	{
		return m_Detached ? static_cast<unsigned int>(std::strtoull(Value(column).c_str(), nullptr, 10)) : m_pResult ? m_pResult->getUInt(column) : 0; 
	}
	int64_t getInt64(const SQLString& column) const 
	{ 
		return m_Detached ? std::strtoll(Value(column).c_str(), nullptr, 10) : m_pResult ? m_pResult->getInt64(column) : 0; 
	}
	uint64_t getUInt64(const SQLString& column) const 
	{ 
		return m_Detached ? std::strtoull(Value(column).c_str(), nullptr, 10) : m_pResult ? m_pResult->getUInt64(column) : 0; 
	}
	double getDouble(const SQLString& column) const 
	{ 
		return m_Detached ? std::strtod(Value(column).c_str(), nullptr) : m_pResult ? m_pResult->getDouble(column) : 0.0; 
	}
	float getFloat(const SQLString& column) const 
	{ 
		return m_Detached ? static_cast<float>(std::strtod(Value(column).c_str(), nullptr)) : m_pResult ? static_cast<float>(m_pResult->getDouble(column)) : 0.0f; 
	}
	std::string getString(const SQLString& column) const 
	{ 
		return m_Detached ? Value(column) : m_pResult ? std::string(m_pResult->getString(column).c_str()) : ""; 
	}
	std::string getDateTime(const SQLString& column) const 
	{ 
		return m_Detached ? Value(column) : m_pResult ? std::string(m_pResult->getString(column).c_str()) : ""; 
	}
	bool next() const 
	{
		return m_Detached ? (++m_Row <= m_Rows.size()) : m_pResult && m_pResult->next(); 
	}
	size_t rowsCount() const 
	{
		return m_Detached ? m_Rows.size() : m_pResult ? m_pResult->rowsCount() : 0; 
	}
	size_t getRow() const 
	{ 
		return m_Detached ? m_Row : m_pResult ? m_pResult->getRow() : 0; 
	}

	nlohmann::json getJson(const SQLString& column) const
	{
		if(!*this)
			return nullptr;

		const std::string jsonString = getString(column).c_str();
		if(jsonString.empty())
			return nullptr;

		try
		{
			return nlohmann::json::parse(jsonString);
		}
		catch(const nlohmann::json::parse_error& e)
		{
			[[unlikely]] {
				const auto errorMsg = fmt_default("JSON from DB for column '{}' failed to parse: {}", column.c_str(), e.what());
				dbg_assert(false, errorMsg.c_str());
				return nullptr;
			}
		}
		return nullptr;
	}

	BigInt getBigInt(const SQLString& column) const
	{
		if(!*this)
			return BigInt();

		const std::string stringValue = getString(column).c_str();
		if(stringValue.empty())
			return BigInt();

		try
		{
			return BigInt(stringValue);
		}
		catch(const SQLException& e)
		{
			[[unlikely]] {
				const auto errorMsg = fmt_default("Failed to convert column '{}' to BigInt: {}", column.c_str(), e.what());
				dbg_assert(false, errorMsg.c_str());
				return BigInt();
			}
		}
	}

private:
	const std::string& Value(const SQLString& Column) const
	{
		const auto& Row = m_Rows.at(m_Row - 1);
		for(const auto& [Name, Value] : Row)
			if(str_comp_nocase(Name.c_str(), Column.c_str()) == 0)
				return Value;

		throw std::out_of_range("Unknown SQL column");
	}

	bool m_Detached = false;
	mutable size_t m_Row = 0;
	std::vector<std::vector<std::pair<std::string, std::string>>> m_Rows;
	std::unique_ptr<Statement> m_pStmt;
	std::unique_ptr<ResultSet> m_pResult;
	std::shared_ptr<Connection> m_pConnection;
};


// SECTION: Type Aliases
// =================================================================

using ResultPtr = std::shared_ptr<WrapperResultSet>;
using CallbackResultPtr = std::function<void(ResultPtr)>;
using CallbackUpdatePtr = std::function<void(bool)>;


// SECTION: CThreadPool Class
// =================================================================

/**
 * @class CThreadPool
 * @brief Manages a pool of persistent worker threads for executing database tasks.
 *        One pool per database; every worker holds one connection to that database.
 */
class CThreadPool
{
public:
	CThreadPool(DbKind Kind, int NumThreads);
	~CThreadPool();

	CThreadPool(const CThreadPool&) = delete;
	CThreadPool& operator=(const CThreadPool&) = delete;

	// Enqueues a task to be executed by a worker thread.
	// The task must be a callable that accepts a `Connection*`.
	void Enqueue(std::function<void(Connection*, int)> task, DB type, std::string query);
	void EnqueueWithRetry(std::function<void(Connection*, int)> task, DB type, std::string query, int retryCount);

	// Finishes the queued tasks and joins the workers. Tasks enqueued later are dropped.
	void Stop();

private:
	struct CTask
	{
		std::function<void(Connection*, int)> m_Task;
		int64_t m_EnqueueTime;
		DB m_Type;
		std::string m_Query;
		int m_RetryCount;
	};

	void EnqueueTask(CTask&& task);
	void EnqueueRetry(CTask&& task, const char* reason, int maxRetries);
	void WorkerThread();

	std::vector<std::thread> m_vWorkers;
	std::queue<CTask> m_qTasks;
	std::mutex m_mxQueue;
	std::condition_variable m_cvCondition;
	std::atomic<bool> m_bStop;
	std::atomic<size_t> m_QueueSize { 0 };
	DbKind m_Kind;
};


// SECTION: CConectionPool (Query Manager) Class
// =================================================================

/**
 * @class CConectionPool
 * @brief Manages database query execution, dispatching async tasks to a thread pool.
 */
class CConectionPool
{
public:
	static CConectionPool* GetInstance()
	{
		static CConectionPool instance;
		return &instance;
	}

	CConectionPool(const CConectionPool&) = delete;
	CConectionPool& operator=(const CConectionPool&) = delete;
	static std::unique_ptr<Connection> CreateConnection(DbKind Kind);

	// Startup, before the first query: opens both connections and checks that
	// the local tables are in the local database and not in the shared one.
	// The server must not start if it fails.
	bool InitDatabases();

	// Async callbacks are applied only by the server tick, never by SQL workers.
	static void PostCallback(std::function<void()> Callback);
	static void ProcessCallbacks();

	// True if the table belongs to the local database (always false while
	// sv_sql_local_database is not set).
	bool IsLocalTable(std::string_view Table) const;

private:
	CConectionPool();
	~CConectionPool();

	// The table of the query chooses the connection: a local table uses the
	// local pool, every other table the shared one.
	DbKind KindOfTable(const char* pTable) const;
	CThreadPool* GetThreadPool(DbKind Kind) const;

	Driver* m_pDriver;
	DbSettings m_aSettings[2];
	bool m_bHasLocal = false;
	std::unique_ptr<CThreadPool> m_apThreadPools[2];

public:
	class CResultBase
	{
	protected:
		friend class CConectionPool;
		std::string m_Query;
		DB m_TypeQuery;
		DbKind m_Kind = DbKind::Shared;
	public:
		const char* GetQueryString() const { return m_Query.c_str(); }
	};

	class CResultSelect : public CResultBase
	{
	public:
		template<typename... Ts>
		CResultSelect& UpdateQuery(const char* pSelect, const char* pTable, const char* pBuffer, Ts&&... args)
		{
			std::string strQuery = fmt_default(pBuffer, std::forward<Ts>(args)...);
			m_Query = fmt_default("SELECT {} FROM {} {};", pSelect, pTable, strQuery);
			return *this;
		}

		[[nodiscard]] ResultPtr Execute() const;
		void AtExecute(CallbackResultPtr pCallbackResult);
	};

	class CResultQuery : public CResultBase
	{
	public:
		template <typename ... Ts>
		CResultQuery& UpdateQuery(const char* pTable, const char* pBuffer, Ts&&... args)
		{
			std::string strQuery = fmt_default(pBuffer, std::forward<Ts>(args)...);
			if(m_TypeQuery == DB::INSERT)
				m_Query = fmt_default("INSERT INTO {} {};", pTable, strQuery);
			else if(m_TypeQuery == DB::UPDATE)
				m_Query = fmt_default("UPDATE {} SET {};", pTable, strQuery);
			else if(m_TypeQuery == DB::REMOVE)
				m_Query = fmt_default("DELETE FROM {} {};", pTable, strQuery);
			return *this;
		}

		void AtExecute(CallbackUpdatePtr pCallbackResult, int DelayMilliseconds = 0);
		void Execute(CallbackUpdatePtr pCallbackResult, int DelayMilliseconds = 0) { AtExecute(std::move(pCallbackResult), DelayMilliseconds); }
		void Execute(int DelayMilliseconds = 0) { AtExecute(CallbackUpdatePtr(), DelayMilliseconds); }
	};

private:
	static std::unique_ptr<CResultSelect> PrepareQuerySelect(DB Type, const char* pSelect, const char* pTable, std::string strQuery)
	{
		auto Data = std::make_unique<CResultSelect>();
		Data->m_Query = "SELECT " + std::string(pSelect) + " FROM " + std::string(pTable) + " " + strQuery + ";";
		Data->m_TypeQuery = Type;
		Data->m_Kind = GetInstance()->KindOfTable(pTable);
		return Data;
	}

	static std::unique_ptr<CResultQuery> PrepareQueryInsertUpdateDelete(DB Type, const char* pTable, std::string strQuery)
	{
		auto Data = std::make_unique<CResultQuery>();
		Data->m_TypeQuery = Type;
		if(Type == DB::INSERT)
			Data->m_Query = "INSERT INTO " + std::string(pTable) + " " + strQuery + ";";
		else if(Type == DB::UPDATE)
			Data->m_Query = "UPDATE " + std::string(pTable) + " SET " + strQuery + ";";
		else if(Type == DB::REMOVE)
			Data->m_Query = "DELETE FROM " + std::string(pTable) + " " + strQuery + ";";
		Data->m_Kind = GetInstance()->KindOfTable(pTable);
		return Data;
	}

public:
	template<DB T>
	static std::enable_if_t<T == DB::SELECT, std::unique_ptr<CResultSelect>> Prepare(const char* pSelect, const char* pTable)
	{
		return PrepareQuerySelect(T, pSelect, pTable, "");
	}
	template<DB T, typename... Ts>
	static std::enable_if_t<T == DB::SELECT, std::unique_ptr<CResultSelect>> Prepare(const char* pSelect, const char* pTable, const char* pBuffer, Ts&&... args)
	{
		std::string strQuery = fmt_default(pBuffer, std::forward<Ts>(args)...);
		return PrepareQuerySelect(T, pSelect, pTable, std::move(strQuery));
	}

	template<DB T>
	static std::enable_if_t<T == DB::SELECT, ResultPtr> Execute(const char* pSelect, const char* pTable)
	{
		return PrepareQuerySelect(T, pSelect, pTable, "")->Execute();
	}
	template<DB T, typename... Ts>
	static std::enable_if_t<T == DB::SELECT, ResultPtr> Execute(const char* pSelect, const char* pTable, const char* pBuffer, Ts&&... args)
	{
		std::string strQuery = fmt_default(pBuffer, std::forward<Ts>(args)...);
		return PrepareQuerySelect(T, pSelect, pTable, std::move(strQuery))->Execute();
	}

	template<DB T, int Milliseconds = 0, typename... Ts>
	static std::enable_if_t<(T == DB::INSERT || T == DB::UPDATE || T == DB::REMOVE), void> Execute(const char* pTable, const char* pBuffer, Ts&&... args)
	{
		std::string strQuery = fmt_default(pBuffer, std::forward<Ts>(args)...);
		PrepareQueryInsertUpdateDelete(T, pTable, std::move(strQuery))->Execute(Milliseconds);
	}
	template<DB T, int Milliseconds = 0, typename... Ts>
	static std::enable_if_t<(T == DB::INSERT || T == DB::UPDATE || T == DB::REMOVE), void> Execute(CallbackUpdatePtr pCallbackResult, const char* pTable, const char* pBuffer, Ts&&... args)
	{
		std::string strQuery = fmt_default(pBuffer, std::forward<Ts>(args)...);
		PrepareQueryInsertUpdateDelete(T, pTable, std::move(strQuery))->AtExecute(std::move(pCallbackResult), Milliseconds);
	}
};

#define Database CConectionPool::GetInstance()

#endif // ENGINE_SERVER_SQL_CONNECT_POOL_H
