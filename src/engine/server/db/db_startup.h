#ifndef ENGINE_SERVER_DB_DB_STARTUP_H
#define ENGINE_SERVER_DB_DB_STARTUP_H

// The one place that reads the sv_sql_* settings: everything below db_async.h
// takes its settings as an argument, so it stays testable without a config.

namespace Db
{
	// Opens both databases and checks that every table sits in the database it
	// belongs to: a server must not start with a schema that would route the
	// queries to the wrong place. Blocking, once at startup.
	bool StartFromConfig();

	// Joins the workers (waiting for running statements) and drops the pending
	// completions, which belong to the objects being deleted. Idempotent.
	void Shutdown();
}

#endif // ENGINE_SERVER_DB_DB_STARTUP_H
