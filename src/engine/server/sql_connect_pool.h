#ifndef ENGINE_SERVER_SQL_CONNECT_POOL_H
#define ENGINE_SERVER_SQL_CONNECT_POOL_H

// SECTION: Includes and Basic Setup
// =================================================================

#if __cplusplus >= 201703L
	#define throw(...)
	#include <cppconn/driver.h>
	#include <cppconn/statement.h>
	#include <cppconn/resultset.h>
	#undef throw
#else
	#include <cppconn/driver.h>
	#include <cppconn/statement.h>
	#include <cppconn/resultset.h>
#endif

#include <cstdarg>
#include <queue>
#include <atomic>
#include <condition_variable>
#include <memory>

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
	OTHER,
};

// Helper to check for lost connection errors by error code or SQLSTATE.
// SQLSTATE '08xxx' indicates a connection exception.
inline bool is_connection_lost(const SQLException& e)
{
	const int code = e.getErrorCode();
	const char* state = e.getSQLStateCStr();
	return code == 2006 || code == 2013 || (state && std::strncmp(state, "08", 2) == 0);
}


// SECTION: WrapperResultSet Class
// =================================================================

// WrapperResultSet is a temporary wrapper that owns the Statement and ResultSet.
// Its lifetime is confined to the scope of a single query execution.
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
	explicit operator bool() const { return m_pResult != nullptr; }

	// --- Full accessor implementations ---
	bool getBoolean(const SQLString& column) const { return m_pResult ? m_pResult->getBoolean(column) : false; }
	int getInt(const SQLString& column) const { return m_pResult ? m_pResult->getInt(column) : 0; }
	unsigned int getUInt(const SQLString& column) const { return m_pResult ? m_pResult->getUInt(column) : 0; }
	int64_t getInt64(const SQLString& column) const { return m_pResult ? m_pResult->getInt64(column) : 0; }
	uint64_t getUInt64(const SQLString& column) const { return m_pResult ? m_pResult->getUInt64(column) : 0; }
	double getDouble(const SQLString& column) const { return m_pResult ? m_pResult->getDouble(column) : 0.0; }
	float getFloat(const SQLString& column) const { return m_pResult ? static_cast<float>(m_pResult->getDouble(column)) : 0.0f; }
	std::string getString(const SQLString& column) const { return m_pResult ? std::string(m_pResult->getString(column).c_str()) : ""; }
	std::string getDateTime(const SQLString& column) const { return m_pResult ? std::string(m_pResult->getString(column).c_str()) : ""; }
	bool next() const { return m_pResult && m_pResult->next(); }
	size_t rowsCount() const { return m_pResult ? m_pResult->rowsCount() : 0; }
	size_t getRow() const { return m_pResult ? m_pResult->getRow() : 0; }

	nlohmann::json getJson(const SQLString& column) const
	{
		if(!m_pResult)
			return nullptr;

		const std::string jsonString = m_pResult->getString(column).c_str();
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
		if(!m_pResult)
			return BigInt();

		const std::string stringValue = m_pResult->getString(column).c_str();
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
	std::unique_ptr<Statement> m_pStmt;
	std::unique_ptr<ResultSet> m_pResult;
	std::shared_ptr<Connection> m_pConnection;
};


// SECTION: Type Aliases
// =================================================================

using ResultPtr = std::shared_ptr<WrapperResultSet>;
using CallbackResultPtr = std::function<void(ResultPtr)>;
using CallbackUpdatePtr = std::function<void()>;


// SECTION: CThreadPool Class
// =================================================================

/**
 * @class CThreadPool
 * @brief Manages a pool of persistent worker threads for executing database tasks.
 */
class CThreadPool
{
public:
	CThreadPool(size_t numThreads);
	~CThreadPool();

	CThreadPool(const CThreadPool&) = delete;
	CThreadPool& operator=(const CThreadPool&) = delete;

	// Enqueues a task to be executed by a worker thread.
	// The task must be a callable that accepts a `Connection*`.
	void Enqueue(std::function<void(Connection*, int)> task, DB type, std::string query);
	void EnqueueWithRetry(std::function<void(Connection*, int)> task, DB type, std::string query, int retryCount);

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
	static std::unique_ptr<Connection> CreateConnection();

private:
	CConectionPool();
	~CConectionPool();

	Driver* m_pDriver;
	std::unique_ptr<CThreadPool> m_pThreadPool;

public:
	class CResultBase
	{
	protected:
		friend class CConectionPool;
		std::string m_Query;
		DB m_TypeQuery;
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
		void Execute(int DelayMilliseconds = 0) { AtExecute(nullptr, DelayMilliseconds); }
	};

	class CResultQueryCustom : public CResultQuery
	{
	public:
		template <typename... Ts>
		CResultQueryCustom& UpdateQuery(const char* pBuffer, Ts&&... args)
		{
			std::string strQuery = fmt_default(pBuffer, std::forward<Ts>(args)...);
			m_Query = strQuery + ";";
			return *this;
		}
	};

private:
	static std::unique_ptr<CResultSelect> PrepareQuerySelect(DB Type, const char* pSelect, const char* pTable, std::string strQuery)
	{
		auto Data = std::make_unique<CResultSelect>();
		Data->m_Query = "SELECT " + std::string(pSelect) + " FROM " + std::string(pTable) + " " + strQuery + ";";
		Data->m_TypeQuery = Type;
		return Data;
	}

	static std::unique_ptr<CResultQueryCustom> PrepareQueryCustom(DB Type, std::string strQuery)
	{
		auto Data = std::make_unique<CResultQueryCustom>();
		Data->m_Query = std::move(strQuery) + ";";
		Data->m_TypeQuery = Type;
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
	static std::enable_if_t<T == DB::OTHER, void> Execute(const char* pBuffer, Ts&&... args)
	{
		std::string strQuery = fmt_default(pBuffer, std::forward<Ts>(args)...);
		PrepareQueryCustom(T, std::move(strQuery))->Execute(Milliseconds);
	}

	template<DB T, int Milliseconds = 0, typename... Ts>
	static std::enable_if_t<(T == DB::INSERT || T == DB::UPDATE || T == DB::REMOVE), void> Execute(const char* pTable, const char* pBuffer, Ts&&... args)
	{
		std::string strQuery = fmt_default(pBuffer, std::forward<Ts>(args)...);
		PrepareQueryInsertUpdateDelete(T, pTable, std::move(strQuery))->Execute(Milliseconds);
	}
};

#define Database CConectionPool::GetInstance()

#endif // ENGINE_SERVER_SQL_CONNECT_POOL_H
