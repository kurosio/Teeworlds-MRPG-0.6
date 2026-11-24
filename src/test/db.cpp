// Tests of the connection pool and of the asynchronous database API.
//
// They run against a real MariaDB client (the one the server links) and an
// independent fake server that speaks the wire protocol (src/test/fake_mariadb.py),
// so handshake, prepared statements, error codes and disconnects are exercised.
// Tests that need the fake server but cannot have it (no python3) skip themselves.

#include "db_fake_server.h"

#include <engine/server/async.h>
#include <engine/server/db/db_async.h>
#include <engine/server/db/db_mariadb.h>
#include <engine/server/db/db_pool.h>

#include <gtest/gtest.h>

#include <atomic>
#include <cstdio>

#include <string>
#include <thread>
#include <vector>

namespace
{
	Db::SExecutorConfig MakeExecutorConfig(Db::IDbDriver* pDriver, int Port)
	{
		Db::SExecutorConfig Config;
		Config.m_pDriver = pDriver;
		Config.m_Endpoint.m_Name = "remote";
		Config.m_Endpoint.m_Host = "127.0.0.1";
		Config.m_Endpoint.m_Port = Port;
		Config.m_Endpoint.m_User = "game";
		Config.m_Endpoint.m_Password = "gamepw";
		Config.m_Endpoint.m_Database = "mrpg_shared";
		Config.m_MaxConnections = 2;
		Config.m_AcquireTimeoutMs = 3000;
		Config.m_ConnectRetryDelayMs = 100;
		Config.m_IdlePingMs = 0;
		Config.m_Workers = 2;
		Config.m_MaxQueue = 2000;
		Config.m_SelectRetries = 1;
		Config.m_RetryBaseDelayMs = 100;
		Config.m_RequestTimeoutMs = 5000;
		return Config;
	}

	// Runs a statement and waits for the answer; only tests do this, the game
	// never blocks the thread that owns game state.
	Db::CResultSet RunSql(const char* pSql, const Db::CBindings& Bindings = {}, const Db::SQueryOptions& Options = {})
	{
		return Db::Query(Db::EKind::Remote, pSql, Bindings, Options).Result(15000);
	}

	Db::CResultSet RunTransaction(std::function<bool(Db::CTransaction&)> Body)
	{
		return Db::Transaction(Db::EKind::Remote, std::move(Body)).Result(15000);
	}
}

TEST(CDbBindingsTest, BinaryBindingsOwnEmbeddedNulBytes)
{
	std::string Data("a\0b", 3);
	auto Bindings = Db::Bind(Db::Blob(Data));
	Data[0] = 'z';

	ASSERT_EQ(Bindings.Values().size(), 1u);
	const auto* pBlob = std::get_if<Db::SBlob>(&Bindings.Values()[0]);
	ASSERT_NE(pBlob, nullptr);
	ASSERT_EQ(pBlob->m_Data.size(), 3u);
	EXPECT_EQ(pBlob->m_Data[0], 'a');
	EXPECT_EQ(pBlob->m_Data[1], '\0');
	EXPECT_EQ(pBlob->m_Data[2], 'b');

	Db::CResultSet Result;
	Result.SetColumns({ "Data" });
	Result.AddRow({ std::string("a\0b", 3) });
	EXPECT_EQ(Result.GetString("Data"), std::string("a\0b", 3));
}

// ---------------------------------------------------------------------------
// the pool
// ---------------------------------------------------------------------------
class CDbPoolTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		m_pDriver = std::make_unique<Db::CMariaDbDriver>();
		m_Port = CFakeMariaDb::FindFreePort();
	}

	void TearDown() override
	{
		m_pPool.reset();
		m_pServer.reset();
	}

	bool StartServer()
	{
		m_pServer = std::make_unique<CFakeMariaDb>("remote", m_Port);
		if(!m_pServer->IsAvailable() || !m_pServer->Start())
		{
			m_Unavailable = m_pServer->UnavailableReason();
			return false;
		}
		return true;
	}

	std::unique_ptr<Db::CDbPool> MakePool(int MaxConnections, int AcquireTimeoutMs = 3000)
	{
		Db::SPoolConfig Config;
		Config.m_Endpoint.m_Name = "remote";
		Config.m_Endpoint.m_Host = "127.0.0.1";
		Config.m_Endpoint.m_Port = m_Port;
		Config.m_Endpoint.m_User = "game";
		Config.m_Endpoint.m_Password = "gamepw";
		Config.m_Endpoint.m_Database = "mrpg_shared";
		Config.m_MaxConnections = MaxConnections;
		Config.m_AcquireTimeoutMs = AcquireTimeoutMs;
		Config.m_ConnectRetryDelayMs = 100;
		Config.m_IdlePingMs = 0;
		return std::make_unique<Db::CDbPool>(*m_pDriver, Config);
	}

	std::unique_ptr<Db::CMariaDbDriver> m_pDriver;
	std::unique_ptr<CFakeMariaDb> m_pServer;
	std::unique_ptr<Db::CDbPool> m_pPool;
	int m_Port = 0;
	const char* m_Unavailable = "";
};

TEST_F(CDbPoolTest, RoundTripAndReuse)
{
	if(!StartServer())
		GTEST_SKIP() << m_Unavailable;
	m_pPool = MakePool(2);

	{
		auto Lease = m_pPool->Acquire(3000);
		ASSERT_TRUE(Lease.has_value());
		ASSERT_TRUE(Lease->Get()->IsUsable());

		Db::CResultSet Result = Lease->Get()->Execute("SELECT ROWS 3", {}, false);
		ASSERT_TRUE(Result.Ok()) << Result.Error().ToString();
		EXPECT_EQ(Result.RowCount(), 3u);
		ASSERT_TRUE(Result.Next());
		EXPECT_EQ(Result.GetInt("id"), 0);

		// the lease returns the connection when it dies
	}

	{
		auto Lease = m_pPool->Acquire(3000);
		ASSERT_TRUE(Lease.has_value());
		Db::CResultSet Result = Lease->Get()->Execute("SELECT ROWS 1", {}, false);
		ASSERT_TRUE(Result.Ok()) << Result.Error().ToString();
		EXPECT_EQ(Result.RowCount(), 1u);
	}

	const Db::CDbPool::SStats Stats = m_pPool->Stats();
	EXPECT_EQ(Stats.m_Created, 1u) << "the pool opened a second connection instead of reusing the idle one";
	EXPECT_GE(Stats.m_Reused, 1u);
	EXPECT_EQ(Stats.m_Open, 1);
	EXPECT_EQ(Stats.m_Busy, 0);
}

TEST_F(CDbPoolTest, MaxConnectionsIsAHardLimit)
{
	if(!StartServer())
		GTEST_SKIP() << m_Unavailable;
	m_pPool = MakePool(1);

	auto First = m_pPool->Acquire(3000);
	ASSERT_TRUE(First.has_value());

	// the only connection is busy: a second acquire must give up, not open another
	auto Second = m_pPool->Acquire(200);
	EXPECT_FALSE(Second.has_value());
	EXPECT_EQ(m_pPool->Stats().m_Open, 1);
	EXPECT_GE(m_pPool->Stats().m_AcquireTimeouts, 1u);

	First->Release();
	auto Third = m_pPool->Acquire(3000);
	EXPECT_TRUE(Third.has_value());
	EXPECT_EQ(m_pPool->Stats().m_Created, 1u);
}

TEST_F(CDbPoolTest, ShutdownRefusesNewLeases)
{
	if(!StartServer())
		GTEST_SKIP() << m_Unavailable;
	m_pPool = MakePool(1);

	{
		auto Lease = m_pPool->Acquire(3000);
		ASSERT_TRUE(Lease.has_value());
	}

	m_pPool->Shutdown(1000);
	EXPECT_TRUE(m_pPool->IsShutdown());
	EXPECT_FALSE(m_pPool->Acquire(100).has_value());
	// idempotent
	m_pPool->Shutdown(0);
}

TEST_F(CDbPoolTest, LeaseOutlivingThePoolIsSafe)
{
	if(!StartServer())
		GTEST_SKIP() << m_Unavailable;
	m_pPool = MakePool(1, 200);

	auto Lease = m_pPool->Acquire(3000);
	ASSERT_TRUE(Lease.has_value());

	// the lease keeps the state alive; releasing into a destroyed pool must not
	// touch freed memory
	m_pPool.reset();
	Lease->Release();
	EXPECT_FALSE(static_cast<bool>(*Lease));
}

// ---------------------------------------------------------------------------
// the asynchronous API
// ---------------------------------------------------------------------------
class CDbAsyncTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		m_pDriver = std::make_unique<Db::CMariaDbDriver>();
		m_Port = CFakeMariaDb::FindFreePort();
	}

	void TearDown() override
	{
		Db::Stop(2000);
		Async::RunMainThreadTasks();
		m_pServer.reset();
	}

	bool StartServer()
	{
		m_pServer = std::make_unique<CFakeMariaDb>("remote", m_Port);
		if(!m_pServer->IsAvailable() || !m_pServer->Start())
		{
			m_Unavailable = m_pServer->UnavailableReason();
			return false;
		}
		return true;
	}

	Db::SExecutorConfig Config()
	{
		return MakeExecutorConfig(m_pDriver.get(), m_Port);
	}

	// Starts both required pools against the fake endpoint; returns false when it cannot run.
	bool StartManager(std::function<void(Db::SExecutorConfig&)> Adjust = {})
	{
		if(!StartServer())
			return false;
		Db::SExecutorConfig ExecutorConfig = Config();
		if(Adjust)
			Adjust(ExecutorConfig);
		return Db::Start(ExecutorConfig, ExecutorConfig);
	}

	std::unique_ptr<Db::CMariaDbDriver> m_pDriver;
	std::unique_ptr<CFakeMariaDb> m_pServer;
	int m_Port = 0;
	const char* m_Unavailable = "";
};

TEST_F(CDbAsyncTest, OneCallIsTheWholeOperation)
{
	if(!StartManager())
		GTEST_SKIP() << m_Unavailable;

	// The common case: one statement, one callback, no request object, no
	// options, no resource to release. The callback runs on the game thread.
	bool Answered = false;
	std::string Server;
	Db::Query(Db::EKind::Remote, "SELECT 1").Then([&](Db::CResultSet&& Result)
	{
		Answered = true;
		Server = Result.GetString("server");
	});

	EXPECT_FALSE(Answered); // the worker cannot run game code by itself
	for(int i = 0; i < 200 && Async::PendingMainThreadTasks() == 0; ++i)
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	Async::RunMainThreadTasks();
	EXPECT_TRUE(Answered);
	EXPECT_EQ(Server, "remote");
}

TEST_F(CDbAsyncTest, RowsAffectedRowsAndInsertIdComeWithTheResult)
{
	if(!StartManager())
		GTEST_SKIP() << m_Unavailable;

	// a read: rows, and the first one is readable without Next()
	auto Rows = RunSql("SELECT ROWS 4");
	ASSERT_TRUE(Rows.Ok()) << Rows.Error().ToString();
	EXPECT_TRUE(Rows.HasRows());
	EXPECT_EQ(Rows.RowCount(), 4u);
	EXPECT_FALSE(Rows.Changed());
	EXPECT_EQ(Rows.GetString("id"), "0");
	EXPECT_EQ(Rows.GetString("val"), std::string(16, 'v'));

	// the rows after the first one are one Next() away, each of them once
	EXPECT_TRUE(Rows.Next());
	EXPECT_EQ(Rows.GetString("id"), "1");
	int RowsWalked = 1;
	while(Rows.Next())
		++RowsWalked;
	EXPECT_EQ(RowsWalked, 3);

	// a read that finds nothing is not an error, and reading it cannot crash
	auto Empty = RunSql("SELECT ROWS 0");
	ASSERT_TRUE(Empty.Ok()) << Empty.Error().ToString();
	EXPECT_FALSE(Empty.HasRows());
	EXPECT_FALSE(Empty.Changed());
	EXPECT_TRUE(Empty.GetString("id").empty());
	EXPECT_EQ(Empty.GetInt("id"), 0);

	// a write: the affected rows say it matched, the insert id comes for free
	auto Write = RunSql("UPDATE tw_accounts SET Nick = ? WHERE ID = ?", Db::Bind("someone", 7));
	ASSERT_TRUE(Write.Ok()) << Write.Error().ToString();
	EXPECT_TRUE(Write.Changed());
	EXPECT_EQ(Write.AffectedRows(), 1u);
	EXPECT_FALSE(Write.HasRows());

	// an INSERT is recognised without asking: the id is there
	auto Insert = RunSql("INSERT INTO tw_accounts (Username) VALUES (?)", Db::Bind("a"));
	ASSERT_TRUE(Insert.Ok()) << Insert.Error().ToString();
	EXPECT_GT(Insert.InsertId(), 0);

	// a statement without an id does not pay for reading one
	EXPECT_EQ(Write.InsertId(), 0);
}

TEST_F(CDbAsyncTest, ParameterisedStatementsKeepValuesOutOfSql)
{
	if(!StartManager())
		GTEST_SKIP() << m_Unavailable;

	const char* pInjected = "x' OR '1'='1";
	// The fake echoes the prepared statement, so the test can check that the
	// value never appears in SQL text sent to MariaDB.
	auto Result = RunSql("SELECT * FROM tw_accounts WHERE Username = ?", Db::Bind(pInjected));
	ASSERT_TRUE(Result.Ok()) << Result.Error().ToString();
	ASSERT_TRUE(Result.HasRows());

	const std::string Received = Result.GetString("query");
	EXPECT_EQ(Received.find(pInjected), std::string::npos) << "the value was spliced into the statement: " << Received;
	EXPECT_NE(Received.find("Username = ?"), std::string::npos) << "the placeholder was not preserved: " << Received;
}

TEST_F(CDbAsyncTest, BindingCountMismatchIsRejected)
{
	if(!StartManager())
		GTEST_SKIP() << m_Unavailable;

	auto Missing = RunSql("SELECT ?");
	EXPECT_FALSE(Missing.Ok());
	EXPECT_NE(Missing.Error().m_SqlState, "");

	auto Extra = RunSql("SELECT 1", Db::Bind(7));
	EXPECT_FALSE(Extra.Ok());
	EXPECT_NE(Extra.Error().m_SqlState, "");
}

TEST_F(CDbAsyncTest, ErrorsAreValuesNotExceptions)
{
	if(!StartManager())
		GTEST_SKIP() << m_Unavailable;

	auto Failure = RunSql("SELECT FAIL");
	EXPECT_FALSE(Failure.Ok());
	EXPECT_EQ(Failure.Error().m_Code, 1146);
	EXPECT_EQ(Failure.Error().m_SqlState, "42S02");
	EXPECT_FALSE(Failure.Changed());
	// reading a failed result is safe as well
	EXPECT_FALSE(Failure.HasRows());
	EXPECT_TRUE(Failure.GetString("query").empty());

	// the connection was not broken by the statement error: the next query runs
	auto Next = RunSql("SELECT ROWS 1");
	EXPECT_TRUE(Next.Ok()) << Next.Error().ToString();
	EXPECT_EQ(Db::Pool(Db::EKind::Remote)->Stats().m_Created, 1u);
}

TEST_F(CDbAsyncTest, CallbackRunsOnTheGameThreadOnly)
{
	if(!StartManager())
		GTEST_SKIP() << m_Unavailable;

	std::thread::id CallbackThread = std::this_thread::get_id();
	bool Called = false;

	Db::Query(Db::EKind::Remote, "SELECT ROWS 2").Then([&](Db::CResultSet&& Result)
	{
		CallbackThread = std::this_thread::get_id();
		Called = Result.Ok() && Result.RowCount() == 2;
	});

	// the worker finishes, but nothing runs game code until the tick pumps it
	ASSERT_TRUE(Db::Executor(Db::EKind::Remote) != nullptr);
	for(int i = 0; i < 200 && Async::PendingMainThreadTasks() == 0; ++i)
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	EXPECT_FALSE(Called) << "a database callback ran on a worker thread";
	EXPECT_EQ(Async::PendingMainThreadTasks(), 1u);

	Async::RunMainThreadTasks();
	EXPECT_TRUE(Called);
	EXPECT_EQ(CallbackThread, std::this_thread::get_id());
}

TEST_F(CDbAsyncTest, CallbackAttachedAfterTheAnswerStillReceivesIt)
{
	if(!StartManager())
		GTEST_SKIP() << m_Unavailable;

	// A login asks for the account row while the session is still being taken and
	// attaches the callback only after the claim: the answer of such a request
	// exists before anyone listens for it.
	Db::CRequest Request = Db::Query(Db::EKind::Remote, "SELECT ROWS 2");

	// Give the first request time to finish before the marker. Pumping the game
	// thread until the delayed marker arrives then exercises delivery with no
	// callback attached to the first request.
	std::atomic<bool> Marker { false };
	Db::Query(Db::EKind::Remote, "SELECT SLOW 0.2").Then([&](Db::CResultSet&&) { Marker = true; });
	for(int i = 0; i < 500 && !Marker; ++i)
	{
		Async::RunMainThreadTasks();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	ASSERT_TRUE(Marker) << "the marker request never answered";

	int Calls = 0;
	bool Ok = false;
	uint32_t Rows = 0;
	Request.Then([&](Db::CResultSet&& Result)
	{
		Calls++;
		Ok = Result.Ok();
		Rows = Result.RowCount();
	});

	for(int i = 0; i < 200 && Calls == 0; ++i)
	{
		Async::RunMainThreadTasks();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	EXPECT_EQ(Calls, 1) << "the answer was dropped because the callback was attached after it arrived, "
			       "so the code waiting for it waits forever";
	EXPECT_TRUE(Ok);
	EXPECT_EQ(Rows, 2u);

	// and the answer is still delivered exactly once
	for(int i = 0; i < 20; ++i)
	{
		Async::RunMainThreadTasks();
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
	EXPECT_EQ(Calls, 1);
}

TEST_F(CDbAsyncTest, CancelDeliversACancellationError)
{
	if(!StartManager())
		GTEST_SKIP() << m_Unavailable;

	Db::CRequest Request = Db::Query(Db::EKind::Remote, "SELECT SLOW 0.5");
	Request.Cancel();
	auto Result = Request.Result(15000);
	EXPECT_FALSE(Result.Ok());
	EXPECT_NE(Result.Error().ToString().find("cancelled"), std::string::npos) << Result.Error().ToString();

	// A cancelled request still answers its callback exactly once, so the code
	// waiting for the answer is never left behind.
	bool Delivered = false;
	bool DeliveredAsFailure = false;
	Db::CRequest Slow = Db::Query(Db::EKind::Remote, "SELECT SLOW 0.5");
	Slow.Then([&](Db::CResultSet&& Result) {
		Delivered = true;
		DeliveredAsFailure = !Result.Ok();
	});
	Slow.Cancel();
	for(int i = 0; i < 400 && !Delivered; ++i)
	{
		Async::RunMainThreadTasks();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	EXPECT_TRUE(Delivered) << "a cancelled request left its callback waiting forever";
	EXPECT_TRUE(DeliveredAsFailure);
}

TEST_F(CDbAsyncTest, CancelAfterCompletionIsHarmless)
{
	if(!StartManager())
		GTEST_SKIP() << m_Unavailable;

	Db::CRequest Request = Db::Query(Db::EKind::Remote, "SELECT ROWS 1");
	Request.Result(15000);
	// too late to cancel: the answer is already there and must be kept
	Request.Cancel();
	auto Result = Request.Result(0);
	EXPECT_TRUE(Result.Ok());
	EXPECT_EQ(Result.RowCount(), 1u);
	// cancelling twice is not an error either
	Request.Cancel();
}

TEST_F(CDbAsyncTest, CancelAfterCompletionButBeforeDeliveryOverridesSuccess)
{
	if(!StartManager())
		GTEST_SKIP() << m_Unavailable;

	std::atomic<bool> Called { false };
	std::string Error;
	auto Request = Db::Query(Db::EKind::Remote, "SELECT ROWS 1");
	Request.Then([&](Db::CResultSet&& Result)
	{
		if(!Result.Ok())
			Error = Result.Error().ToString();
		Called = true;
	});

	for(int i = 0; i < 400 && Async::PendingMainThreadTasks() == 0; ++i)
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	ASSERT_GT(Async::PendingMainThreadTasks(), 0u);
	Request.Cancel();
	Async::RunMainThreadTasks();

	EXPECT_TRUE(Called);
	EXPECT_NE(Error.find("cancelled"), std::string::npos);
}

TEST_F(CDbAsyncTest, TimeoutIsReportedInsteadOfHanging)
{
	if(!StartManager())
		GTEST_SKIP() << m_Unavailable;

	const auto Started = std::chrono::steady_clock::now();
	auto Result = RunSql("SELECT SLOW 0.6", {}, Db::SQueryOptions().Timeout(150));
	const auto Elapsed = std::chrono::steady_clock::now() - Started;
	EXPECT_FALSE(Result.Ok());
	EXPECT_NE(Result.Error().ToString().find("timed out"), std::string::npos) << Result.Error().ToString();
	EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(Elapsed).count(), 5000);

	// the request object has the same bound: an answer or a timeout, never a hang
	Db::CRequest Request = Db::Query(Db::EKind::Remote, "SELECT SLOW 0.6");
	auto Waiting = Request.Result(50);
	EXPECT_FALSE(Waiting.Ok());
	EXPECT_NE(Waiting.Error().ToString().find("timed out"), std::string::npos) << Waiting.Error().ToString();
}

TEST_F(CDbAsyncTest, DelayWaitsOnTheWorkerNotOnTheCaller)
{
	if(!StartManager())
		GTEST_SKIP() << m_Unavailable;

	auto Started = std::chrono::steady_clock::now();
	Db::CRequest Request = Db::Query(Db::EKind::Remote, "SELECT ROWS 1", {}, Db::SQueryOptions().Delay(300));
	const auto Returned = std::chrono::steady_clock::now() - Started;
	EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(Returned).count(), 150)
		<< "submitting a delayed statement blocked the caller";

	auto Result = Request.Result(15000);
	EXPECT_TRUE(Result.Ok()) << Result.Error().ToString();
	const auto Finished = std::chrono::steady_clock::now() - Started;
	EXPECT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(Finished).count(), 300);
}

TEST_F(CDbAsyncTest, FullQueueFailsFast)
{
	if(!StartManager([](Db::SExecutorConfig& Config) {
		Config.m_Workers = 1;
		Config.m_MaxQueue = 1;
	}))
		GTEST_SKIP() << m_Unavailable;

	auto pExecutor = Db::Executor(Db::EKind::Remote);
	ASSERT_TRUE(pExecutor != nullptr);

	Db::CRequest Running = Db::Query(Db::EKind::Remote, "SELECT SLOW 0.6");
	// wait until the single worker took it, so that the next request is the one
	// that occupies the only queue slot
	for(int i = 0; i < 400 && pExecutor->QueuedCount() != 0; ++i)
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	ASSERT_EQ(pExecutor->QueuedCount(), 0u);

	Db::CRequest Queued = Db::Query(Db::EKind::Remote, "SELECT SLOW 0.6");
	ASSERT_EQ(pExecutor->QueuedCount(), 1u);
	Db::CRequest Refused = Db::Query(Db::EKind::Remote, "SELECT ROWS 1");

	auto RefusedResult = Refused.Result(15000);
	EXPECT_FALSE(RefusedResult.Ok());
	EXPECT_NE(RefusedResult.Error().ToString().find("queue is full"), std::string::npos)
		<< RefusedResult.Error().ToString();

	auto RunningResult = Running.Result(15000);
	EXPECT_TRUE(RunningResult.Ok()) << RunningResult.Error().ToString();
	auto QueuedResult = Queued.Result(15000);
	EXPECT_TRUE(QueuedResult.Ok()) << QueuedResult.Error().ToString();
}

TEST_F(CDbAsyncTest, RecoversFromAServerRestart)
{
	if(!StartManager([](Db::SExecutorConfig& Config) {
		Config.m_AcquireTimeoutMs = 4000;
		Config.m_SelectRetries = 1;
	}))
		GTEST_SKIP() << m_Unavailable;

	ASSERT_TRUE(RunSql("SELECT ROWS 1").Ok());

	// the database goes away; the pooled connection is dead but still pooled
	m_pServer->Stop();
	EXPECT_FALSE(RunSql("SELECT ROWS 1").Ok());

	// and it comes back: the next statement opens a fresh connection
	ASSERT_TRUE(m_pServer->Start());
	auto AfterRestart = RunSql("SELECT ROWS 2");
	EXPECT_TRUE(AfterRestart.Ok()) << AfterRestart.Error().ToString();
	EXPECT_EQ(AfterRestart.RowCount(), 2u);
	EXPECT_GE(Db::Pool(Db::EKind::Remote)->Stats().m_Created, 2u);
}

TEST_F(CDbAsyncTest, ConnectionLostDuringAnActiveStatementIsDiscarded)
{
	if(!StartManager([](Db::SExecutorConfig& Config) {
		Config.m_SelectRetries = 0;
		Config.m_RequestTimeoutMs = 5000;
	}))
		GTEST_SKIP() << m_Unavailable;

	ASSERT_TRUE(RunSql("SELECT ROWS 1").Ok());
	Db::CRequest InFlight = Db::Query(Db::EKind::Remote, "SELECT SLOW 2", {}, Db::SQueryOptions().Retries(0).Timeout(5000));
	// Let the fake server enter its in-query delay, then simulate a process
	// restart which drops the live TCP session rather than returning SQL ERR.
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	m_pServer->Stop();

	const Db::CResultSet Lost = InFlight.Result(7000);
	EXPECT_FALSE(Lost.Ok()) << "a severed in-flight statement must not be reported as successful";
	ASSERT_TRUE(m_pServer->Start());
	const Db::CResultSet Recovered = RunSql("SELECT ROWS 2");
	EXPECT_TRUE(Recovered.Ok()) << Recovered.Error().ToString();
	EXPECT_EQ(Recovered.RowCount(), 2u);
	EXPECT_GE(Db::Pool(Db::EKind::Remote)->Stats().m_Created, 2u);
	EXPECT_EQ(Db::Pool(Db::EKind::Remote)->Stats().m_Busy, 0);
}

TEST_F(CDbAsyncTest, TransactionsCommitAndRollback)
{
	if(!StartManager())
		GTEST_SKIP() << m_Unavailable;

	// committed; the id of the first insert is the id of the transaction
	auto Commit = RunTransaction([](Db::CTransaction& Tx)
	{
		auto Account = Tx.Execute("INSERT INTO tw_accounts (Username) VALUES (?)", Db::Bind("one"));
		if(!Account.Changed())
			return false;
		return Tx.Execute("INSERT INTO tw_accounts_data (ID, Nick) VALUES (?, ?)",
			Db::Bind(Account.InsertId(), "one")).Changed();
	});
	ASSERT_TRUE(Commit.Ok()) << Commit.Error().ToString();
	EXPECT_GT(Commit.InsertId(), 0);

	// refused by the body: rolled back
	auto Rollback = RunTransaction([](Db::CTransaction& Tx)
	{
		Tx.Execute("INSERT INTO tw_accounts (Username) VALUES (?)", Db::Bind("two"));
		return false;
	});
	EXPECT_FALSE(Rollback.Ok());
	EXPECT_NE(Rollback.Error().ToString().find("rolled back"), std::string::npos) << Rollback.Error().ToString();

	// a failed statement inside the transaction stops it: the body cannot
	// continue by accident, it just keeps returning results with the error
	auto Failed = RunTransaction([](Db::CTransaction& Tx)
	{
		auto Broken = Tx.Execute("SELECT FAIL");
		auto Ignored = Tx.Execute("INSERT INTO tw_accounts (Username) VALUES (?)", Db::Bind("three"));
		EXPECT_FALSE(Ignored.Ok());
		EXPECT_EQ(Ignored.Error().m_Code, 1146);
		return Broken.Ok();
	});
	EXPECT_FALSE(Failed.Ok());
	EXPECT_EQ(Failed.Error().m_Code, 1146);
}

TEST_F(CDbAsyncTest, ManyConcurrentRequestsShareTheConnectionLimit)
{
	if(!StartManager([](Db::SExecutorConfig& Config) {
		Config.m_MaxConnections = 3;
		Config.m_Workers = 4;
	}))
		GTEST_SKIP() << m_Unavailable;

	constexpr int REQUESTS = 40;
	std::vector<Db::CRequest> vRequests;
	vRequests.reserve(REQUESTS);
	for(int i = 0; i < REQUESTS; ++i)
		vRequests.push_back(Db::Query(Db::EKind::Remote, "SELECT ROWS 2"));

	// every request answers, and the answer arrives even when the caller kept
	// the request object around
	int Ok = 0;
	for(Db::CRequest& Request : vRequests)
	{
		auto Result = Request.Result(20000);
		if(Result.Ok() && Result.RowCount() == 2)
			++Ok;
	}
	EXPECT_EQ(Ok, REQUESTS);
	EXPECT_LE(Db::Pool(Db::EKind::Remote)->Stats().m_Created, 3u)
		<< "the pool exceeded its connection limit";
}

TEST_F(CDbAsyncTest, CallbacksFireInSubmissionOrderForEachConnection)
{
	if(!StartManager([](Db::SExecutorConfig& Config) {
		Config.m_MaxConnections = 1;
		Config.m_Workers = 1;
	}))
		GTEST_SKIP() << m_Unavailable;

	std::vector<int> vOrder;
	int Next = 0;
	for(int i = 0; i < 10; ++i)
	{
		Db::Query(Db::EKind::Remote, "SELECT ROWS 1").Then([&](Db::CResultSet&& Result) {
			if(Result.Ok())
				vOrder.push_back(Next++);
		});
	}

	// one worker answers them one after another; the callbacks keep that order
	for(int i = 0; i < 2000 && static_cast<int>(vOrder.size()) < 10; ++i)
	{
		Async::RunMainThreadTasks();
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
	ASSERT_EQ(vOrder.size(), 10u);
	for(int i = 0; i < 10; ++i)
		EXPECT_EQ(vOrder[i], i);
}

TEST_F(CDbAsyncTest, StopIsGracefulAndRefusesNewWork)
{
	if(!StartManager())
		GTEST_SKIP() << m_Unavailable;

	Db::CRequest Running = Db::Query(Db::EKind::Remote, "SELECT SLOW 0.3");
	Db::Stop(3000);
	// the queued work was answered, not dropped on the floor
	auto RunningResult = Running.Result(5000);
	EXPECT_TRUE(RunningResult.Ok()) << RunningResult.Error().ToString();

	auto AfterStop = RunSql("SELECT ROWS 1");
	EXPECT_FALSE(AfterStop.Ok());
	EXPECT_NE(AfterStop.Error().ToString().find("not configured"), std::string::npos)
		<< AfterStop.Error().ToString();
}

// ---------------------------------------------------------------------------
// both endpoints
// ---------------------------------------------------------------------------
TEST_F(CDbAsyncTest, SharedAndLocalAreIndependent)
{
	const int LocalPort = CFakeMariaDb::FindFreePort();
	if(!StartServer())
		GTEST_SKIP() << m_Unavailable;
	CFakeMariaDb LocalServer("local", LocalPort);

	Db::SExecutorConfig SharedConfig = Config();
	Db::SExecutorConfig LocalConfig = MakeExecutorConfig(m_pDriver.get(), LocalPort);
	LocalConfig.m_Endpoint.m_Name = "local";
	LocalConfig.m_Endpoint.m_Database = "mrpg_local";

	// one start call covers both; the manager owns both
	ASSERT_TRUE(Db::Start(SharedConfig, LocalConfig));

	auto Shared = Db::Query(Db::EKind::Remote, "SELECT 1");
	auto Local = Db::Query(Db::EKind::Local, "SELECT 1");

	// the local server is not started yet: the local request must fail without
	// touching the shared one
	auto SharedResult = Shared.Result(15000);
	ASSERT_TRUE(SharedResult.Ok()) << SharedResult.Error().ToString();
	EXPECT_EQ(SharedResult.GetString("server"), "remote");

	auto LocalWhileDown = Local.Result(15000);
	EXPECT_FALSE(LocalWhileDown.Ok());
	EXPECT_EQ(LocalWhileDown.ColumnCount(), 0u);

	// give the local endpoint a server and it works
	ASSERT_TRUE(LocalServer.Start());
	auto LocalResult = Db::Query(Db::EKind::Local, "SELECT 1").Result(15000);
	ASSERT_TRUE(LocalResult.Ok()) << LocalResult.Error().ToString();
	EXPECT_EQ(LocalResult.GetString("server"), "local");
}
