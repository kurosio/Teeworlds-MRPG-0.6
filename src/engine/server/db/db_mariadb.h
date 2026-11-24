#ifndef ENGINE_SERVER_DB_DB_MARIADB_H
#define ENGINE_SERVER_DB_DB_MARIADB_H

// MariaDB (Connector/C++) implementation of the database seam. This is the only
// file that includes a database client header.

#include "db_driver.h"

#include <memory>

namespace Db
{
	// One driver instance serves both endpoint pools (remote and local); connection
	// creation is serialised internally because the connector's URL parser is
	// not thread safe in the versions this project supports.
	class CMariaDbDriver final : public IDbDriver
	{
	public:
		CMariaDbDriver() = default;
		~CMariaDbDriver() override = default;

		// Limits the driver applies to every connection it opens. A statement
		// timeout is what stops a slow query from holding a worker and a connection
		// long after the request that asked for it is gone.
		void Configure(int StatementTimeoutMs, int SocketTimeoutMs);

		std::unique_ptr<IDbConnection> Connect(const SEndpoint& Endpoint, int TimeoutMs, SError& Error) override;

	private:
		int m_StatementTimeoutMs = 0;
		int m_SocketTimeoutMs = 60000;
	};
}

#endif // ENGINE_SERVER_DB_DB_MARIADB_H
