/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <varargs.h>
#include <engine/server/sql_string_helpers.h>
#include "GuildHistoryLogManager.h"

#include "../GuildData.h"

// Function to get guild logs
GuildHistoryContainer&& CGuildHistoryController::GetLogs() const
{
	// Create a container to hold guild logs
	GuildHistoryContainer Logs;
	// Reserve space for 20 logs
	Logs.reserve(20);

	// Execute a select query to fetch guild logs from the database
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_guilds_history", "WHERE GuildID = '%d' ORDER BY ID DESC LIMIT 20", m_pGuild->GetID());
	while(pRes->next())
	{
		// Create a log object and populate it with time and text values from the database
		Logs.push_back({ pRes->getString("Time").c_str(), pRes->getString("Text").c_str() });
	}

	// Return the container of guild logs
	return std::move(Logs);
}

// Function to add a guild log
void CGuildHistoryController::Add(const char* pBuffer, ...) const
{
	char aBuf[512];
	va_list VarArgs;
	va_start(VarArgs, pBuffer);
#if defined(CONF_FAMILY_WINDOWS)
	_vsnprintf(aBuf, sizeof(aBuf), pBuffer, VarArgs);
#else
	vsnprintf(aBuf, sizeof(aBuf), pBuffer, VarArgs);
#endif
	va_end(VarArgs);

	// Execute an insert query to add the guild log to the database
	sqlstr::CSqlString<64> cBuf = sqlstr::CSqlString<64>(aBuf);
	Database->Execute<DB::INSERT>("tw_guilds_history", "(GuildID, Text) VALUES ('%d', '%s')", m_pGuild->GetID(), cBuf.cstr());
}
