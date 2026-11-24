#include "db_startup.h"

#include <base/system.h>
#include <engine/server/async.h>
#include <engine/shared/config.h>
#include <engine/server/db/db_async.h>
#include <engine/server/db/db_mariadb.h>
#include <engine/server/db/db_types.h>

#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

namespace
{
	Db::SEndpoint MakeEndpoint(const char* pName, const char* pHost, int Port, const char* pLogin, const char* pPassword, const char* pDatabase, bool UseTls)
	{
		Db::SEndpoint Endpoint;
		Endpoint.m_Name = pName;
		Endpoint.m_Host = pHost;
		Endpoint.m_Port = Port;
		Endpoint.m_User = pLogin;
		Endpoint.m_Password = pPassword;
		Endpoint.m_Database = pDatabase;
		Endpoint.m_UseTls = UseTls;
		return Endpoint;
	}

	// The limits of the pool and the retry policy come from the server settings,
	// with sensible defaults when a setting is zero.
	Db::SExecutorConfig MakeExecutorConfig(const Db::SEndpoint& Endpoint, int Connections)
	{
		Db::SExecutorConfig Config;
		Config.m_Endpoint = Endpoint;
		Config.m_MaxConnections = std::max(1, Connections);
		Config.m_AcquireTimeoutMs = g_Config.m_SvSqlQueueWaitTimeoutMs;
		Config.m_ConnectTimeoutMs = g_Config.m_SvSqlConnectTimeoutMs;
		Config.m_ConnectRetryDelayMs = g_Config.m_SvSqlConnectRetryDelayMs;
		Config.m_ConnectRetryMaxDelayMs = g_Config.m_SvSqlConnectRetryMaxDelayMs;
		Config.m_IdlePingMs = g_Config.m_SvSqlIdlePingMs;
		Config.m_Workers = std::max(1, Connections);
		Config.m_MaxQueue = g_Config.m_SvSqlQueueMaxSize;
		Config.m_QueueWarnSize = g_Config.m_SvSqlQueueWarnSize;
		Config.m_SelectRetries = g_Config.m_SvSqlSelectMaxRetries;
		Config.m_RetryBaseDelayMs = g_Config.m_SvSqlRetryBaseDelayMs;
		Config.m_RetryMaxDelayMs = g_Config.m_SvSqlRetryMaxDelayMs;
		Config.m_RequestTimeoutMs = g_Config.m_SvSqlQueryTimeoutMs;
		Config.m_SlowStatementWarnMs = g_Config.m_SvSqlSlowStatementWarnMs;
		return Config;
	}

	// Reads the lower case names of all tables of one database.
	bool ListTables(Db::EKind Kind, std::vector<std::string>& vTables)
	{
		Db::CResultSet Result = Db::Query(Kind, "SHOW TABLES;").Result();
		if(!Result.Ok())
		{
			dbg_msg("SQL Error", "Failed to read the list of tables: %s", Result.Error().ToString().c_str());
			return false;
		}

		while(Result.Next())
			vTables.emplace_back(Result.GetStringAt(0));
		return true;
	}

	// The set of tables of one database that is looked up for every local table,
	// case insensitively because the server may run on a case insensitive host.
	const char* FindTable(const std::vector<std::string>& vTables, std::string_view Table)
	{
		for(const std::string& Name : vTables)
		{
			if(Db::IsSameName(Name, Table))
				return Name.c_str();
		}
		return nullptr;
	}
}

namespace Db
{
	bool StartFromConfig()
	{
		SetDebugLoggingEnabled(g_Config.m_SvSqlDebug != 0);
		// Each endpoint is explicit. The remote endpoint is the shared authority;
		// the local endpoint is this server's own MariaDB, never an implicit alias
		// of the remote endpoint.
		const SEndpoint Remote = MakeEndpoint("remote", g_Config.m_SvMySqlHost, g_Config.m_SvMySqlPort,
			g_Config.m_SvMySqlLogin, g_Config.m_SvMySqlPassword, g_Config.m_SvMySqlDatabaseShared,
			g_Config.m_SvMySqlUseTls != 0);
		const SEndpoint Local = MakeEndpoint("local", g_Config.m_SvMySqlLocalHost, g_Config.m_SvMySqlLocalPort,
			g_Config.m_SvMySqlLocalLogin, g_Config.m_SvMySqlLocalPassword, g_Config.m_SvMySqlLocalDatabase,
			g_Config.m_SvMySqlLocalUseTls != 0);

		const auto ValidEndpoint = [](const SEndpoint& Endpoint) {
			return !Endpoint.m_Host.empty() && Endpoint.m_Port > 0 && Endpoint.m_Port <= 65535 &&
				!Endpoint.m_User.empty() && !Endpoint.m_Database.empty();
		};
		if(!ValidEndpoint(Remote))
		{
			dbg_msg("SQL Error", "Invalid remote database endpoint: configure sv_sql_host, sv_sql_port, sv_sql_login and sv_sql_database_shared.");
			return false;
		}
		if(!ValidEndpoint(Local))
		{
			dbg_msg("SQL Error", "Invalid local database endpoint: configure sv_sql_local_host, sv_sql_local_port, sv_sql_local_login and sv_sql_local_database.");
			return false;
		}

		SExecutorConfig RemoteConfig = MakeExecutorConfig(Remote, g_Config.m_SvMySqlPoolSize);
		SExecutorConfig LocalConfig = MakeExecutorConfig(Local, g_Config.m_SvMySqlLocalPoolSize);
		if(g_Config.m_SvSqlDebug)
			dbg_msg("SQL Debug", "configuration: remote=%s/%s pool=%d workers=%d; local=%s/%s pool=%d workers=%d; acquire=%dms connect=%dms request=%dms statement=%dms socket=%dms queue-limit=%d",
				Remote.m_Host.c_str(), Remote.m_Database.c_str(), RemoteConfig.m_MaxConnections, RemoteConfig.m_Workers,
				Local.m_Host.c_str(), Local.m_Database.c_str(), LocalConfig.m_MaxConnections, LocalConfig.m_Workers,
				RemoteConfig.m_AcquireTimeoutMs, RemoteConfig.m_ConnectTimeoutMs, RemoteConfig.m_RequestTimeoutMs,
				g_Config.m_SvSqlStatementTimeoutMs, g_Config.m_SvSqlSocketTimeoutMs, RemoteConfig.m_MaxQueue);

		// One MariaDB driver object serves both independently configured pools.
		// It owns no connection itself; CDbPool is the only connection owner.
		static CMariaDbDriver s_Driver;
		s_Driver.Configure(g_Config.m_SvSqlStatementTimeoutMs, g_Config.m_SvSqlSocketTimeoutMs);
		RemoteConfig.m_pDriver = &s_Driver;
		LocalConfig.m_pDriver = &s_Driver;

		if(!Db::Start(RemoteConfig, LocalConfig))
		{
			dbg_msg("SQL Error", "Failed to start both database pools; check the remote and local endpoint settings.");
			return false;
		}

		// Probe both endpoints through the same worker/driver/pool path used by
		// normal requests before loading any game state.
		std::vector<std::string> vRemoteTables;
		if(!ListTables(EKind::Remote, vRemoteTables))
		{
			dbg_msg("SQL Error", "Remote database '%s' is unavailable at %s; check network access, TLS, credentials and grants.",
				Remote.m_Database.c_str(), Remote.Address().c_str());
			Shutdown();
			return false;
		}

		std::string WrongPlace;
		for(const std::string_view Table : LOCAL_TABLES)
		{
			if(FindTable(vRemoteTables, Table))
				WrongPlace.append(WrongPlace.empty() ? "" : ", ").append(Table);
		}
		if(!WrongPlace.empty())
		{
			dbg_msg("SQL Error", "Local tables found in remote database '%s': %s. Keep each table in exactly one database.",
				Remote.m_Database.c_str(), WrongPlace.c_str());
			Shutdown();
			return false;
		}

		std::vector<std::string> vLocalTables;
		if(!ListTables(EKind::Local, vLocalTables))
		{
			dbg_msg("SQL Error", "Local database '%s' is unavailable at %s; check local MariaDB, credentials and grants.",
				Local.m_Database.c_str(), Local.Address().c_str());
			Shutdown();
			return false;
		}

		std::string Missing;
		for(const std::string_view Table : LOCAL_TABLES)
		{
			if(!FindTable(vLocalTables, Table))
				Missing.append(Missing.empty() ? "" : ", ").append(Table);
		}
		if(!Missing.empty())
		{
			dbg_msg("SQL Error", "Tables missing from local database '%s': %s. Import mrpg_local.sql into it.",
				Local.m_Database.c_str(), Missing.c_str());
			Shutdown();
			return false;
		}

		dbg_msg("SQL", "Remote database '%s' (%s, pool %d); local database '%s' (%s, pool %d).",
			Remote.m_Database.c_str(), Remote.Address().c_str(), RemoteConfig.m_MaxConnections,
			Local.m_Database.c_str(), Local.Address().c_str(), LocalConfig.m_MaxConnections);
		if(g_Config.m_SvSqlDebug)
			dbg_msg("SQL Debug", "startup schema routing validated: remote tables=%zu, local tables=%zu, required local tables=%zu",
				vRemoteTables.size(), vLocalTables.size(), std::size(LOCAL_TABLES));
		return true;
	}

	void Shutdown()
	{
		Stop(g_Config.m_SvSqlQueryTimeoutMs);
		// Generic asynchronous work has independent workers, but can capture
		// server-owned storage and objects. Drain it before those resources are
		// destroyed, then discard queued completions while game state is torn down.
		Async::Drain();
		Async::StopMainThreadTasks();
	}
}
