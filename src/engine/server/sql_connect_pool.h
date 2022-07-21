#ifndef ENGINE_SERVER_SQL_CONNECTIONPOOL_H
#define ENGINE_SERVER_SQL_CONNECTIONPOOL_H

#include <mysql_connection.h>
#include <cppconn/driver.h>
#include <cppconn/statement.h>
#include <cppconn/resultset.h>

using namespace sql;
#define SJK CConectionPool::GetInstance()
typedef std::unique_ptr<ResultSet> ResultPtr;

class CConectionPool
{
	CConectionPool();

	static std::shared_ptr<CConectionPool> m_Instance;

	std::list<class Connection*>m_ConnList;
	class Driver *m_pDriver;

	void InsertFormated(int Milliseconds, const char *Table, const char *Buffer, va_list args);
	void UpdateFormated(int Milliseconds, const char *Table, const char *Buffer, va_list args);
	void DeleteFormated(int Milliseconds, const char *Table, const char *Buffer, va_list args);

public:
	~CConectionPool();

	class Connection* GetConnection();
	class Connection* CreateConnection();
	void ReleaseConnection(class Connection* pConnection);
	void DisconnectConnection(class Connection* pConnection);
	void DisconnectConnectionHeap();
	static CConectionPool& GetInstance();

	// simply inserts data
	void ID(const char *Table, const char *Buffer, ...);
	void IDS(int Milliseconds, const char *Table, const char *Buffer, ...);

	// simply update the data that will be specified
	void UD(const char *Table, const char *Buffer, ...);
	void UDS(int Milliseconds, const char *Table, const char *Buffer, ...);

	// simply deletes the data that will be specified
	void DD(const char *Table, const char *Buffer, ...);
	void DDS(int Milliseconds, const char *Table, const char *Buffer, ...);

	// database extraction function
	ResultPtr SD(const char *Select, const char *Table, const char *Buffer = "", ...);
	void SDT(const char* Select, const char* Table, std::function<void(ResultPtr)> func, const char* Buffer = "", ...);
};

#endif
