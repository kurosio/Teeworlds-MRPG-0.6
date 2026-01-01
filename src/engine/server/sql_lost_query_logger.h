#ifndef ENGINE_SERVER_SQL_LOST_QUERY_LOGGER_H
#define ENGINE_SERVER_SQL_LOST_QUERY_LOGGER_H

#include <base/system.h>

#include <mutex>
#include <string>

class CSqlLostQueryLogger
{
public:
	static CSqlLostQueryLogger& Instance();

	void LogLostQuery(const char* pReason, const char* pType, const std::string& Query);

private:
	CSqlLostQueryLogger() = default;
	void EnsureFileLocked(const char* pFilename);

	IOHANDLE m_File { nullptr };
	std::string m_Filename;
	std::mutex m_Mutex;
};

#endif // ENGINE_SERVER_SQL_LOST_QUERY_LOGGER_H
