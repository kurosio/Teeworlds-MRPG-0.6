#ifndef ENGINE_SERVER_DB_DB_DRIVER_H
#define ENGINE_SERVER_DB_DB_DRIVER_H

// The seam between the database layer and the database client.
//
// A connection is owned by exactly one lease of the pool; nothing above this
// interface ever sees a client handle. That is why a statement can be queued,
// retried on a fresh connection, cancelled or dropped without leaving a raw
// pointer behind.

#include "db_result.h"
#include "db_types.h"

#include <memory>
#include <string>
#include <string_view>

namespace Db
{
	class IDbConnection
	{
	public:
		virtual ~IDbConnection() = default;

		// Cheap check, no round trip: false once the client marked the connection
		// closed or a previous operation made its session unsafe to reuse.
		virtual bool IsUsable() const = 0;

		// Round trip that verifies the connection still answers. Returns false
		// when it does not; the pool then discards and closes the connection.
		virtual bool Ping() = 0;

		// Runs one statement and returns either its detached rows/outcome or an
		// error. A connection-level failure is reflected by IsUsable().
		virtual CResultSet Execute(std::string_view Sql, const CBindings& Bindings, bool WantInsertId) = 0;

		// Idempotently invalidate the session. Implementations must make
		// IsUsable() false even when the underlying transport close fails; callers
		// use this after uncertain transaction state and never return that lease.
		virtual void Close() = 0;
	};

	class IDbDriver
	{
	public:
		virtual ~IDbDriver() = default;

		// Opens a new connection or returns nullptr and fills Error. TimeoutMs is
		// the remaining acquisition budget, including any driver-internal lock.
		// The message must not include credentials; the pool uses the error code
		// and SQLSTATE for recovery and diagnostics.
		virtual std::unique_ptr<IDbConnection> Connect(const SEndpoint& Endpoint, int TimeoutMs, SError& Error) = 0;
	};
}

#endif // ENGINE_SERVER_DB_DB_DRIVER_H
