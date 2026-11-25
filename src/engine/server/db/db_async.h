#ifndef ENGINE_SERVER_DB_DB_ASYNC_H
#define ENGINE_SERVER_DB_DB_ASYNC_H

// The database API: Query and Transaction are asynchronous; CRequest owns
// completion state and results own copied rows.
//
//     Db::Query(Kind, "SELECT Nick FROM tw_accounts_data WHERE ID = ?", Db::Bind(ID))
//         .Then([](Db::CResultSet&& R) { if(R.Ok() && R.HasRows()) Use(R.GetString("Nick")); });
//
//     Db::Transaction(Kind, [](Db::CTransaction& Tx) { return Tx.Execute("...", Db::Bind(...)).Ok(); });
//
//     auto Rows = Db::Query(Kind, "SHOW TABLES").Result();  // startup only
//
// Query and Transaction callbacks run on the game thread through
// Async::RunMainThreadTasks() and are delivered at most once while the pump is
// active. Requests on one endpoint may run concurrently; submission order is
// not a commit-order guarantee. Put dependent writes in one Transaction or
// submit the next step from the previous callback. Retries, timeouts and lost
// connections are handled by the executor.

#include "db_pool.h"
#include "db_result.h"
#include "db_types.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace Db
{
	class CDbExecutor;
	class CTransaction;

	// One endpoint: only the driver and the endpoint have to be set, the rest is
	// what a server wants.
	struct SExecutorConfig
	{
		IDbDriver* m_pDriver = nullptr;   // Db::CMariaDbDriver in the server
		SEndpoint m_Endpoint;
		int m_MaxConnections = 2;         // idle + busy, a hard limit
		int m_AcquireTimeoutMs = 5000;    // wait for a free connection
		int m_ConnectTimeoutMs = 5000;   // bound one open, including connector lock contention
		int m_ConnectRetryDelayMs = 500;  // initial reconnect backoff
		int m_ConnectRetryMaxDelayMs = 30000; // cap for exponential reconnect backoff
		// Ping a connection that was idle this long before reusing it (0 = off).
		int m_IdlePingMs = 30000;
		int m_Workers = 2;                // keep them equal to the connections
		int m_MaxQueue = 2000;            // a full queue refuses work instead of stalling the game
		int m_QueueWarnSize = 200;
		int m_SelectRetries = 3;          // reads recover from a lost connection
		int m_RetryBaseDelayMs = 500;
		int m_RetryMaxDelayMs = 8000;
		int m_RequestTimeoutMs = 30000;   // one request, retries included
		// A statement that takes longer is reported with its text; 0 turns the
		// report off. Without it a slow database only shows up as a long queue.
		int m_SlowStatementWarnMs = 1000;
	};

	// One pending operation: a handle on shared state, so the worker may finish
	// after the caller dropped it. A default constructed request is a no-op.
	class CRequest
	{
	public:
		using CCallback = std::function<void(CResultSet&&)>;

		CRequest() = default;

		// Runs at most once on the game thread while its completion pump is active.
		// Returns *this, so a statement and callback are one expression.
		CRequest& Then(CCallback Callback);

		// Cancels queued work before it starts when possible. An SQL call already
		// running cannot be interrupted safely; only its result delivery is
		// cancelled, and a write may already have committed. Cancellation is never
		// a reason to retry a write with an unknown outcome.
		void Cancel();

		// Explicit blocking wait; for tests and non-game startup only. A wait
		// timeout does not cancel the SQL task, which may still finish (including
		// committing a write); call Cancel() separately when appropriate. Asking
		// twice returns the same answer. Use this or a callback for one answer, not both.
		CResultSet Result(int TimeoutMs = -1);

		explicit operator bool() const { return m_pState != nullptr; }

	private:
		friend class CDbExecutor;
		friend CRequest Query(EKind Kind, std::string_view Sql, const CBindings& Bindings, const SQueryOptions& Options);
		friend CRequest Transaction(EKind Kind, std::function<bool(CTransaction&)> Body, const SQueryOptions& Options);

		struct CCancelToken
		{
			// 0 = queued, 1 = executing, 2 = cancelled before execution, 3 = finished.
			std::atomic<int> m_Phase { 0 };
			std::atomic<bool> m_CancelRequested { false };
		};

		struct SState
		{
			std::mutex m_Mutex;
			std::condition_variable m_Cv;
			std::string m_Tag;
			std::atomic<bool> m_TagActive { false };
			CResultSet m_Result;
			CCallback m_Callback;
			CCancelToken m_CancelToken;
			bool m_Completed = false;
			bool m_Delivered = false;
			bool m_Cancelled = false;
		};

		struct SCoalescedGroup
		{
			std::vector<std::shared_ptr<SState>> m_vWaiters;
		};

		static std::mutex s_CoalesceMutex;
		static std::unordered_map<std::string, std::shared_ptr<SCoalescedGroup>> s_Coalesced;

		static CRequest FromState(std::shared_ptr<SState> pState);
		static void FinishTag(const std::shared_ptr<SState>& pState);
		static void FinishCoalescedGroup(const std::string& Key, const std::shared_ptr<SCoalescedGroup>& pGroup, const CResultSet& Result);
		static CRequest Submit(EKind Kind, std::function<CResultSet(IDbConnection&)> Job, const SQueryOptions& Options, int Retries, std::string Sql = {}, std::string DedupeKey = {}, bool RetrySafe = false);
		void Complete(CResultSet&& Result);
		void Deliver();

		std::shared_ptr<SState> m_pState;
	};

	// Statements inside one transaction, sequentially on one worker and one
	// connection. The body must use only Tx.Execute(): nested Db::Query/Transaction
	// submissions are rejected so they cannot escape the transaction's atomic
	// boundary or deadlock the pool. Only SELECT/INSERT/UPDATE/DELETE/REPLACE
	// statements are accepted; control and implicit-commit statements are refused.
	// A failed statement is remembered and later statements are refused.
	class CTransaction
	{
	public:
		CResultSet Execute(std::string_view Sql, const CBindings& Bindings = {});
		CTransaction(const CTransaction&) = delete;
		CTransaction& operator=(const CTransaction&) = delete;
		CTransaction(CTransaction&&) = delete;
		CTransaction& operator=(CTransaction&&) = delete;

	private:
		friend CRequest Transaction(EKind Kind, std::function<bool(CTransaction&)> Body, const SQueryOptions& Options);
		explicit CTransaction(IDbConnection& Connection, EKind Kind) : m_Connection(Connection), m_Kind(Kind) {}

		IDbConnection& m_Connection;
		EKind m_Kind;
		SError m_Error;
		size_t m_AffectedRows = 0;
		int64_t m_InsertId = 0;
	};

	// Worker threads of one endpoint. A job receives a borrowed connection;
	// retries, timeouts, the delay and the lease are handled here, in one place.
	class CDbExecutor
	{
	public:
		using CJob = std::function<CResultSet(IDbConnection&)>;
		using CDone = std::function<void(CResultSet&&)>;

		CDbExecutor(std::shared_ptr<CDbPool> pPool, SExecutorConfig Config);
		~CDbExecutor();

		CDbExecutor(const CDbExecutor&) = delete;
		CDbExecutor& operator=(const CDbExecutor&) = delete;

		std::shared_ptr<CDbPool> Pool() const { return m_pPool; }
		const SExecutorConfig& Config() const { return m_Config; }
		size_t QueuedCount() const;

		// What the workers did so far: the numbers a queue that never drains is
		// diagnosed with.
		struct SStats
		{
			uint64_t m_Submitted = 0;
			uint64_t m_TimedOut = 0;   // the deadline passed while the statement ran
			uint64_t m_Expired = 0;    // the deadline passed before it started
			uint64_t m_Retries = 0;    // additional attempts of explicitly safe work
			uint64_t m_Coalesced = 0;  // answered by an identical read in flight
			uint64_t m_WaitMaxMs = 0;  // longest time a request sat in the queue
			uint64_t m_SlowestMs = 0;
			std::string m_SlowestLabel;
			size_t m_QueueMax = 0;
		};
		SStats Stats() const;
		// Counts a request answered by the coalescing table instead of the pool.
		void CountCoalesced();

	private:
		friend class CRequest;
		friend void Stop(int WaitMs);

		// False when stopping or full. Reads may give up waiting for a connection
		// early; writes wait up to their request deadline.
		bool Submit(CJob Job, const SQueryOptions& Options, int Retries, CDone Done, CRequest::CCancelToken* pCancelToken, std::string Label = {}, bool ReadOnly = false, bool RetrySafe = false);
		// Shutdown is controlled by the manager, not individual callers.
		void Stop(int WaitMs = 1000);

		struct CTask
		{
			CJob m_Job;
			CDone m_Done;
			// Non-owning: m_Done captures the owning CRequest state for the entire
			// queued/running lifetime of this task.
			CRequest::CCancelToken* m_pCancelToken = nullptr;
			int64_t m_DeadlineMs = 0;
			int64_t m_QueuedMs = 0;
			int m_RetriesLeft = 0;
			int m_Attempt = 0;
			int m_DelayMs = 0;
			bool m_ReadOnly = false;
			bool m_RetrySafe = false;
			// A short one-line form of the statement, for the diagnostics.
			std::string m_Label;
		};

		void WorkerThread();
		CResultSet RunTask(CTask& Task);
		void WarnAboutQueue(size_t Queued);
		// Late: the request had already timed out when the answer arrived.
		void ReportLongStatement(const std::string& Label, long long ElapsedMs, bool Late = false);
		void CountExpired();

		std::shared_ptr<CDbPool> m_pPool;
		SExecutorConfig m_Config;
		std::vector<std::thread> m_vWorkers;
		// Fixed-capacity FIFO ring: all queue storage is allocated once during
		// startup, not on the submitting/game thread under load.
		std::vector<std::optional<CTask>> m_vTaskSlots;
		size_t m_QueueHead = 0;
		size_t m_Queued = 0;
		mutable std::mutex m_Mutex;
		std::condition_variable m_Cv;
		std::atomic<bool> m_Stopping { false };
		std::mutex m_StopMutex;
		SStats m_Stats;
		long long m_LastQueueWarnMs = 0;
		int m_SuppressedQueueWarns = 0;
		long long m_LastLongStatementWarnMs = 0;
		int m_SuppressedLongStatements = 0;
		bool m_QueueWarned = false;
	};

	// --- lifecycle ----------------------------------------------------------
	// Both independent endpoints are mandatory. Startup is atomic: neither pool
	// becomes visible unless both executors and worker sets are created.
	bool Start(const SExecutorConfig& Remote, const SExecutorConfig& Local);
	// Stops both endpoints and joins their workers. Running calls rely on driver
	// timeouts; WaitMs does not forcibly interrupt an active database call.
	void Stop(int WaitMs = 2000);
	bool IsStarted();

	// Monitoring: connection counts, reuse and failure statistics, queue size.
	// Shared handles stay valid while Stop() retires the endpoints.
	std::shared_ptr<CDbExecutor> Executor(EKind Kind);
	std::shared_ptr<CDbPool> Pool(EKind Kind);

	// --- statements ---------------------------------------------------------
	// Runs one statement with bound parameters; the result carries rows, affected
	// rows or the insert id, and the error if any.
	CRequest Query(EKind Kind, std::string_view Sql, const CBindings& Bindings = {}, const SQueryOptions& Options = {});

	// Named in-flight query state. WatchTag registers a one-shot notification
	// when the last request with this tag completes; its callback runs on the
	// game thread. It returns false if the tag is not active.
	bool IsTagRunning(std::string_view Tag, int MaxAgeSeconds = 0);
	bool WatchTag(std::string_view Tag, std::function<void()> Callback);

	// Runs Body atomically on one connection: true commits, false or a failed
	// statement rolls back. InsertId() is the first generated id.
	CRequest Transaction(EKind Kind, std::function<bool(CTransaction&)> Body, const SQueryOptions& Options = {});

}

#endif // ENGINE_SERVER_DB_DB_ASYNC_H
