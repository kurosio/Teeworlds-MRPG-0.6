#include "sql_lost_query_logger.h"

#include <base/format.h>
#include <engine/shared/config.h>

CSqlLostQueryLogger& CSqlLostQueryLogger::Instance()
{
	static CSqlLostQueryLogger s_Instance;
	return s_Instance;
}

void CSqlLostQueryLogger::EnsureFileLocked(const char* pFilename)
{
	if(!pFilename || pFilename[0] == '\0')
	{
		if(m_File)
		{
			io_close(m_File);
			m_File = nullptr;
		}
		m_Filename.clear();
		return;
	}

	if(m_File && m_Filename == pFilename)
		return;

	if(m_File)
	{
		io_close(m_File);
		m_File = nullptr;
	}

	m_Filename = pFilename;
	m_File = io_open(pFilename, IOFLAG_APPEND);
}

void CSqlLostQueryLogger::LogLostQuery(const char* pReason, const char* pType, const std::string& Query)
{
	if(g_Config.m_SvSqlFailedLogFile[0] == '\0')
		return;

	std::lock_guard<std::mutex> lock(m_Mutex);
	EnsureFileLocked(g_Config.m_SvSqlFailedLogFile);
	if(!m_File)
		return;

	// Keep every record on one line even if a query contains control characters.
	// The caller redacts SQL literals; this logger also bounds and sanitizes the
	// text so a malformed/dynamic statement cannot forge log entries.
	constexpr size_t MAX_QUERY_LOG_LENGTH = 4096;
	std::string SafeQuery = Query.substr(0, MAX_QUERY_LOG_LENGTH);
	for(char& c : SafeQuery)
	{
		const auto Byte = static_cast<unsigned char>(c);
		if(Byte < 0x20 || Byte == 0x7f)
			c = ' ';
	}
	if(Query.size() > MAX_QUERY_LOG_LENGTH)
		SafeQuery.append("...");

	char aTimestamp[64];
	str_timestamp(aTimestamp, sizeof(aTimestamp));
	const std::string line = fmt_default("[{}] {} {}: {}", aTimestamp, pReason, pType, SafeQuery);
	io_write(m_File, line.c_str(), line.size());
	io_write_newline(m_File);
	io_flush(m_File);
}
