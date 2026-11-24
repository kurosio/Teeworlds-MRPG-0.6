#include "db_types.h"

#include <algorithm>
#include <array>
#include <atomic>

#include <base/format.h>
#include <base/system.h>

namespace Db
{
	namespace
	{
		std::atomic<bool> s_DebugLoggingEnabled { false };
	}

	std::string SError::ToString() const
	{
		if(!*this)
			return "ok";

		std::string Result = fmt_default("code={}", m_Code);
		if(!m_SqlState.empty())
			Result += fmt_default(", sqlstate={}", m_SqlState);
		if(!m_Message.empty())
			Result += fmt_default(": {}", m_Message);
		return Result;
	}

	std::string SEndpoint::Address() const
	{
		return fmt_default("{}:{}", m_Host, m_Port);
	}

	bool IsConnectionLostError(int Code, std::string_view SqlState)
	{
		// Connector/C client errors: can't connect, server gone away, socket
		// dropped, commands out of sync, or a lost connection with system error.
		if(Code == 2002 || Code == 2003 || Code == 2006 || Code == 2013 || Code == 2014 || Code == 2055)
			return true;
		// SQLSTATE class 08 is "connection exception".
		return SqlState.size() >= 2 && SqlState[0] == '0' && SqlState[1] == '8';
	}

	bool IsAuthenticationError(int Code, std::string_view SqlState)
	{
		return Code == 1044 || Code == 1045 || Code == 1129 || Code == 1130 || Code == 1698 || SqlState == "28000";
	}

	bool IsDeadlockError(int Code, std::string_view SqlState)
	{
		return Code == 1213 || SqlState == "40001";
	}

	bool IsLockWaitTimeoutError(int Code, std::string_view)
	{
		return Code == 1205;
	}

	bool IsServerUnavailableError(int Code, std::string_view SqlState)
	{
		return IsConnectionLostError(Code, SqlState) || Code == 1040;
	}

	void SetDebugLoggingEnabled(bool Enabled)
	{
		s_DebugLoggingEnabled.store(Enabled, std::memory_order_relaxed);
	}

	bool DebugLoggingEnabled()
	{
		return s_DebugLoggingEnabled.load(std::memory_order_relaxed);
	}
}
