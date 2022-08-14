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

enum class DB
{
	SELECT = 0,
	INSERT,
	UPDATE,
	REMOVE,
	OTHER,
};

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
			char aBuf[1024];
			va_list VarArgs;
			va_start(VarArgs, pBuffer);
			va_str_format(aBuf, sizeof(aBuf), pBuffer, VarArgs);
			va_end(VarArgs);

			m_Query = std::string("SELECT " + std::string(pSelect) + " FROM " + std::string(pTable) + " " + std::string(aBuf) + ";");
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

		void AtExecute(const std::function<void(IServer*, ResultPtr)>& pCallback = nullptr)
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
			char aBuf[1024];
			va_list VarArgs;
			va_start(VarArgs, pBuffer);
			va_str_format(aBuf, sizeof(aBuf), pBuffer, VarArgs);
			va_end(VarArgs);

			if (m_TypeQuery == DB::INSERT)
				m_Query = std::string("INSERT INTO " + std::string(pTable) + " " + std::string(aBuf) + ";");
			else if (m_TypeQuery == DB::UPDATE)
				m_Query = std::string("UPDATE " + std::string(pTable) + " SET " + std::string(aBuf) + ";");
			else if (m_TypeQuery == DB::REMOVE)
				m_Query = std::string("DELETE FROM " + std::string(pTable) + " " + std::string(aBuf) + ";");
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
			char aBuf[1024];
			va_list VarArgs;
			va_start(VarArgs, pBuffer);
			va_str_format(aBuf, sizeof(aBuf), pBuffer, VarArgs);
			va_end(VarArgs);

			m_Query = std::string(std::string(aBuf) + ";");
			return *this;
		}
	};

	// - - - - - - - - - - - - - - - -
	// select
	// - - - - - - - - - - - - - - - -
	static std::shared_ptr<CResultSelect> PrepareQuerySelect(DB Type, const char* pSelect, const char* pTable, const char* pBuffer)
	{
		CResultSelect Data;
		Data.m_Query = std::string("SELECT " + std::string(pSelect) + " FROM " + std::string(pTable) + " " + std::string(pBuffer) + ";");
		Data.m_TypeQuery = Type;

		return std::make_shared<CResultSelect>(Data);
	}

public:
	template<DB T>
	static std::enable_if_t<T == DB::SELECT, std::shared_ptr<CResultSelect>> Prepare(const char* pSelect, const char* pTable, const char* pBuffer = "\0", ...)
	{
		char aBuf[1024];
		va_list VarArgs;
		va_start(VarArgs, pBuffer);
		va_str_format(aBuf, sizeof(aBuf), pBuffer, VarArgs);
		va_end(VarArgs);

		// checking format query
		return PrepareQuerySelect(T, pSelect, pTable, aBuf);
	}

	template<DB T>
	static std::enable_if_t<T == DB::SELECT, ResultPtr> Execute(const char* pSelect, const char* pTable, const char* pBuffer = "\0", ...)
	{
		char aBuf[1024];
		va_list VarArgs;
		va_start(VarArgs, pBuffer);
		va_str_format(aBuf, sizeof(aBuf), pBuffer, VarArgs);
		va_end(VarArgs);

		// checking format query
		return PrepareQuerySelect(T, pSelect, pTable, aBuf)->Execute();
	}

	// - - - - - - - - - - - - - - - -
	// custom
	// - - - - - - - - - - - - - - - -
private:
	static std::shared_ptr<CResultQueryCustom> PrepareQueryCustom(DB Type, const char* pBuffer)
	{
		CResultQueryCustom Data;
		Data.m_Query = std::string(std::string(pBuffer) + ";");
		Data.m_TypeQuery = Type;

		return std::make_shared<CResultQueryCustom>(Data);
	}

public:
	template<DB T>
	static std::enable_if_t<T == DB::OTHER, CResultQueryCustom> Prepare(const char* pBuffer, ...)
	{
		char aBuf[1024];
		va_list VarArgs;
		va_start(VarArgs, pBuffer);
		va_str_format(aBuf, sizeof(aBuf), pBuffer, VarArgs);
		va_end(VarArgs);

		// checking format query
		return PrepareQueryCustom(T, aBuf);
	}

	template<DB T, int Milliseconds = 0>
	static std::enable_if_t<T == DB::OTHER, void> Execute(const char* pBuffer, ...)
	{
		char aBuf[1024];
		va_list VarArgs;
		va_start(VarArgs, pBuffer);
		va_str_format(aBuf, sizeof(aBuf), pBuffer, VarArgs);
		va_end(VarArgs);

		// checking format query
		PrepareQueryCustom(T, aBuf)->Execute(Milliseconds);
	}

	// - - - - - - - - - - - - - - - -
	// insert : update : delete
	// - - - - - - - - - - - - - - - -
private:
	static std::shared_ptr<CResultQuery> PrepareQueryInsertUpdateDelete(DB Type, const char * pTable, const char* pBuffer)
	{
		CResultQuery Data;
		Data.m_TypeQuery = Type;
		if(Type == DB::INSERT)
			Data.m_Query = std::string("INSERT INTO " + std::string(pTable) + " " + std::string(pBuffer) + ";");
		else if(Type == DB::UPDATE)
			Data.m_Query = std::string("UPDATE " + std::string(pTable) + " SET " + std::string(pBuffer) + ";");
		else if(Type == DB::REMOVE)
			Data.m_Query = std::string("DELETE FROM " + std::string(pTable) + " " + std::string(pBuffer) + ";");

		return std::make_shared<CResultQuery>(Data);
	}

public:
	template<DB T>
	static std::enable_if_t<(T == DB::INSERT || T == DB::UPDATE || T == DB::REMOVE), CResultQuery> Prepare(const char* pTable, const char* pBuffer, ...)
	{
		char aBuf[1024];
		va_list VarArgs;
		va_start(VarArgs, pBuffer);
		va_str_format(aBuf, sizeof(aBuf), pBuffer, VarArgs);
		va_end(VarArgs);

		// checking format query
		return PrepareQueryInsertUpdateDelete(T, pTable, aBuf);
	}

	template<DB T, int Milliseconds = 0>
	static std::enable_if_t<(T == DB::INSERT || T == DB::UPDATE || T == DB::REMOVE), void> Execute(const char* pTable, const char* pBuffer, ...)
	{
		char aBuf[1024];
		va_list VarArgs;
		va_start(VarArgs, pBuffer);
		va_str_format(aBuf, sizeof(aBuf), pBuffer, VarArgs);
		va_end(VarArgs);

		// checking format query
		PrepareQueryInsertUpdateDelete(T, pTable, aBuf)->Execute(Milliseconds);
	}
};

#endif