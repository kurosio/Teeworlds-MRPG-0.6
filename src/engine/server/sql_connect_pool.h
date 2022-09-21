#ifndef ENGINE_SERVER_SQL_CONNECT_POOL_H
#define ENGINE_SERVER_SQL_CONNECT_POOL_H

#include <cppconn/driver.h>
#include <cppconn/statement.h>
#include <cppconn/resultset.h>

#include <cstdarg>
#include <thread>
#include <mutex>

using namespace sql;
#define Sqlpool CConectionPool::GetInstance()
typedef std::unique_ptr<ResultSet> ResultPtr;
inline std::recursive_mutex g_SqlThreadRecursiveLock;

#define MAX_QUERY_LEN 2048

enum class DB
{
	SELECT = 0,
	INSERT,
	UPDATE,
	REMOVE,
	OTHER,
};

#define FORMAT_STRING_ARGS(format, output, len) \
{                                               \
	va_list ap;                                 \
	char buffer[len];                           \
	va_start(ap, format);                       \
	vsprintf(buffer, format, ap);               \
	buffer[len - 1] = 0;						\
	va_end(ap);                                 \
	(output) = buffer;                          \
}

static int va_str_format(char* buffer, int buffer_size, const char* format, va_list VarArgs)
{
#if defined(CONF_FAMILY_WINDOWS)
	_vsnprintf(buffer, buffer_size, format, VarArgs);
	buffer[buffer_size - 1] = 0;
#else
	vsnprintf(buffer, buffer_size, format, VarArgs);
	/* null termination is assured by definition of vsnprintf */
#endif
	return str_utf8_fix_truncation(buffer);
}

class CConectionPool
{
	CConectionPool();

	static std::shared_ptr<CConectionPool> m_Instance;
	class IServer* m_pServer;

	std::list<Connection*>m_ConnList;
	Driver* m_pDriver;

public:
	~CConectionPool();
	void Init(IServer* pServer) { m_pServer = pServer; }

	Connection* GetConnection();
	Connection* CreateConnection();
	void ReleaseConnection(Connection* pConnection);
	void DisconnectConnection(Connection* pConnection);
	void DisconnectConnectionHeap();
	static CConectionPool& GetInstance();

	// database extraction function
private:
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
		CResultSelect& UpdateQuery(const char* pSelect, const char* pTable, const char* pBuffer = "\0", ...)
		{
			std::string strQuery;
			FORMAT_STRING_ARGS(pBuffer, strQuery, MAX_QUERY_LEN);
			m_Query = std::string("SELECT " + std::string(pSelect) + " FROM " + std::string(pTable) + " " + strQuery + ";");
			return *this;
		}

		[[nodiscard]] ResultPtr Execute() const
		{
			const char* pError = nullptr;

			g_SqlThreadRecursiveLock.lock();
			Sqlpool.m_pDriver->threadInit();
			Connection* pConnection = Sqlpool.GetConnection();
			ResultPtr pResult = nullptr;
			try
			{
				const std::unique_ptr<Statement> pStmt(pConnection->createStatement());
				pResult.reset(pStmt->executeQuery(m_Query.c_str()));
				pStmt->close();
			}
			catch (SQLException& e)
			{
				pError = e.what();
			}
			Sqlpool.ReleaseConnection(pConnection);
			Sqlpool.m_pDriver->threadEnd();
			g_SqlThreadRecursiveLock.unlock();

			if (pError != nullptr)
				dbg_msg("SQL", "%s", pError);

			return pResult;
		}

		void AtExecute(const std::function<void(IServer*, ResultPtr)> pCallback = nullptr)
		{
			auto Item = [pCallback](const std::string Query)
			{
				const char* pError = nullptr;

				g_SqlThreadRecursiveLock.lock();
				Sqlpool.m_pDriver->threadInit();
				Connection* pConnection = Sqlpool.GetConnection();
				try
				{
					const std::unique_ptr<Statement> pStmt(pConnection->createStatement());
					ResultPtr pResult(pStmt->executeQuery(Query.c_str()));
					if (pCallback)
						pCallback(Sqlpool.m_pServer, std::move(pResult));
					pStmt->close();
				}
				catch (SQLException& e)
				{
					pError = e.what();
				}
				Sqlpool.ReleaseConnection(pConnection);
				Sqlpool.m_pDriver->threadEnd();
				g_SqlThreadRecursiveLock.unlock();

				if (pError != nullptr)
					dbg_msg("SQL", "%s", pError);
			};
			std::thread(Item, m_Query).detach();
		}
	};

	class CResultQuery : public CResultBase
	{
	public:
		CResultQuery& UpdateQuery(const char* pTable, const char* pBuffer, ...)
		{
			std::string strQuery;
			FORMAT_STRING_ARGS(pBuffer, strQuery, MAX_QUERY_LEN);

			if (m_TypeQuery == DB::INSERT)
				m_Query = std::string("INSERT INTO " + std::string(pTable) + " " + strQuery + ";");
			else if (m_TypeQuery == DB::UPDATE)
				m_Query = std::string("UPDATE " + std::string(pTable) + " SET " + strQuery + ";");
			else if (m_TypeQuery == DB::REMOVE)
				m_Query = std::string("DELETE FROM " + std::string(pTable) + " " + strQuery + ";");
			return *this;
		}

		void AtExecute(const std::function<void(IServer*)>& pCallback = nullptr, int DelayMilliseconds = 0)
		{
			auto Item = [pCallback](const std::string Query, const int Milliseconds)
			{
				if (Milliseconds > 0)
					std::this_thread::sleep_for(std::chrono::milliseconds(Milliseconds));

				const char* pError = nullptr;

				g_SqlThreadRecursiveLock.lock();
				Sqlpool.m_pDriver->threadInit();
				Connection* pConnection = Sqlpool.GetConnection();
				try
				{
					const std::unique_ptr<Statement> pStmt(pConnection->createStatement());
					pStmt->execute(Query.c_str());
					if (pCallback)
						pCallback(Sqlpool.m_pServer);
					pStmt->close();
				}
				catch (SQLException& e)
				{
					pError = e.what();
				}
				Sqlpool.ReleaseConnection(pConnection);
				Sqlpool.m_pDriver->threadEnd();
				g_SqlThreadRecursiveLock.unlock();

				if (pError != nullptr)
					dbg_msg("SQL", "%s", pError);
			};
			std::thread(Item, m_Query, DelayMilliseconds).detach();
		}
		void Execute(int DelayMilliseconds = 0) { return AtExecute(nullptr, DelayMilliseconds); }
	};

	class CResultQueryCustom : public CResultQuery
	{
	public:
		CResultQueryCustom& UpdateQuery(const char* pBuffer, ...)
		{
			std::string strQuery;
			FORMAT_STRING_ARGS(pBuffer, strQuery, MAX_QUERY_LEN);
			m_Query = std::string(strQuery + ";");
			return *this;
		}
	};

	// - - - - - - - - - - - - - - - -
	// select
	// - - - - - - - - - - - - - - - -
	static std::shared_ptr<CResultSelect> PrepareQuerySelect(DB Type, const char* pSelect, const char* pTable, std::string strQuery)
	{
		CResultSelect Data;
		Data.m_Query = std::string("SELECT " + std::string(pSelect) + " FROM " + std::string(pTable) + " " + strQuery + ";");
		Data.m_TypeQuery = Type;

		return std::make_shared<CResultSelect>(Data);
	}

public:
	template<DB T>
	static std::enable_if_t<T == DB::SELECT, std::shared_ptr<CResultSelect>> Prepare(const char* pSelect, const char* pTable, const char* pBuffer = "\0", ...)
	{
		std::string strQuery;
		FORMAT_STRING_ARGS(pBuffer, strQuery, MAX_QUERY_LEN);

		// checking format query
		return PrepareQuerySelect(T, pSelect, pTable, strQuery);
	}

	template<DB T>
	static std::enable_if_t<T == DB::SELECT, ResultPtr> Execute(const char* pSelect, const char* pTable, const char* pBuffer = "\0", ...)
	{
		std::string strQuery;
		FORMAT_STRING_ARGS(pBuffer, strQuery, MAX_QUERY_LEN);

		// checking format query
		return PrepareQuerySelect(T, pSelect, pTable, strQuery)->Execute();
	}

	// - - - - - - - - - - - - - - - -
	// custom
	// - - - - - - - - - - - - - - - -
private:
	static std::shared_ptr<CResultQueryCustom> PrepareQueryCustom(DB Type, std::string strQuery)
	{
		CResultQueryCustom Data;
		Data.m_Query = std::string(strQuery + ";");
		Data.m_TypeQuery = Type;

		return std::make_shared<CResultQueryCustom>(Data);
	}

public:
	template<DB T>
	static std::enable_if_t<T == DB::OTHER, CResultQueryCustom> Prepare(const char* pBuffer, ...)
	{
		std::string strQuery;
		FORMAT_STRING_ARGS(pBuffer, strQuery, MAX_QUERY_LEN);

		// checking format query
		return PrepareQueryCustom(T, strQuery);
	}

	template<DB T, int Milliseconds = 0>
	static std::enable_if_t<T == DB::OTHER, void> Execute(const char* pBuffer, ...)
	{
		std::string strQuery;
		FORMAT_STRING_ARGS(pBuffer, strQuery, MAX_QUERY_LEN);

		// checking format query
		PrepareQueryCustom(T, strQuery)->Execute(Milliseconds);
	}

	// - - - - - - - - - - - - - - - -
	// insert : update : delete
	// - - - - - - - - - - - - - - - -
private:
	static std::shared_ptr<CResultQuery> PrepareQueryInsertUpdateDelete(DB Type, const char * pTable, std::string strQuery)
	{
		CResultQuery Data;
		Data.m_TypeQuery = Type;
		if(Type == DB::INSERT)
			Data.m_Query = std::string("INSERT INTO " + std::string(pTable) + " " + strQuery + ";");
		else if(Type == DB::UPDATE)
			Data.m_Query = std::string("UPDATE " + std::string(pTable) + " SET " + strQuery + ";");
		else if(Type == DB::REMOVE)
			Data.m_Query = std::string("DELETE FROM " + std::string(pTable) + " " + strQuery + ";");

		return std::make_shared<CResultQuery>(Data);
	}

public:
	template<DB T>
	static std::enable_if_t<(T == DB::INSERT || T == DB::UPDATE || T == DB::REMOVE), CResultQuery> Prepare(const char* pTable, const char* pBuffer, ...)
	{
		std::string strQuery;
		FORMAT_STRING_ARGS(pBuffer, strQuery, MAX_QUERY_LEN);

		// checking format query
		return PrepareQueryInsertUpdateDelete(T, pTable, strQuery);
	}

	template<DB T, int Milliseconds = 0>
	static std::enable_if_t<(T == DB::INSERT || T == DB::UPDATE || T == DB::REMOVE), void> Execute(const char* pTable, const char* pBuffer, ...)
	{
		std::string strQuery;
		FORMAT_STRING_ARGS(pBuffer, strQuery, MAX_QUERY_LEN);

		// checking format query
		PrepareQueryInsertUpdateDelete(T, pTable, strQuery)->Execute(Milliseconds);
	}
};

#endif