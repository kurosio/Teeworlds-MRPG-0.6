/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <varargs.h>
#include <engine/server/sql_string_helpers.h>
#include "GuildHistoryLogManager.h"

#include "../GuildData.h"

GuildHistoryContainer&& CGuildHistoryController::GetLogs() const
{
	GuildHistoryContainer Logs;
	Logs.reserve(20);

	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_guilds_history", "WHERE GuildID = '%d' ORDER BY ID DESC LIMIT 20", m_pGuild->GetID());
	while(pRes->next())
		Logs.push_back({ pRes->getString("Time").c_str(), pRes->getString("Text").c_str() });

	return std::move(Logs);
}

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

	sqlstr::CSqlString<64> cBuf = sqlstr::CSqlString<64>(aBuf);
	Database->Execute<DB::INSERT>("tw_guilds_history", "(GuildID, Text) VALUES ('%d', '%s')", m_pGuild->GetID(), cBuf.cstr());
}
