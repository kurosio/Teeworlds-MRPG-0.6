/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <stdarg.h>
#include "GuildLoggerManager.h"

#include <engine/server/sql_string_helpers.h>
#include "../GuildData.h"

CGuildLoggerManager::CGuildLoggerManager(CGuildData* pGuild, int64_t Logflag) : m_pGuild(pGuild)
{
	m_Logflag = Logflag < 0 ? LOGFLAG_GUILD_FULL : Logflag;

	CGuildLoggerManager::InitLogs();
}

// Set the log flag based on the given flag value
void CGuildLoggerManager::SetActivityFlag(int64_t Flag)
{
	if(Flag & m_Logflag)
		m_Logflag &= ~Flag;
	else
		m_Logflag |= Flag;

	// Update the log flag in the database for the guild
	Database->Execute<DB::UPDATE>(TW_GUILDS_TABLE, "LogFlag = '%d' WHERE ID = '%d'", m_Logflag, m_pGuild->GetID());
}

// Check if the given flag is set in the log flag
bool CGuildLoggerManager::IsActivityFlagSet(int64_t Flag) const
{
	// Return true if the log flag is less than or equal to 0, or if the given flag is set in the log flag, or if the guild full flag is set in the log flag
	return (Flag & m_Logflag);
}

// Function to add a guild log
void CGuildLoggerManager::Add(int64_t LogFlag, const char* pBuffer, ...)
{
	if(LogFlag & m_Logflag)
	{
		// Initialize the variable argument list
		char aBuf[512];
		va_list VarArgs;
		va_start(VarArgs, pBuffer);
#if defined(CONF_FAMILY_WINDOWS)
		_vsnprintf(aBuf, sizeof(aBuf), pBuffer, VarArgs);
#else
		vsnprintf(aBuf, sizeof(aBuf), pBuffer, VarArgs);
#endif
		va_end(VarArgs);

		// If the number of logs exceeds the maximum limit, remove the oldest log
		if(m_aLogs.size() >= MAX_GUILD_LOGS_NUM)
		{
			m_aLogs.pop_front();
		}

		// Get the current timestamp and store it in aBufTimeStamp
		char aBufTimeStamp[64];
		str_timestamp(aBufTimeStamp, sizeof(aBufTimeStamp));

		// Add the formatted string and timestamp to the logs list
		const sqlstr::CSqlString<64> cBuf = sqlstr::CSqlString<64>(aBuf);
		m_aLogs.push_back({ cBuf.cstr(), aBufTimeStamp });

		// Execute an SQL INSERT statement to store the log in the database
		Database->Execute<DB::INSERT>(TW_GUILDS_HISTORY_TABLE, "(GuildID, Text, Time) VALUES ('%d', '%s', '%s')", m_pGuild->GetID(), cBuf.cstr(), aBufTimeStamp);
	}
}

void CGuildLoggerManager::InitLogs()
{
	// Execute a select query to fetch guild logs from the database
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_GUILDS_HISTORY_TABLE, "WHERE GuildID = '%d' ORDER BY ID DESC LIMIT %d", m_pGuild->GetID(), (int)MAX_GUILD_LOGS_NUM);
	while(pRes->next())
	{
		// Create a log object and populate it with time and text values from the database
		m_aLogs.push_back({ pRes->getString("Text").c_str(), pRes->getString("Time").c_str() });
	}
}
