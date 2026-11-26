// Tests of the queue of one endpoint: identical reads that share one statement,
// the deadline of a request and the counters the diagnostics are built on.
//
// They use a scripted driver, so they need no database server and no client:
// what is under test is the queue, not the wire.

#include <engine/server/async.h>
#include <engine/server/db/db_async.h>
#include <engine/server/db/db_pool.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{
	// One scripted statement: matched by a piece of its text.
	struct SRule
	{
		std::string m_SqlContains;
		int m_DelayMs = 0;
	};

	class CFailingDriver final : public Db::IDbDriver
	{
	public:
		explicit CFailingDriver(int ConnectDelayMs = 0) : m_ConnectDelayMs(ConnectDelayMs) {}

		std::unique_ptr<Db::IDbConnection> Connect(const Db::SEndpoint&, int TimeoutMs, Db::SError& Error) override
		{
			++m_Connects;
			const int Active = ++m_Active;
			int Maximum = m_MaxConcurrent.load();
			while(Active > Maximum && !m_MaxConcurrent.compare_exchange_weak(Maximum, Active))
			{
			}
			if(m_ConnectDelayMs > 0)
				std::this_thread::sleep_for(std::chrono::milliseconds(std::min(m_ConnectDelayMs, std::max(1, TimeoutMs))));
			--m_Active;
			Error = Db::SError { 2003, "HY000", "scripted connect failure" };
			return nullptr;
		}

		int Connects() const { return m_Connects.load(); }
		int MaxConcurrent() const { return m_MaxConcurrent.load(); }

	private:
		const int m_ConnectDelayMs;
		std::atomic<int> m_Connects { 0 };
		std::atomic<int> m_Active { 0 };
		std::atomic<int> m_MaxConcurrent { 0 };
	};

	class CClosedConnection final : public Db::IDbConnection
	{
	public:
		bool IsUsable() const override { return false; }
		bool Ping() override { return false; }
		Db::CResultSet Execute(std::string_view, const Db::CBindings&, bool) override
		{
			return Db::CResultSet::Failure({ 2013, "HY000", "connection is closed" });
		}
		void Close() override {}
	};

	class CClosedConnectionDriver final : public Db::IDbDriver
	{
	public:
		std::unique_ptr<Db::IDbConnection> Connect(const Db::SEndpoint&, int, Db::SError&) override
		{
			return std::make_unique<CClosedConnection>();
		}
	};

	class CRecoverableConnection final : public Db::IDbConnection
	{
	public:
		explicit CRecoverableConnection(std::shared_ptr<std::atomic<bool>> pOpen) : m_pOpen(std::move(pOpen)) {}
		bool IsUsable() const override { return m_pOpen->load(); }
		bool Ping() override { return IsUsable(); }
		void Close() override { m_pOpen->store(false); }
		Db::CResultSet Execute(std::string_view, const Db::CBindings&, bool) override
		{
			if(!IsUsable())
				return Db::CResultSet::Failure({ 2013, "HY000", "connection is closed" });
			return {};
		}

	private:
		std::shared_ptr<std::atomic<bool>> m_pOpen;
	};

	class CRecoverableDriver final : public Db::IDbDriver
	{
	public:
		std::unique_ptr<Db::IDbConnection> Connect(const Db::SEndpoint&, int, Db::SError&) override
		{
			auto pOpen = std::make_shared<std::atomic<bool>>(true);
			std::lock_guard<std::mutex> Lock(m_Mutex);
			m_vConnections.push_back(pOpen);
			return std::make_unique<CRecoverableConnection>(std::move(pOpen));
		}

		void BreakConnections()
		{
			std::lock_guard<std::mutex> Lock(m_Mutex);
			for(const auto& pOpen : m_vConnections)
				pOpen->store(false);
		}

		size_t ConnectCount() const
		{
			std::lock_guard<std::mutex> Lock(m_Mutex);
			return m_vConnections.size();
		}

	private:
		mutable std::mutex m_Mutex;
		std::vector<std::shared_ptr<std::atomic<bool>>> m_vConnections;
	};

	class CTransientConnectDriver final : public Db::IDbDriver
	{
	public:
		std::unique_ptr<Db::IDbConnection> Connect(const Db::SEndpoint&, int, Db::SError& Error) override
		{
			if(++m_Connects == 1)
			{
				Error = Db::SError { 2003, "HY000", "scripted transient connect failure" };
				return nullptr;
			}
			return std::make_unique<CRecoverableConnection>(std::make_shared<std::atomic<bool>>(true));
		}

		int Connects() const { return m_Connects.load(); }

	private:
		std::atomic<int> m_Connects { 0 };
	};

	class CFirstConnectOnlyDriver final : public Db::IDbDriver
	{
	public:
		std::unique_ptr<Db::IDbConnection> Connect(const Db::SEndpoint&, int, Db::SError& Error) override
		{
			if (++m_Connects == 1)
				return std::make_unique<CRecoverableConnection>(std::make_shared<std::atomic<bool>>(true));
			Error = { 2013, "HY000", "scripted connect failure" };
			return nullptr;
		}
		std::atomic<int> m_Connects{ 0 };
	};

	class CBlockingSecondConnectDriver final : public Db::IDbDriver
	{
	public:
		std::unique_ptr<Db::IDbConnection> Connect(const Db::SEndpoint&, int, Db::SError& Error) override
		{
			if (++m_Connects == 1)
				return std::make_unique<CRecoverableConnection>(std::make_shared<std::atomic<bool>>(true));
			m_ConnectStarted = true;
			while (!m_ReleaseConnect.load())
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			Error = { 2013, "HY000", "scripted connect failure" };
			return nullptr;
		}
		std::atomic<int> m_Connects{ 0 };
		std::atomic<bool> m_ConnectStarted{ false };
		std::atomic<bool> m_ReleaseConnect{ false };
	};

	class CScriptedDriver;

	class CScriptedConnection final : public Db::IDbConnection
	{
	public:
		explicit CScriptedConnection(CScriptedDriver& Driver) : m_pDriver(Driver) {}

		bool Ping() override { return !m_Broken; }
		bool IsUsable() const override { return !m_Broken; }
		void Close() override { m_Broken = true; }

		Db::CResultSet Execute(std::string_view Sql, const Db::CBindings& Bindings, bool WantInsertId) override;

	private:
		CScriptedDriver& m_pDriver;
		bool m_Broken = false;
	};

	// Records every statement it is asked to run and answers the way its rules
	// say; a statement without a rule answers empty and successful.
	class CScriptedDriver final : public Db::IDbDriver
	{
	public:
		CScriptedDriver& Rule(std::string SqlContains, int DelayMs)
		{
			std::lock_guard<std::mutex> Lock(m_Mutex);
			m_vRules.push_back({ std::move(SqlContains), DelayMs });
			return *this;
		}

		CScriptedDriver& FailOnce(std::string SqlContains, int Code, std::string SqlState)
		{
			std::lock_guard<std::mutex> Lock(m_Mutex);
			m_FailOnceContains = std::move(SqlContains);
			m_FailOnceCode = Code;
			m_FailOnceState = std::move(SqlState);
			m_FailureIssued = false;
			return *this;
		}

		std::unique_ptr<Db::IDbConnection> Connect(const Db::SEndpoint& Endpoint, int TimeoutMs, Db::SError&) override
		{
			(void)Endpoint;
			(void)TimeoutMs;
			++m_Connections;
			return std::make_unique<CScriptedConnection>(*this);
		}

		size_t CallCount() const
		{
			std::lock_guard<std::mutex> Lock(m_Mutex);
			return m_vCalls.size();
		}

		int MaxConcurrentCalls() const { return m_MaxConcurrentCalls.load(); }
		int ConnectionCount() const { return m_Connections.load(); }

		void BlockExecutions()
		{
			std::lock_guard<std::mutex> Lock(m_Mutex);
			m_BlockExecutions = true;
		}

		void ReleaseExecutions()
		{
			{
				std::lock_guard<std::mutex> Lock(m_Mutex);
				m_BlockExecutions = false;
			}
			m_Cv.notify_all();
		}

		bool WaitForCalls(size_t Count, int TimeoutMs)
		{
			std::unique_lock<std::mutex> Lock(m_Mutex);
			return m_Cv.wait_for(Lock, std::chrono::milliseconds(TimeoutMs), [this, Count] { return m_vCalls.size() >= Count; });
		}

		// Runs the statement the way the rules say, after recording it.
		void Run(const std::string& Sql, Db::CResultSet& Out)
		{
			const int Active = ++m_ActiveCalls;
			int Maximum = m_MaxConcurrentCalls.load();
			while(Active > Maximum && !m_MaxConcurrentCalls.compare_exchange_weak(Maximum, Active))
			{
			}
			struct CActiveCallGuard
			{
				std::atomic<int>& m_Active;
				~CActiveCallGuard() { --m_Active; }
			} ActiveGuard { m_ActiveCalls };

			int DelayMs = 0;
			int ErrorCode = 0;
			std::string ErrorState;
			{
				std::lock_guard<std::mutex> Lock(m_Mutex);
				m_vCalls.push_back(Sql);
				for(const SRule& Rule_ : m_vRules)
				{
					if(Sql.find(Rule_.m_SqlContains) != std::string::npos)
					{
						DelayMs = Rule_.m_DelayMs;
						break;
					}
				}
				if(!m_FailureIssued && !m_FailOnceContains.empty() && Sql.find(m_FailOnceContains) != std::string::npos)
				{
					m_FailureIssued = true;
					ErrorCode = m_FailOnceCode;
					ErrorState = m_FailOnceState;
				}
			}
			m_Cv.notify_all();
			{
				std::unique_lock<std::mutex> Lock(m_Mutex);
				m_Cv.wait(Lock, [this] { return !m_BlockExecutions; });
			}

			if(DelayMs > 0)
				std::this_thread::sleep_for(std::chrono::milliseconds(DelayMs));

			if(ErrorCode != 0)
			{
				Out = Db::CResultSet::Failure(Db::SError { ErrorCode, ErrorState, "scripted database failure" });
				return;
			}
			Out = Db::CResultSet();
			Out.SetColumns({ "value" });
			Out.AddRow({ std::string("answer") });
		}

	private:
		mutable std::mutex m_Mutex;
		std::condition_variable m_Cv;
		std::vector<SRule> m_vRules;
		std::vector<std::string> m_vCalls;
		std::atomic<int> m_Connections { 0 };
		std::atomic<int> m_ActiveCalls { 0 };
		std::atomic<int> m_MaxConcurrentCalls { 0 };
		bool m_BlockExecutions = false;
		std::string m_FailOnceContains;
		std::string m_FailOnceState;
		int m_FailOnceCode = 0;
		bool m_FailureIssued = false;
	};

	class CExecutionGateGuard
	{
	public:
		explicit CExecutionGateGuard(CScriptedDriver& Driver) : m_Driver(Driver) {}
		~CExecutionGateGuard() { m_Driver.ReleaseExecutions(); }

	private:
		CScriptedDriver& m_Driver;
	};

	Db::CResultSet CScriptedConnection::Execute(std::string_view Sql, const Db::CBindings& Bindings, bool WantInsertId)
	{
		(void)Bindings;
		(void)WantInsertId;
		Db::CResultSet Result;
		m_pDriver.Run(std::string(Sql), Result);
		if(Result.Error().m_Code == 2006 || Result.Error().m_Code == 2013 || Result.Error().m_Code == 2055)
			m_Broken = true;
		return Result;
	}

	Db::SExecutorConfig MakeConfig(Db::IDbDriver* pDriver)
	{
		Db::SExecutorConfig Config;
		Config.m_pDriver = pDriver;
		Config.m_Endpoint.m_Name = "remote";
		Config.m_MaxConnections = 2;
		Config.m_AcquireTimeoutMs = 3000;
		Config.m_ConnectRetryDelayMs = 0;
		Config.m_IdlePingMs = 0;
		Config.m_Workers = 2;
		Config.m_MaxQueue = 100;
		Config.m_QueueWarnSize = 0;
		Config.m_SelectRetries = 0;
		Config.m_RequestTimeoutMs = 5000;
		return Config;
	}
}

class CDbQueueTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		const auto Config = MakeConfig(&m_Driver);
		ASSERT_TRUE(Db::Start(Config, Config));
	}

	void TearDown() override { Db::Stop(3000); }

	CScriptedDriver m_Driver;
};

// The point of the coalescing: a menu every player opens at once costs one
// statement, and every request still gets the rows.
TEST_F(CDbQueueTest, IdenticalReadsShareOneStatement)
{
	m_Driver.Rule("SELECT shared_answer", 200);

	std::vector<Db::CRequest> vRequests;
	for(int i = 0; i < 5; ++i)
		vRequests.push_back(Db::Query(Db::EKind::Remote, "SELECT shared_answer FROM tw_something", {}, Db::SQueryOptions().Coalesce()));

	int Answered = 0;
	for(Db::CRequest& Request : vRequests)
	{
		Db::CResultSet Result = Request.Result(5000);
		ASSERT_TRUE(Result.Ok()) << Result.Error().ToString();
		ASSERT_TRUE(Result.HasRows());
		EXPECT_EQ(Result.GetString("value"), "answer");
		++Answered;
	}

	EXPECT_EQ(Answered, 5);
	EXPECT_EQ(m_Driver.CallCount(), 1u) << "five identical reads must cost one statement";

	auto pExecutor = Db::Executor(Db::EKind::Remote);
	ASSERT_NE(pExecutor, nullptr);
	EXPECT_EQ(pExecutor->Stats().m_Coalesced, 4u);

	// The answer is not remembered: the next identical read runs again, so a
	// write that landed in between is seen.
	EXPECT_TRUE(Db::Query(Db::EKind::Remote, "SELECT shared_answer FROM tw_something", {}, Db::SQueryOptions().Coalesce()).Result(5000).Ok());
	EXPECT_EQ(m_Driver.CallCount(), 2u);
}

TEST_F(CDbQueueTest, TaggedQueriesNotifyAfterEveryMatchingCallback)
{
	m_Driver.BlockExecutions();
	CExecutionGateGuard GateGuard(m_Driver);

	constexpr std::string_view Tag = "test_shared_tag";
	const auto Options = Db::SQueryOptions().Tag(Tag);
	int CallbackCount = 0;
	int WatcherCount = 0;
	std::thread::id WatcherThread;

	auto First = Db::Query(Db::EKind::Remote, "SELECT tagged_first", {}, Options);
	auto Second = Db::Query(Db::EKind::Remote, "SELECT tagged_second", {}, Options);
	ASSERT_TRUE(m_Driver.WaitForCalls(2, 3000));
	EXPECT_TRUE(Db::IsTagRunning(Tag));
	EXPECT_TRUE(Db::WatchTag(Tag, [&]
	{
		++WatcherCount;
		WatcherThread = std::this_thread::get_id();
	}));

	First.Then([&](Db::CResultSet&& Result)
	{
		EXPECT_TRUE(Result.Ok());
		++CallbackCount;
	});
	Second.Then([&](Db::CResultSet&& Result)
	{
		EXPECT_TRUE(Result.Ok());
		++CallbackCount;
	});

	m_Driver.ReleaseExecutions();
	for(int i = 0; i < 500 && WatcherCount == 0; ++i)
	{
		Async::RunMainThreadTasks();
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
	Async::RunMainThreadTasks();

	EXPECT_EQ(CallbackCount, 2);
	EXPECT_EQ(WatcherCount, 1);
	EXPECT_EQ(WatcherThread, std::this_thread::get_id());
	EXPECT_FALSE(Db::IsTagRunning(Tag));
}

TEST_F(CDbQueueTest, AllowsASingleTrailingStatementTerminator)
{
	const auto Result = Db::Query(Db::EKind::Remote, "SHOW TABLES;").Result(5000);
	EXPECT_TRUE(Result.Ok());
	EXPECT_EQ(m_Driver.CallCount(), 1u);
}

TEST_F(CDbQueueTest, ReadsAreNotSharedUnlessAskedFor)
{
	m_Driver.Rule("SELECT own_answer", 200);

	std::vector<Db::CRequest> vRequests;
	for(int i = 0; i < 3; ++i)
		vRequests.push_back(Db::Query(Db::EKind::Remote, "SELECT own_answer FROM tw_something"));

	for(Db::CRequest& Request : vRequests)
		EXPECT_TRUE(Request.Result(5000).Ok());

	EXPECT_EQ(m_Driver.CallCount(), 3u) << "a read that must see our own writes runs every time";
}

TEST_F(CDbQueueTest, RetriesAReadAfterConnectionLossButNeverReplaysAWrite)
{
	m_Driver.FailOnce("SELECT retry_read", 2013, "HY000");
	const auto Read = Db::Query(Db::EKind::Remote, "SELECT retry_read FROM tw_something", {}, Db::SQueryOptions().Retries(2)).Result(5000);
	ASSERT_TRUE(Read.Ok()) << Read.Error().ToString();
	EXPECT_EQ(m_Driver.CallCount(), 2u);

	m_Driver.FailOnce("INSERT INTO retry_write", 2013, "HY000");
	const auto Write = Db::Query(Db::EKind::Remote, "INSERT INTO retry_write (Value) VALUES (1)", {}, Db::SQueryOptions().Retries(2)).Result(5000);
	EXPECT_FALSE(Write.Ok());
	EXPECT_EQ(m_Driver.CallCount(), 3u) << "a write with an unknown outcome must not be replayed";

	m_Driver.FailOnce("SELECT 1 INTO OUTFILE", 2013, "HY000");
	const auto StatefulRead = Db::Query(Db::EKind::Remote, "SELECT 1 INTO OUTFILE '/tmp/result'", {}, Db::SQueryOptions().Retries(2)).Result(5000);
	EXPECT_FALSE(StatefulRead.Ok());
	EXPECT_EQ(m_Driver.CallCount(), 4u) << "a SELECT with a side effect must not use automatic retry";
}

TEST_F(CDbQueueTest, RetriesDeadlocksButNotOrdinarySqlErrors)
{
	m_Driver.FailOnce("SELECT retry_deadlock", 1213, "40001");
	const auto Deadlock = Db::Query(Db::EKind::Remote, "SELECT retry_deadlock FROM tw_something", {}, Db::SQueryOptions().Retries(1)).Result(5000);
	ASSERT_TRUE(Deadlock.Ok()) << Deadlock.Error().ToString();
	EXPECT_EQ(m_Driver.CallCount(), 2u);

	m_Driver.FailOnce("SELECT syntax_error", 1064, "42000");
	const auto SqlError = Db::Query(Db::EKind::Remote, "SELECT syntax_error FROM tw_something", {}, Db::SQueryOptions().Retries(3)).Result(5000);
	EXPECT_FALSE(SqlError.Ok());
	EXPECT_EQ(m_Driver.CallCount(), 3u) << "syntax errors are not transient and must not be retried";

	m_Driver.FailOnce("SELECT retry_lock_wait", 1205, "HY000");
	const auto LockWait = Db::Query(Db::EKind::Remote, "SELECT retry_lock_wait FROM tw_something", {}, Db::SQueryOptions().Retries(1)).Result(5000);
	EXPECT_TRUE(LockWait.Ok()) << LockWait.Error().ToString();
	EXPECT_EQ(m_Driver.CallCount(), 5u) << "a lock-wait timeout is retryable only for safe reads";
}

TEST_F(CDbQueueTest, DropsAndReopensAConnectionLostDuringExecution)
{
	m_Driver.FailOnce("SELECT connection_lost", 2013, "HY000");
	const auto Result = Db::Query(Db::EKind::Remote, "SELECT connection_lost FROM tw_something", {}, Db::SQueryOptions().Retries(1)).Result(5000);
	ASSERT_TRUE(Result.Ok()) << Result.Error().ToString();
	EXPECT_EQ(m_Driver.CallCount(), 2u);
	EXPECT_GE(m_Driver.ConnectionCount(), 2);
	EXPECT_EQ(Db::Pool(Db::EKind::Remote)->Stats().m_Busy, 0);
	EXPECT_LE(Db::Pool(Db::EKind::Remote)->Stats().m_Open, 2);
}

TEST_F(CDbQueueTest, RetriesConnectionAcquisitionWhenPoolIsTemporarilyBusy)
{
	Db::Stop(3000);
	auto Config = MakeConfig(&m_Driver);
	Config.m_MaxConnections = 1;
	Config.m_Workers = 1;
	Config.m_AcquireTimeoutMs = 40;
	Config.m_RequestTimeoutMs = 600;
	Config.m_RetryBaseDelayMs = 20;
	Config.m_RetryMaxDelayMs = 40;
	ASSERT_TRUE(Db::Start(Config, Config));

	auto HeldLease = Db::Pool(Db::EKind::Remote)->Acquire(100);
	ASSERT_TRUE(HeldLease.has_value());

	auto Request = Db::Query(Db::EKind::Remote, "SELECT after_pool_contention");
	std::this_thread::sleep_for(std::chrono::milliseconds(80));
	HeldLease->Release();

	const Db::CResultSet Result = Request.Result(1000);
	ASSERT_TRUE(Result.Ok()) << Result.Error().ToString();
	EXPECT_EQ(m_Driver.CallCount(), 1u) << "the SQL job must run once after a lease becomes available";
	EXPECT_GE(Db::Executor(Db::EKind::Remote)->Stats().m_Retries, 1u);
	EXPECT_EQ(Db::Pool(Db::EKind::Remote)->Stats().m_Busy, 0);
}

TEST_F(CDbQueueTest, ReadsWithDifferentParametersAreNotShared)
{
	m_Driver.Rule("SELECT by_id", 200);

	Db::CRequest First = Db::Query(Db::EKind::Remote, "SELECT by_id FROM tw_something WHERE ID = ?", Db::Bind(1), Db::SQueryOptions().Coalesce());
	Db::CRequest Second = Db::Query(Db::EKind::Remote, "SELECT by_id FROM tw_something WHERE ID = ?", Db::Bind(2), Db::SQueryOptions().Coalesce());

	EXPECT_TRUE(First.Result(5000).Ok());
	EXPECT_TRUE(Second.Result(5000).Ok());
	EXPECT_EQ(m_Driver.CallCount(), 2u);
}

TEST_F(CDbQueueTest, LockingReadsAreNotCoalesced)
{
	m_Driver.Rule("SELECT locked", 200);
	const Db::SQueryOptions Options = Db::SQueryOptions().Coalesce();
	std::vector<Db::CRequest> vRequests;
	for(const std::string Sql : {
		"SELECT locked FROM tw_something FOR UPDATE",
		"SELECT locked FROM tw_something FOR\nSHARE",
		"SELECT locked FROM tw_something LOCK IN SHARE MODE",
	})
	{
		vRequests.push_back(Db::Query(Db::EKind::Remote, Sql, {}, Options));
		vRequests.push_back(Db::Query(Db::EKind::Remote, Sql, {}, Options));
	}

	for(Db::CRequest& Request : vRequests)
		EXPECT_TRUE(Request.Result(5000).Ok());
	EXPECT_EQ(m_Driver.CallCount(), 6u);
}

TEST_F(CDbQueueTest, TransactionRejectsControlStatementsAndNestedRequests)
{
	EXPECT_FALSE(Db::Query(Db::EKind::Remote, "COMMIT").Result(5000).Ok());
	EXPECT_FALSE(Db::Query(Db::EKind::Remote, "SET autocommit = 0").Result(5000).Ok());
	EXPECT_EQ(m_Driver.CallCount(), 0u) << "pool/session control cannot be run as an ordinary query";

	bool ControlRejected = false;
	const auto ControlStatement = Db::Transaction(Db::EKind::Remote, [&ControlRejected](Db::CTransaction& Tx)
	{
		ControlRejected = !Tx.Execute("COMMIT").Ok();
		return true;
	}).Result(5000);
	EXPECT_FALSE(ControlStatement.Ok());
	EXPECT_TRUE(ControlRejected);
	EXPECT_EQ(m_Driver.CallCount(), 3u) << "only isolation setup, BEGIN, and rollback may reach the driver";

	bool NestedRejected = false;
	const auto NestedRequest = Db::Transaction(Db::EKind::Remote, [&NestedRejected](Db::CTransaction& Tx)
	{
		NestedRejected = !Db::Query(Db::EKind::Remote, "SELECT nested").Result(100).Ok();
		return false;
	}).Result(5000);
	EXPECT_FALSE(NestedRequest.Ok());
	EXPECT_TRUE(NestedRejected);
	EXPECT_EQ(m_Driver.CallCount(), 6u) << "the nested query must never enter the work queue";
}

TEST_F(CDbQueueTest, ExceptionInTransactionBodyRollsBackAndReleasesLease)
{
	const auto Failed = Db::Transaction(Db::EKind::Remote, [](Db::CTransaction&) -> bool
	{
		throw std::runtime_error("scripted transaction body exception");
	}).Result(5000);
	EXPECT_FALSE(Failed.Ok());
	EXPECT_EQ(m_Driver.CallCount(), 3u) << "isolation setup, BEGIN and ROLLBACK should execute";
	EXPECT_EQ(Db::Pool(Db::EKind::Remote)->Stats().m_Busy, 0);

	const auto Later = Db::Query(Db::EKind::Remote, "SELECT after_exception").Result(5000);
	EXPECT_TRUE(Later.Ok());
	EXPECT_EQ(m_Driver.CallCount(), 4u) << "the connection must be usable again after a confirmed rollback";
}

TEST(CDbErrorClassificationTest, SeparatesAuthenticationTransportAndSqlFailures)
{
	EXPECT_TRUE(Db::IsAuthenticationError(1045, "28000"));
	EXPECT_TRUE(Db::IsConnectionLostError(2013, "HY000"));
	EXPECT_TRUE(Db::IsServerUnavailableError(2003, "HY000"));
	EXPECT_TRUE(Db::IsDeadlockError(1213, "40001"));
	EXPECT_TRUE(Db::IsLockWaitTimeoutError(1205, "HY000"));
	EXPECT_FALSE(Db::IsConnectionLostError(1064, "42000"));
	EXPECT_FALSE(Db::IsServerUnavailableError(1045, "28000"));
}

TEST(CDbDebugLoggingTest, WorkerFlagCanBeToggledSafely)
{
	Db::SetDebugLoggingEnabled(true);
	EXPECT_TRUE(Db::DebugLoggingEnabled());
	Db::SetDebugLoggingEnabled(false);
	EXPECT_FALSE(Db::DebugLoggingEnabled());
}

TEST(CDbResultSetTest, RefillResetsTheCursorAndOutcome)
{
	Db::CResultSet Result;
	Result.SetColumns({ "value" });
	Result.AddRow({ "first" });
	EXPECT_EQ(Result.GetString("value"), "first"); // peek first row without Next()

	Result.SetError({ 1, "HY000", "old failure" });
	Result.SetAffectedRows(9);
	Result.SetInsertId(12);
	Result.SetColumns({ "value" });
	Result.AddRow({ "replacement" });

	EXPECT_TRUE(Result.Ok());
	EXPECT_EQ(Result.AffectedRows(), 0u);
	EXPECT_EQ(Result.InsertId(), 0);
	ASSERT_TRUE(Result.Next());
	EXPECT_EQ(Result.GetString("value"), "replacement");
}

TEST_F(CDbQueueTest, WritesAreNeverShared)
{
	m_Driver.Rule("INSERT INTO tw_something", 200);

	std::vector<Db::CRequest> vRequests;
	for(int i = 0; i < 3; ++i)
		vRequests.push_back(Db::Query(Db::EKind::Remote, "INSERT INTO tw_something (Value) VALUES (?)", Db::Bind(7), Db::SQueryOptions().Coalesce()));

	for(Db::CRequest& Request : vRequests)
		EXPECT_TRUE(Request.Result(5000).Ok());

	EXPECT_EQ(m_Driver.CallCount(), 3u) << "coalescing is for reads only, a shared write would be a lost row";
}

TEST_F(CDbQueueTest, HighVolumeConcurrentRequestsRespectWorkerAndConnectionCaps)
{
	Db::Stop(3000);
	auto Config = MakeConfig(&m_Driver);
	Config.m_MaxConnections = 4;
	Config.m_Workers = 4;
	Config.m_MaxQueue = 1024;
	Config.m_RequestTimeoutMs = 15000;
	Config.m_AcquireTimeoutMs = 5000;
	ASSERT_TRUE(Db::Start(Config, Config));
	m_Driver.Rule("SELECT load", 4);

	constexpr int REQUESTS = 1000;
	constexpr int PRODUCERS = 8;
	constexpr int REQUESTS_PER_PRODUCER = REQUESTS / PRODUCERS;
	std::vector<Db::CRequest> vRequests(REQUESTS);
	std::vector<std::thread> vProducers;
	for(int Producer = 0; Producer < PRODUCERS; ++Producer)
	{
		vProducers.emplace_back([Producer, &vRequests]
		{
			for(int i = 0; i < REQUESTS_PER_PRODUCER; ++i)
			{
				const int Index = Producer * REQUESTS_PER_PRODUCER + i;
				const std::string Sql = "SELECT load FROM stress_table WHERE ID = " + std::to_string(Index);
				vRequests[Index] = Db::Query(Db::EKind::Remote, Sql);
			}
		});
	}
	for(std::thread& Producer : vProducers)
		Producer.join();

	int Succeeded = 0;
	for(Db::CRequest& Request : vRequests)
		Succeeded += Request.Result(20000).Ok();
	EXPECT_EQ(Succeeded, REQUESTS);

	auto pExecutor = Db::Executor(Db::EKind::Remote);
	auto pPool = Db::Pool(Db::EKind::Remote);
	ASSERT_NE(pExecutor, nullptr);
	ASSERT_NE(pPool, nullptr);
	EXPECT_EQ(pExecutor->Stats().m_Submitted, REQUESTS);
	EXPECT_LE(pExecutor->Stats().m_QueueMax, static_cast<size_t>(Config.m_MaxQueue));
	EXPECT_LE(pPool->Stats().m_Open, Config.m_MaxConnections);
	EXPECT_EQ(pPool->Stats().m_Busy, 0);
	EXPECT_EQ(m_Driver.CallCount(), REQUESTS);
	EXPECT_LE(m_Driver.MaxConcurrentCalls(), Config.m_MaxConnections);
	EXPECT_GT(m_Driver.MaxConcurrentCalls(), 1) << "load should exercise parallel statement execution";
}

TEST_F(CDbQueueTest, QueueOverflowRejectsWithoutBlockingAndDrainsCleanly)
{
	Db::Stop(3000);
	auto Config = MakeConfig(&m_Driver);
	Config.m_MaxConnections = 1;
	Config.m_Workers = 1;
	Config.m_MaxQueue = 8;
	Config.m_RequestTimeoutMs = 15000;
	ASSERT_TRUE(Db::Start(Config, Config));

	m_Driver.BlockExecutions();
	CExecutionGateGuard GateGuard(m_Driver);
	auto Running = Db::Query(Db::EKind::Remote, "SELECT overload active");
	ASSERT_TRUE(m_Driver.WaitForCalls(1, 3000));

	constexpr int SUBMISSIONS = 1000;
	std::vector<Db::CRequest> vRequests;
	vRequests.reserve(SUBMISSIONS);
	for(int i = 0; i < SUBMISSIONS; ++i)
	{
		const std::string Sql = "SELECT overload queued WHERE ID = " + std::to_string(i);
		vRequests.push_back(Db::Query(Db::EKind::Remote, Sql));
	}
	ASSERT_EQ(Db::Executor(Db::EKind::Remote)->QueuedCount(), static_cast<size_t>(Config.m_MaxQueue));
	EXPECT_LE(Db::Executor(Db::EKind::Remote)->Stats().m_QueueMax, static_cast<size_t>(Config.m_MaxQueue));

	m_Driver.ReleaseExecutions();
	EXPECT_TRUE(Running.Result(5000).Ok());
	int Succeeded = 0;
	int Rejected = 0;
	for(Db::CRequest& Request : vRequests)
	{
		const Db::CResultSet Result = Request.Result(10000);
		if(Result.Ok())
			++Succeeded;
		else if(Result.Error().ToString().find("queue is full") != std::string::npos)
			++Rejected;
	}
	EXPECT_EQ(Succeeded, Config.m_MaxQueue);
	EXPECT_EQ(Rejected, SUBMISSIONS - Config.m_MaxQueue);
	EXPECT_EQ(m_Driver.CallCount(), static_cast<size_t>(Config.m_MaxQueue + 1));
	EXPECT_EQ(Db::Pool(Db::EKind::Remote)->Stats().m_Busy, 0);
	EXPECT_LE(Db::Pool(Db::EKind::Remote)->Stats().m_Open, 1);
}

TEST_F(CDbQueueTest, ConcurrentTransactionsFinishWithoutHoldingLeasesAcrossBodies)
{
	Db::Stop(3000);
	auto Config = MakeConfig(&m_Driver);
	Config.m_MaxConnections = 2;
	Config.m_Workers = 2;
	ASSERT_TRUE(Db::Start(Config, Config));

	std::mutex GateMutex;
	std::condition_variable GateCv;
	int EnteredBodies = 0;
	auto Body = [&GateMutex, &GateCv, &EnteredBodies](Db::CTransaction& Tx)
	{
		std::unique_lock<std::mutex> Lock(GateMutex);
		++EnteredBodies;
		GateCv.notify_all();
		const bool BothEntered = GateCv.wait_for(Lock, std::chrono::seconds(3), [&EnteredBodies] { return EnteredBodies == 2; });
		Lock.unlock();
		if(!BothEntered)
			return false;
		return Tx.Execute("UPDATE stress_table SET Value = 1 WHERE ID = 1").Ok();
	};

	auto First = Db::Transaction(Db::EKind::Remote, Body);
	auto Second = Db::Transaction(Db::EKind::Remote, Body);
	const Db::CResultSet FirstResult = First.Result(7000);
	const Db::CResultSet SecondResult = Second.Result(7000);
	EXPECT_TRUE(FirstResult.Ok()) << FirstResult.Error().ToString();
	EXPECT_TRUE(SecondResult.Ok()) << SecondResult.Error().ToString();
	EXPECT_EQ(EnteredBodies, 2);
	EXPECT_EQ(Db::Pool(Db::EKind::Remote)->Stats().m_Busy, 0);
	EXPECT_LE(Db::Pool(Db::EKind::Remote)->Stats().m_Open, 2);
	EXPECT_EQ(m_Driver.CallCount(), 8u) << "each transaction must execute isolation, BEGIN, body and COMMIT";
}

TEST_F(CDbQueueTest, ShutdownCompletesActiveAndQueuedRequests)
{
	m_Driver.BlockExecutions();
	CExecutionGateGuard GateGuard(m_Driver);
	std::vector<Db::CRequest> vRequests;
	vRequests.push_back(Db::Query(Db::EKind::Remote, "SELECT shutdown active"));
	ASSERT_TRUE(m_Driver.WaitForCalls(1, 3000));
	for(int i = 0; i < 8; ++i)
		vRequests.push_back(Db::Query(Db::EKind::Remote, "SELECT shutdown queued " + std::to_string(i)));

	std::atomic<bool> StopReturned { false };
	std::thread Stopper([&StopReturned] {
		Db::Stop(3000);
		StopReturned = true;
	});
	for(int i = 0; i < 3000 && Db::IsStarted(); ++i)
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	EXPECT_FALSE(StopReturned.load()) << "shutdown must wait for the active database call";
	m_Driver.ReleaseExecutions();
	Stopper.join();
	EXPECT_TRUE(StopReturned.load());

	for(Db::CRequest& Request : vRequests)
		EXPECT_TRUE(Request.Result(5000).Ok());
	EXPECT_EQ(m_Driver.CallCount(), 9u);
}

TEST(CDbQueueCancellationTest, CancellingQueuedWorkPreventsItsSqlFromRunning)
{
	CScriptedDriver Driver;
	Driver.Rule("SELECT first", 150);
	Db::SExecutorConfig Config = MakeConfig(&Driver);
	Config.m_MaxConnections = 1;
	Config.m_Workers = 1;
	Config.m_MaxQueue = 2;
	ASSERT_TRUE(Db::Start(Config, Config));

	auto Running = Db::Query(Db::EKind::Remote, "SELECT first");
	for(int i = 0; i < 200 && Driver.CallCount() == 0; ++i)
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	ASSERT_EQ(Driver.CallCount(), 1u);
	auto Queued = Db::Query(Db::EKind::Remote, "SELECT should_not_run");
	Queued.Cancel();

	EXPECT_TRUE(Running.Result(3000).Ok());
	const Db::CResultSet Cancelled = Queued.Result(3000);
	EXPECT_FALSE(Cancelled.Ok());
	EXPECT_NE(Cancelled.Error().ToString().find("cancelled"), std::string::npos);
	EXPECT_EQ(Driver.CallCount(), 1u);
	Db::Stop(3000);
}

TEST(CDbPoolBackoffTest, RetriesTransientConnectFailureWithinAcquireDeadline)
{
	CTransientConnectDriver Driver;
	Db::SPoolConfig Config;
	Config.m_MaxConnections = 1;
	Config.m_AcquireTimeoutMs = 500;
	Config.m_ConnectRetryDelayMs = 50;
	Config.m_IdlePingMs = 0;
	Db::CDbPool Pool(Driver, Config);

	auto Lease = Pool.Acquire(500);
	ASSERT_TRUE(Lease.has_value());
	EXPECT_EQ(Driver.Connects(), 2);
	EXPECT_EQ(Pool.Stats().m_Failed, 1u);
	EXPECT_EQ(Pool.Stats().m_Open, 1);
	EXPECT_EQ(Pool.Stats().m_Busy, 1);
	Lease->Release();
}

TEST(CDbPoolBackoffTest, ReusesReturnedConnectionDuringFailedConnectBackoff)
{
	CFirstConnectOnlyDriver Driver;
	Db::SPoolConfig Config;
	Config.m_MaxConnections = 2;
	Config.m_ConnectRetryDelayMs = 2000;
	Config.m_ConnectRetryMaxDelayMs = 2000;
	Config.m_IdlePingMs = 0;
	Db::CDbPool Pool(Driver, Config);
	auto Held = Pool.Acquire(500);
	ASSERT_TRUE(Held.has_value());
	std::atomic<bool> Acquired{ false };
	std::thread Worker([&] { auto Lease = Pool.Acquire(1000); Acquired = Lease.has_value(); });
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
	// Wait until the failed attempt has completed AND the worker has reserved
	// a slot for the retry. Otherwise the old implementation could pass if the
	// connection were returned before it entered the backoff path.
	while (std::chrono::steady_clock::now() < Deadline)
	{
		const auto Stats = Pool.Stats();
		if (Stats.m_Failed > 0 && Stats.m_Busy == 2)
			break;
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	const auto BeforeRelease = Pool.Stats();
	const bool Failed = BeforeRelease.m_Failed > 0 && BeforeRelease.m_Busy == 2;
	Held->Release();
	Worker.join();
	EXPECT_TRUE(Failed);
	EXPECT_TRUE(Acquired.load());
	EXPECT_EQ(Driver.m_Connects.load(), 2);
	EXPECT_EQ(Pool.Stats().m_Busy, 0);
	EXPECT_EQ(Pool.Stats().m_Open, 1);
}

TEST(CDbPoolBackoffTest, ShutdownWakesBackoffWaitersAndReleasesReservations)
{
	CFirstConnectOnlyDriver Driver;
	Db::SPoolConfig Config;
	Config.m_MaxConnections = 2;
	Config.m_ConnectRetryDelayMs = 2000;
	Config.m_ConnectRetryMaxDelayMs = 2000;
	Config.m_IdlePingMs = 0;
	Db::CDbPool Pool(Driver, Config);
	auto Held = Pool.Acquire(500);
	ASSERT_TRUE(Held.has_value());
	std::atomic<bool> Acquired{ false };
	std::thread Worker([&] { auto Lease = Pool.Acquire(1500); Acquired = Lease.has_value(); });
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
	while (std::chrono::steady_clock::now() < Deadline)
	{
		const auto Stats = Pool.Stats();
		if (Stats.m_Failed > 0 && Stats.m_Busy == 2)
			break;
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	const auto BeforeStop = Pool.Stats();
	Pool.Shutdown(0);
	Held->Release();
	Worker.join();
	EXPECT_TRUE(BeforeStop.m_Failed > 0 && BeforeStop.m_Busy == 2);
	EXPECT_FALSE(Acquired.load());
	EXPECT_EQ(Pool.Stats().m_Busy, 0);
	EXPECT_EQ(Pool.Stats().m_Open, 0);
}

TEST(CDbPoolBackoffTest, ReusesReturnedConnectionWhileAnotherConnectIsInProgress)
{
	CBlockingSecondConnectDriver Driver;
	Db::SPoolConfig Config;
	Config.m_MaxConnections = 3;
	Config.m_ConnectRetryDelayMs = 0;
	Config.m_IdlePingMs = 0;
	Db::CDbPool Pool(Driver, Config);
	auto Held = Pool.Acquire(500);
	ASSERT_TRUE(Held.has_value());
	std::thread Connecting([&] { Pool.Acquire(2000); });
	const auto ConnectDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
	while (!Driver.m_ConnectStarted && std::chrono::steady_clock::now() < ConnectDeadline)
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	std::atomic<bool> Acquired{ false };
	std::thread Waiting([&] { auto Lease = Pool.Acquire(1500); Acquired = Lease.has_value(); });
	const auto WaitDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
	while (Pool.Stats().m_Busy < 3 && std::chrono::steady_clock::now() < WaitDeadline)
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	const bool BothWaiting = Driver.m_ConnectStarted && Pool.Stats().m_Busy == 3;
	Held->Release();
	const auto ReuseDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(700);
	while (!Acquired && std::chrono::steady_clock::now() < ReuseDeadline)
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	const bool ReusedBeforeConnectFinished = Acquired.load();
	Driver.m_ReleaseConnect = true;
	Connecting.join();
	Waiting.join();
	EXPECT_TRUE(BothWaiting);
	EXPECT_TRUE(ReusedBeforeConnectFinished);
	EXPECT_EQ(Driver.m_Connects.load(), 2);
	EXPECT_EQ(Pool.Stats().m_Busy, 0);
	EXPECT_EQ(Pool.Stats().m_Open, 1);
}

TEST(CDbPoolBackoffTest, WaitsForTheConfiguredConnectBackoff)
{
	CFailingDriver Driver;
	Db::SPoolConfig Config;
	Config.m_MaxConnections = 2;
	Config.m_AcquireTimeoutMs = 1000;
	Config.m_ConnectRetryDelayMs = 400;
	Config.m_IdlePingMs = 0;
	Db::CDbPool Pool(Driver, Config);

	EXPECT_FALSE(Pool.Acquire(500).has_value());
	const auto Started = std::chrono::steady_clock::now();
	EXPECT_FALSE(Pool.Acquire(800).has_value());
	const auto Waited = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - Started).count();

	EXPECT_EQ(Driver.Connects(), 3);
	EXPECT_GE(Waited, 300) << "connection attempts must respect the configured backoff";
	EXPECT_EQ(Pool.Stats().m_Busy, 0);
	EXPECT_EQ(Pool.Stats().m_Open, 0);
}

TEST(CDbPoolBackoffTest, ConnectAttemptHonorsAcquireDeadline)
{
	CFailingDriver Driver(1000);
	Db::SPoolConfig Config;
	Config.m_MaxConnections = 1;
	Config.m_AcquireTimeoutMs = 100;
	Config.m_ConnectRetryDelayMs = 0;
	Config.m_IdlePingMs = 0;
	Db::CDbPool Pool(Driver, Config);

	const auto Started = std::chrono::steady_clock::now();
	EXPECT_FALSE(Pool.Acquire(80).has_value());
	const auto Elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - Started).count();
	EXPECT_LT(Elapsed, 300) << "opening an unavailable endpoint must not block past the acquire budget";
	EXPECT_EQ(Pool.Stats().m_Busy, 0);
	EXPECT_EQ(Pool.Stats().m_Open, 0);
}

TEST(CDbPoolSafetyTest, RejectsAClosedConnectionBeforeLeasingIt)
{
	CClosedConnectionDriver Driver;
	Db::SPoolConfig Config;
	Config.m_MaxConnections = 1;
	Config.m_AcquireTimeoutMs = 100;
	Config.m_ConnectRetryDelayMs = 0;
	Config.m_IdlePingMs = 0;
	Db::CDbPool Pool(Driver, Config);

	EXPECT_FALSE(Pool.Acquire(100).has_value());
	const auto Stats = Pool.Stats();
	EXPECT_EQ(Stats.m_Open, 0);
	EXPECT_EQ(Stats.m_Busy, 0);
	EXPECT_EQ(Stats.m_Created, 0u);
	EXPECT_EQ(Stats.m_Failed, 1u);
}

TEST(CDbPoolSafetyTest, LeaseReleaseIsIdempotent)
{
	CRecoverableDriver Driver;
	Db::SPoolConfig Config;
	Config.m_MaxConnections = 1;
	Config.m_AcquireTimeoutMs = 300;
	Config.m_ConnectRetryDelayMs = 0;
	Config.m_IdlePingMs = 0;
	Db::CDbPool Pool(Driver, Config);

	auto Lease = Pool.Acquire(200);
	ASSERT_TRUE(Lease.has_value());
	Lease->Release();
	Lease->Release();
	EXPECT_FALSE(static_cast<bool>(*Lease));
	EXPECT_EQ(Pool.Stats().m_Open, 1);
	EXPECT_EQ(Pool.Stats().m_Idle, 1);
	EXPECT_EQ(Pool.Stats().m_Busy, 0);
	EXPECT_EQ(Driver.ConnectCount(), 1u);
}

TEST(CDbPoolSafetyTest, ReplacesAnIdleConnectionLostByTheServer)
{
	CRecoverableDriver Driver;
	Db::SPoolConfig Config;
	Config.m_MaxConnections = 1;
	Config.m_AcquireTimeoutMs = 500;
	Config.m_ConnectRetryDelayMs = 0;
	Config.m_IdlePingMs = 0;
	Db::CDbPool Pool(Driver, Config);

	{
		auto Lease = Pool.Acquire(300);
		ASSERT_TRUE(Lease.has_value());
	}
	EXPECT_EQ(Pool.Stats().m_Open, 1);
	EXPECT_EQ(Pool.Stats().m_Idle, 1);

	Driver.BreakConnections();
	auto Recovered = Pool.Acquire(300);
	ASSERT_TRUE(Recovered.has_value());
	EXPECT_EQ(Driver.ConnectCount(), 2u);
	EXPECT_EQ(Pool.Stats().m_Open, 1);
	EXPECT_EQ(Pool.Stats().m_Busy, 1);
	EXPECT_EQ(Pool.Stats().m_Discarded, 1u);
}

TEST(CDbPoolBackoffTest, ConcurrentAcquiresSerializeConnectionAttempts)
{
	CFailingDriver Driver(60);
	Db::SPoolConfig Config;
	Config.m_MaxConnections = 8;
	Config.m_AcquireTimeoutMs = 1000;
	Config.m_ConnectRetryDelayMs = 0;
	Config.m_IdlePingMs = 0;
	Db::CDbPool Pool(Driver, Config);

	std::atomic<int> Acquired { 0 };
	std::vector<std::thread> vWorkers;
	for(int i = 0; i < 8; ++i)
	{
		vWorkers.emplace_back([&Pool, &Acquired]
			{ if(Pool.Acquire(800).has_value()) ++Acquired; });
	}
	for(std::thread& Worker : vWorkers)
		Worker.join();

	EXPECT_EQ(Acquired.load(), 0);
	EXPECT_EQ(Driver.MaxConcurrent(), 1)
		<< "a burst of workers must not stampede the database with simultaneous reconnects";
	EXPECT_EQ(Pool.Stats().m_Busy, 0);
	EXPECT_EQ(Pool.Stats().m_Open, 0);
}
