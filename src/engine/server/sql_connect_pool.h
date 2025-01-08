#ifndef ENGINE_SERVER_SQL_CONNECT_POOL_H
#define ENGINE_SERVER_SQL_CONNECT_POOL_H

// fix c++17 error with removed throw()
#if __cplusplus >= 201703L
	#define throw(...)
	#include <cppconn/driver.h>
	#include <cppconn/statement.h>
	#include <cppconn/resultset.h>
	#undef throw /* reset */
#else
	#include <cppconn/driver.h>
	#include <cppconn/statement.h>
	#include <cppconn/resultset.h>
#endif

#include <cstdarg>

using namespace sql;

/*
 * enums
 */
enum class DB
{
	SELECT = 0,
	INSERT,
	UPDATE,
	REMOVE,
	OTHER,
};

/*
 * defined
 */
#define Database CConectionPool::GetInstance()
inline std::recursive_mutex g_SqlThreadRecursiveLock;

class WrapperResultSet
{
public:
	explicit WrapperResultSet(ResultSet* pResult) : m_pResult(pResult) {}
	~WrapperResultSet() { delete m_pResult; }

	WrapperResultSet(const WrapperResultSet&) = delete;
	WrapperResultSet& operator=(const WrapperResultSet&) = delete;

	bool getBoolean(const SQLString& column) const 
	{
		return m_pResult->getBoolean(column); 
	}

	int getInt(const SQLString& column) const 
	{
		return m_pResult->getInt(column); 
	}
	unsigned int getUInt(const SQLString& column) const 
	{
		return m_pResult->getUInt(column); 
	}

	int64_t getInt64(const SQLString& column) const 
	{
		return m_pResult->getInt64(column); 
	}

	uint64_t getUInt64(const SQLString& column) const 
	{
		return m_pResult->getUInt64(column); 
	}

	float getFloat(const SQLString& column) const 
	{
		return static_cast<float>(m_pResult->getDouble(column)); 
	}

	double getDouble(const SQLString& column) const 
	{
		return m_pResult->getDouble(column); 
	}

	std::string getString(const SQLString& column) const 
	{
		return std::string(m_pResult->getString(column).c_str()); 
	}

	std::string getDateTime(const SQLString& column) const 
	{
		return std::string(m_pResult->getString(column).c_str()); 
	}

	bool next() const 
	{
		return m_pResult->next(); 
	}

	size_t rowsCount() const 
	{
		return m_pResult->rowsCount(); 
	}

	size_t getRow() const 
	{
		return m_pResult->getRow(); 
	}

	BigInt getBigInt(const SQLString& column) const
	{
		try
		{
			const std::string stringValue = m_pResult->getString(column).c_str();
			return BigInt(stringValue);
		}
		catch(const SQLException& e)
		{
			throw std::runtime_error("Failed to convert column '" + std::string(column.c_str()) + "' to BigInt: " + e.what());
		}
	}

private:
	ResultSet* m_pResult;
};

/*
 * using typename
 */
using ResultPtr = std::unique_ptr<WrapperResultSet>;
using CallbackResultPtr = std::function<void(ResultPtr)>;
using CallbackUpdatePtr = std::function<void()>;

/*
 * class
 */
class CConectionPool
{
	inline static CConectionPool* m_ptrInstance {};

public:
	static void Initilize()
	{
		if(!m_ptrInstance)
			m_ptrInstance = new CConectionPool();
	}
	static void Free()
	{
		delete m_ptrInstance;
		m_ptrInstance = nullptr;
	}
	static CConectionPool* GetInstance() { return m_ptrInstance; };


private:
	CConectionPool();

	Connection* CreateConnection();
	Connection* GetConnection();
	void ReleaseConnection(Connection* pConnection);
	void DisconnectConnection(Connection* pConnection);

	std::list< Connection* > m_ConnList;
	Driver* m_pDriver;

public:
	~CConectionPool();

	// functions
	void DisconnectConnectionHeap();

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
		template<typename... Ts>
		CResultSelect& UpdateQuery(const char* pSelect, const char* pTable, const char* pBuffer, Ts&&... args)
		{
			std::string strQuery = fmt_default(pBuffer, std::forward<Ts>(args)...);
			m_Query = fmt_default("SELECT {} FROM {} {};", pSelect, pTable, strQuery);
			return *this;
		}

		[[nodiscard]] ResultPtr Execute() const
		{
			const char* pError = nullptr;

			g_SqlThreadRecursiveLock.lock();
			Database->m_pDriver->threadInit();
			Connection* pConnection = Database->GetConnection();
			ResultPtr pResult = nullptr;
			try
			{
				const std::unique_ptr<Statement> pStmt(pConnection->createStatement());
				pResult = std::make_unique<WrapperResultSet>(pStmt->executeQuery(m_Query.c_str()));
				pStmt->close();
			}
			catch (SQLException& e)
			{
				pError = e.what();
			}
			Database->ReleaseConnection(std::move(pConnection));
			Database->m_pDriver->threadEnd();
			g_SqlThreadRecursiveLock.unlock();

			if (pError != nullptr)
				dbg_msg("SQL", "%s", pError);

			return std::move(pResult);
		}

		void AtExecute(const CallbackResultPtr& pCallbackResult)
		{
			auto Item = [pCallbackResult](const std::string Query)
			{
				const char* pError = nullptr;

				g_SqlThreadRecursiveLock.lock();
				Database->m_pDriver->threadInit();
				Connection* pConnection = Database->GetConnection();
				try
				{
					const std::unique_ptr<Statement> pStmt(pConnection->createStatement());
					auto pResult = std::make_unique<WrapperResultSet>(pStmt->executeQuery(Query.c_str()));
					if(pCallbackResult)
					{
						pCallbackResult(std::move(pResult));
					}
					pStmt->close();
				}
				catch (SQLException& e)
				{
					pError = e.what();
				}
				Database->ReleaseConnection(std::move(pConnection));
				Database->m_pDriver->threadEnd();
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

		void AtExecute(const CallbackUpdatePtr& pCallbackResult, int DelayMilliseconds = 0)
		{
			auto Item = [pCallbackResult](const std::string Query, const int Milliseconds)
			{
				if (Milliseconds > 0)
					std::this_thread::sleep_for(std::chrono::milliseconds(Milliseconds));

				const char* pError = nullptr;

				g_SqlThreadRecursiveLock.lock();
				Database->m_pDriver->threadInit();
				Connection* pConnection = Database->GetConnection();
				try
				{
					const std::unique_ptr<Statement> pStmt(pConnection->createStatement());
					pStmt->execute(Query.c_str());
					if(pCallbackResult)
					{
						pCallbackResult();
					}
					pStmt->close();
				}
				catch (SQLException& e)
				{
					pError = e.what();
				}
				Database->ReleaseConnection(std::move(pConnection));
				Database->m_pDriver->threadEnd();
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
		template <typename... Ts>
		CResultQueryCustom& UpdateQuery(const char* pBuffer, Ts&&... args)
		{
			std::string strQuery = fmt_default(pBuffer, std::forward<Ts>(args)...);
			m_Query = strQuery + ";";
			return *this;
		}
	};

	// - - - - - - - - - - - - - - - -
	// select
	// - - - - - - - - - - - - - - - -
	static std::unique_ptr<CResultSelect> PrepareQuerySelect(DB Type, const char* pSelect, const char* pTable, std::string strQuery)
	{
		CResultSelect Data;
		Data.m_Query = std::string("SELECT " + std::string(pSelect) + " FROM " + std::string(pTable) + " " + strQuery + ";");
		Data.m_TypeQuery = Type;

		return std::make_unique<CResultSelect>(Data);
	}

public:
	template<DB T>
	static std::enable_if_t<T == DB::SELECT, std::unique_ptr<CResultSelect>> Prepare(const char* pSelect, const char* pTable)
	{
		return std::move(PrepareQuerySelect(T, pSelect, pTable, ""));
	}
	template<DB T, typename... Ts>
	static std::enable_if_t<T == DB::SELECT, std::unique_ptr<CResultSelect>> Prepare(const char* pSelect, const char* pTable, const char* pBuffer, Ts&&... args)
	{
		std::string strQuery = fmt_default(pBuffer, std::forward<Ts>(args)...);
		return std::move(PrepareQuerySelect(T, pSelect, pTable, strQuery));
	}

	template<DB T, typename... Ts>
	static std::enable_if_t<T == DB::SELECT, ResultPtr> Execute(const char* pSelect, const char* pTable)
	{
		return PrepareQuerySelect(T, pSelect, pTable, "")->Execute();
	}
	template<DB T, typename... Ts>
	static std::enable_if_t<T == DB::SELECT, ResultPtr> Execute(const char* pSelect, const char* pTable, const char* pBuffer, Ts&&... args)
	{
		std::string strQuery = fmt_default(pBuffer, std::forward<Ts>(args)...);
		return PrepareQuerySelect(T, pSelect, pTable, strQuery)->Execute();
	}

	// - - - - - - - - - - - - - - - -
	// custom
	// - - - - - - - - - - - - - - - -
private:
	static std::unique_ptr<CResultQueryCustom> PrepareQueryCustom(DB Type, std::string strQuery)
	{
		CResultQueryCustom Data;
		Data.m_Query = std::string(strQuery + ";");
		Data.m_TypeQuery = Type;

		return std::make_unique<CResultQueryCustom>(Data);
	}

public:
	template<DB T, int Milliseconds = 0, typename... Ts>
	static std::enable_if_t<T == DB::OTHER, void> Execute(const char* pBuffer, Ts&&... args)
	{
		std::string strQuery = fmt_default(pBuffer, std::forward<Ts>(args)...);
		PrepareQueryCustom(T, strQuery)->Execute(Milliseconds);
	}

	// - - - - - - - - - - - - - - - -
	// insert : update : delete
	// - - - - - - - - - - - - - - - -
private:
	static std::unique_ptr<CResultQuery> PrepareQueryInsertUpdateDelete(DB Type, const char* pTable, std::string strQuery)
	{
		CResultQuery Data;
		Data.m_TypeQuery = Type;

		if(Type == DB::INSERT)
		{
			Data.m_Query = "INSERT INTO ";
			Data.m_Query.append(pTable).append(" ").append(strQuery).append(";");
		}
		else if(Type == DB::UPDATE)
		{
			Data.m_Query = "UPDATE ";
			Data.m_Query.append(pTable).append(" SET ").append(strQuery).append(";");
		}
		else if(Type == DB::REMOVE)
		{
			Data.m_Query = "DELETE FROM ";
			Data.m_Query.append(pTable).append(" ").append(strQuery).append(";");
		}

		return std::make_unique<CResultQuery>(Data);
	}

public:
	template<DB T, int Milliseconds = 0, typename... Ts>
	static std::enable_if_t<(T == DB::INSERT || T == DB::UPDATE || T == DB::REMOVE), void> Execute(const char* pTable, const char* pBuffer, Ts&&... args)
	{
		std::string strQuery = fmt_default(pBuffer, std::forward<Ts>(args)...);
		PrepareQueryInsertUpdateDelete(T, pTable, strQuery)->Execute(Milliseconds);
	}
};

#endif