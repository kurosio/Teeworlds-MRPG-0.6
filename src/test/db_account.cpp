// Tests of the statements of the registration and authentication pipeline
// (src/game/server/core/components/accounts/account_db.cpp).
//
// A scripted driver stands in for MariaDB: it answers by statement pattern and
// records what reached it. That makes the tests fast and deterministic and -
// more importantly - lets them assert what the game sends: bound parameters
// instead of spliced values, compare-and-swap conditions on the session row and
// transactions around account creation.

#include <engine/server/async.h>
#include <engine/server/db/db_async.h>

#include <game/server/core/components/accounts/account_db.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace
{
	struct SCall
	{
		std::string m_Sql;
		Db::CBindings m_Bindings;
	};

	// One canned answer, matched on a substring of the statement.
	struct SRule
	{
		std::string m_SqlContains;
		std::vector<std::string> m_vColumns;
		std::vector<std::vector<std::optional<std::string>>> m_vRows;
		int m_ErrorCode = 0;
		std::string m_ErrorState;
		std::string m_ErrorMessage;
		size_t m_AffectedRows = 1;
		int64_t m_InsertId = 0;
		bool m_BreaksConnection = false;
		int m_DelayMs = 0;
	};

	class CScriptedConnection;

	class CScriptedDriver : public Db::IDbDriver
	{
	public:
		CScriptedDriver& Rule(SRule Rule_)
		{
			std::lock_guard<std::mutex> Lock(m_Mutex);
			m_vRules.push_back(std::move(Rule_));
			return *this;
		}

		void ClearRules()
		{
			std::lock_guard<std::mutex> Lock(m_Mutex);
			m_vRules.clear();
		}

		std::vector<SCall> Calls() const
		{
			std::lock_guard<std::mutex> Lock(m_Mutex);
			return m_vCalls;
		}

		size_t CallCount() const
		{
			std::lock_guard<std::mutex> Lock(m_Mutex);
			return m_vCalls.size();
		}

		// True when the value never appears inside the text of a statement, only
		// ever as a bound parameter.
		bool ValueWasNeverSpliced(const std::string& Value) const
		{
			std::lock_guard<std::mutex> Lock(m_Mutex);
			for(const SCall& Call : m_vCalls)
			{
				if(Call.m_Sql.find(Value) != std::string::npos)
					return false;
			}
			return true;
		}

		bool ValueWasBound(const std::string& Value) const
		{
			std::lock_guard<std::mutex> Lock(m_Mutex);
			for(const SCall& Call : m_vCalls)
			{
				for(const Db::SValue& Bound : Call.m_Bindings.Values())
				{
					if(const std::string* pText = std::get_if<std::string>(&Bound))
					{
						if(*pText == Value)
							return true;
					}
					else if(const Db::SBlob* pBlob = std::get_if<Db::SBlob>(&Bound))
					{
						if(pBlob->m_Data == Value)
							return true;
					}
				}
			}
			return false;
		}

		std::unique_ptr<Db::IDbConnection> Connect(const Db::SEndpoint& Endpoint, int TimeoutMs, Db::SError& Error) override;

	private:
		friend class CScriptedConnection;

		std::optional<SRule> FindRule(const std::string& Sql) const
		{
			std::lock_guard<std::mutex> Lock(m_Mutex);
			for(const SRule& Rule_ : m_vRules)
			{
				if(Sql.find(Rule_.m_SqlContains) != std::string::npos)
					return Rule_;
			}
			return std::nullopt;
		}

		void Record(std::string Sql, const Db::CBindings& Bindings)
		{
			std::lock_guard<std::mutex> Lock(m_Mutex);
			m_vCalls.push_back({ std::move(Sql), Bindings });
		}

		mutable std::mutex m_Mutex;
		std::vector<SRule> m_vRules;
		std::vector<SCall> m_vCalls;
	};

	class CScriptedConnection : public Db::IDbConnection
	{
	public:
		explicit CScriptedConnection(CScriptedDriver& Driver) : m_Driver(Driver) {}

		bool Ping() override { return !m_Broken; }
		bool IsUsable() const override { return !m_Broken; }
		void Close() override { m_Broken = true; }

		Db::CResultSet Execute(std::string_view Sql, const Db::CBindings& Bindings, bool WantInsertId) override
		{
			m_Driver.Record(std::string(Sql), Bindings);

			const std::optional<SRule> Rule_ = m_Driver.FindRule(std::string(Sql));
			if(Rule_ && Rule_->m_DelayMs > 0)
				std::this_thread::sleep_for(std::chrono::milliseconds(Rule_->m_DelayMs));

			if(!Rule_)
			{
				// isolation setup, START TRANSACTION, COMMIT, ROLLBACK and anything unscripted
				return Db::CResultSet();
			}

			if(Rule_->m_ErrorCode != 0 || !Rule_->m_ErrorMessage.empty())
			{
				m_Broken = Rule_->m_BreaksConnection;
				return Db::CResultSet::Failure(Db::SError { Rule_->m_ErrorCode, Rule_->m_ErrorState, Rule_->m_ErrorMessage });
			}

			Db::CResultSet Out;
			Out.SetColumns(Rule_->m_vColumns);
			for(const auto& Row : Rule_->m_vRows)
				Out.AddRow(Row);
			Out.SetAffectedRows(Rule_->m_AffectedRows);
			if(WantInsertId || Rule_->m_InsertId)
				Out.SetInsertId(Rule_->m_InsertId);
			return Out;
		}

	private:
		CScriptedDriver& m_Driver;
		bool m_Broken = false;
	};

	std::unique_ptr<Db::IDbConnection> CScriptedDriver::Connect(const Db::SEndpoint&, int, Db::SError&)
	{
		return std::make_unique<CScriptedConnection>(*this);
	}

	// Runs the callbacks that the workers queued for the game thread until the
	// condition holds. Returns the condition.
	bool Pump(const std::function<bool()>& Condition, int TimeoutMs = 5000)
	{
		const auto Deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(TimeoutMs);
		while(!Condition() && std::chrono::steady_clock::now() < Deadline)
		{
			Async::RunMainThreadTasks();
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		Async::RunMainThreadTasks();
		return Condition();
	}

	// A request is used in these tests the way the game uses it (a callback) and
	// the way startup code does (Result). Both must work.
	Db::CResultSet RunRequest(Db::CRequest&& Request, int TimeoutMs = 5000)
	{
		return Request.Result(TimeoutMs);
	}

	class CAccountDbTest : public ::testing::Test
	{
	protected:
		void SetUp() override
		{
			Db::SExecutorConfig Config;
			Config.m_pDriver = &m_Driver;
			Config.m_Endpoint.m_Name = "remote";
			Config.m_Endpoint.m_Database = "mrpg_shared";
			Config.m_MaxConnections = 2;
			Config.m_AcquireTimeoutMs = 1000;
			Config.m_IdlePingMs = 0;
			Config.m_Workers = 2;
			Config.m_MaxQueue = 100;
			Config.m_SelectRetries = 0;
			Config.m_RetryBaseDelayMs = 10;
			Config.m_RequestTimeoutMs = 5000;

			ASSERT_TRUE(Db::Start(Config, Config));
		}

		void TearDown() override
		{
			Db::Stop(2000);
			for(int i = 0; i < 100 && Async::PendingMainThreadTasks() > 0; ++i)
				Async::RunMainThreadTasks();
		}

		SRule &NicknameRow()
		{
			SRule Rule_;
			Rule_.m_SqlContains = "FROM tw_accounts_data d";
			Rule_.m_vColumns = { "ID", "Username", "TimeoutCode" };
			Rule_.m_vRows = { { std::string("42"), std::string("alice"), std::string("tok") } };
			m_vRules.push_back(Rule_);
			return m_vRules.back();
		}

		CScriptedDriver m_Driver;
		std::vector<SRule> m_vRules; // storage for rules returned by the helpers
	};
}

// ---------------------------------------------------------------------------
// registration
// ---------------------------------------------------------------------------
TEST_F(CAccountDbTest, RegistrationCreatesAccountAndDataInOneTransaction)
{
	m_Driver.Rule({ "INSERT INTO tw_accounts (", {}, {}, 0, "", "", 1, 7 });
	m_Driver.Rule({ "INSERT INTO tw_accounts_data", {}, {}, 0, "", "", 1, 0 });

	AccountDb::SRegistration Data;
	Data.m_Login = "alice";
	Data.m_PasswordHash = "hash";
	Data.m_PasswordSalt = "salt";
	Data.m_Nickname = "Alice";
	Data.m_RegisteredIP = "10.0.0.1";
	Data.m_Language = "en";

	auto Result = RunRequest(AccountDb::Register(Data));
	ASSERT_TRUE(Result.Ok()) << Result.Error().ToString();
	EXPECT_EQ(Result.InsertId(), 7) << "the transaction must report the created account id";

	const std::vector<SCall> vCalls = m_Driver.Calls();
	ASSERT_GE(vCalls.size(), 5u);
	EXPECT_EQ(vCalls[0].m_Sql, "SET TRANSACTION ISOLATION LEVEL REPEATABLE READ");
	EXPECT_EQ(vCalls[1].m_Sql, "START TRANSACTION");
	EXPECT_NE(vCalls[2].m_Sql.find("INSERT INTO tw_accounts ("), std::string::npos);
	EXPECT_NE(vCalls[3].m_Sql.find("INSERT INTO tw_accounts_data"), std::string::npos);
	EXPECT_EQ(vCalls[4].m_Sql, "COMMIT");

	// the data row is linked to the id the first insert generated
	ASSERT_EQ(vCalls[3].m_Bindings.Values().size(), 2u);
	ASSERT_TRUE(std::holds_alternative<int64_t>(vCalls[3].m_Bindings.Values()[0]));
	EXPECT_EQ(std::get<int64_t>(vCalls[3].m_Bindings.Values()[0]), 7);
	EXPECT_EQ(std::get<std::string>(vCalls[3].m_Bindings.Values()[1]), "Alice");

	// nothing sensitive is spliced into a statement
	EXPECT_TRUE(m_Driver.ValueWasNeverSpliced("hash"));
	EXPECT_TRUE(m_Driver.ValueWasNeverSpliced("10.0.0.1"));
	EXPECT_TRUE(m_Driver.ValueWasBound("hash"));
	EXPECT_TRUE(m_Driver.ValueWasBound("10.0.0.1"));
}

TEST_F(CAccountDbTest, MissingProfessionsAreCreatedInOneBoundStatement)
{
	const std::vector<AccountDb::SProfessionData> vProfessions = {
		{ 1, R"({"level":1})" },
		{ 2, R"({"level":1})" },
		{ 3, R"({"level":1})" },
	};
	const auto Result = RunRequest(AccountDb::CreateProfessions(42, vProfessions));
	ASSERT_TRUE(Result.Ok());

	const auto vCalls = m_Driver.Calls();
	ASSERT_EQ(vCalls.size(), 1u);
	EXPECT_NE(vCalls[0].m_Sql.find("VALUES (?, ?, ?), (?, ?, ?), (?, ?, ?)"), std::string::npos);
	ASSERT_EQ(vCalls[0].m_Bindings.Values().size(), 9u);
	EXPECT_EQ(std::get<int64_t>(vCalls[0].m_Bindings.Values()[0]), 42);
	EXPECT_EQ(std::get<int64_t>(vCalls[0].m_Bindings.Values()[1]), 1);
	EXPECT_EQ(std::get<std::string>(vCalls[0].m_Bindings.Values()[2]), R"({"level":1})");
	EXPECT_EQ(std::get<int64_t>(vCalls[0].m_Bindings.Values()[3]), 42);
	EXPECT_EQ(std::get<int64_t>(vCalls[0].m_Bindings.Values()[4]), 2);
}

TEST_F(CAccountDbTest, CredentialDeleteOwnsAsynchronousStringViews)
{
	m_Driver.Rule({ "SET TRANSACTION ISOLATION LEVEL REPEATABLE READ", {}, {}, 0, "", "", 1, 0, false, 100 });
	Db::CRequest Request;
	{
		std::string Login = "temporary_login";
		std::string Hash = "temporary_hash";
		std::string Salt = "temporary_salt";
		Request = AccountDb::DeleteAccountByCredentials(Login, Hash, Salt);
		Login.assign(200, 'L');
		Hash.assign(200, 'H');
		Salt.assign(200, 'S');
	}
	EXPECT_FALSE(Request.Result(5000).Ok()); // no matching account; transaction rolls back

	for(const SCall& Call : m_Driver.Calls())
	{
		if(Call.m_Sql.find("SELECT ID FROM tw_accounts WHERE Username") == std::string::npos)
			continue;
		ASSERT_EQ(Call.m_Bindings.Values().size(), 3u);
		EXPECT_EQ(std::get<std::string>(Call.m_Bindings.Values()[0]), "temporary_login");
		EXPECT_EQ(std::get<std::string>(Call.m_Bindings.Values()[1]), "temporary_hash");
		EXPECT_EQ(std::get<std::string>(Call.m_Bindings.Values()[2]), "temporary_salt");
		return;
	}
	FAIL() << "credential lookup was not executed";
}

TEST_F(CAccountDbTest, FailedRegistrationIsRolledBack)
{
	m_Driver.Rule({ "INSERT INTO tw_accounts (", {}, {}, 0, "", "", 1, 9 });
	// the nickname was taken by a concurrent registration
	m_Driver.Rule({ "INSERT INTO tw_accounts_data", {}, {}, 1062, "23000", "Duplicate entry" });

	AccountDb::SRegistration Data;
	Data.m_Login = "bob";
	Data.m_Nickname = "Bob";

	auto Result = RunRequest(AccountDb::Register(Data));
	EXPECT_FALSE(Result.Ok());
	EXPECT_EQ(Result.Error().m_Code, 1062);

	const std::vector<SCall> vCalls = m_Driver.Calls();
	bool RolledBack = false;
	for(const SCall& Call : vCalls)
		RolledBack = RolledBack || Call.m_Sql == "ROLLBACK";
	EXPECT_TRUE(RolledBack) << "the half created account was committed";
	for(const SCall& Call : vCalls)
		EXPECT_NE(Call.m_Sql, "COMMIT");
}

TEST_F(CAccountDbTest, AmbiguousCommitIsNotReplayedEvenWhenRetriesAreRequested)
{
	m_Driver.Rule({ "INSERT INTO tw_accounts (", {}, {}, 0, "", "", 1, 9 });
	m_Driver.Rule({ "COMMIT", {}, {}, 2013, "HY000", "connection lost after commit", 0, 0, true });

	const auto Result = Db::Transaction(Db::EKind::Remote, [](Db::CTransaction& Tx)
	{
		return Tx.Execute("INSERT INTO tw_accounts (Username) VALUES (?)", Db::Bind("once")).Changed();
	}, Db::SQueryOptions().Retries(3)).Result(5000);
	EXPECT_FALSE(Result.Ok());

	size_t Inserts = 0;
	size_t Commits = 0;
	for(const SCall& Call : m_Driver.Calls())
	{
		Inserts += Call.m_Sql.find("INSERT INTO tw_accounts (") != std::string::npos;
		Commits += Call.m_Sql == "COMMIT";
	}
	EXPECT_EQ(Inserts, 1u) << "an unknown commit outcome must never replay the write";
	EXPECT_EQ(Commits, 1u);
	EXPECT_GE(Db::Pool(Db::EKind::Remote)->Stats().m_Discarded, 1u);
}

TEST_F(CAccountDbTest, FailedRollbackDiscardsTheUncertainConnection)
{
	m_Driver.Rule({ "INSERT INTO tw_accounts (", {}, {}, 0, "", "", 1, 9 });
	m_Driver.Rule({ "INSERT INTO tw_accounts_data", {}, {}, 1062, "23000", "Duplicate entry" });
	m_Driver.Rule({ "ROLLBACK", {}, {}, 2013, "HY000", "connection lost during rollback", 0, 0, true });

	AccountDb::SRegistration Data;
	Data.m_Login = "rollback_failure";
	Data.m_Nickname = "RollbackFailure";
	const auto Result = RunRequest(AccountDb::Register(Data));
	EXPECT_FALSE(Result.Ok());
	EXPECT_GE(Db::Pool(Db::EKind::Remote)->Stats().m_Discarded, 1u)
		<< "a session with uncertain transaction state must not return to the pool";
}

TEST_F(CAccountDbTest, DeleteAccountCleansUpBothRows)
{
	auto Result = RunRequest(AccountDb::DeleteAccount(12));
	EXPECT_TRUE(Result.Ok()) << Result.Error().ToString();

	size_t DataDeletes = 0, AccountDeletes = 0;
	for(const SCall& Call : m_Driver.Calls())
	{
		if(Call.m_Sql.find("DELETE FROM tw_accounts_data") != std::string::npos)
		{
			++DataDeletes;
			ASSERT_EQ(Call.m_Bindings.Values().size(), 1u);
			EXPECT_EQ(std::get<int64_t>(Call.m_Bindings.Values()[0]), 12);
		}
		if(Call.m_Sql.find("DELETE FROM tw_accounts ") != std::string::npos)
			++AccountDeletes;
	}
	EXPECT_EQ(DataDeletes, 1u);
	EXPECT_EQ(AccountDeletes, 1u);
}

// ---------------------------------------------------------------------------
// login
// ---------------------------------------------------------------------------
TEST_F(CAccountDbTest, NicknameLookupBindsTheName)
{
	m_Driver.Rule({ "FROM tw_accounts_data d", { "ID", "Username", "TimeoutCode" },
		{ { std::string("42"), std::string("alice"), std::string("tok") } } });

	const std::string Nickname = "a' OR 1=1 --";
	auto Result = RunRequest(AccountDb::LookupNickname(Nickname));
	ASSERT_TRUE(Result.Ok()) << Result.Error().ToString();

	const AccountDb::SNickname Parsed = AccountDb::ParseNickname(Result);
	ASSERT_TRUE(Parsed.m_Found);
	EXPECT_EQ(Parsed.m_AccountID, 42);
	EXPECT_EQ(Parsed.m_Username, "alice");
	EXPECT_EQ(Parsed.m_TimeoutCode, "tok");

	EXPECT_TRUE(m_Driver.ValueWasBound(Nickname));
	EXPECT_TRUE(m_Driver.ValueWasNeverSpliced(Nickname));
	const std::vector<SCall> vCalls = m_Driver.Calls();
	ASSERT_EQ(vCalls.size(), 1u);
	EXPECT_NE(vCalls[0].m_Sql.find("Nick = ?"), std::string::npos);
	// Routing is selected by the endpoint kind, never by a SQL comment.
	EXPECT_TRUE(vCalls[0].m_Sql.starts_with("SELECT")) << vCalls[0].m_Sql;
}

TEST_F(CAccountDbTest, AuthenticationParsesCredentialsAndBan)
{
	m_Driver.Rule({ "LEFT JOIN tw_accounts a", {
		"ID", "AccountNickname", "CredentialsFound", "Username", "TimeoutCode",
		"LoginDate", "Language", "PinCode", "Password", "PasswordSalt",
		"HasBan", "BannedUntil", "Reason" },
		{ {
			std::string("42"), std::string("Alice"), std::string("1"), std::string("alice"),
			std::string("tok"), std::string("2026-01-02 03:04:05"), std::string("en"),
			std::string("1234"), std::string("hash"), std::string("salt"),
			std::string("1"), std::string("2026-12-01 00:00:00"), std::string("cheating")
		} } });

	auto Result = RunRequest(AccountDb::Authenticate("Alice", "alice"));
	ASSERT_TRUE(Result.Ok()) << Result.Error().ToString();

	const AccountDb::SCredentials Credentials = AccountDb::ParseCredentials(Result);
	ASSERT_TRUE(Credentials.m_Found);
	EXPECT_EQ(Credentials.m_AccountID, 42);
	EXPECT_EQ(Credentials.m_Nickname, "Alice");
	EXPECT_TRUE(Credentials.m_CredentialsFound);
	EXPECT_EQ(Credentials.m_Username, "alice");
	EXPECT_EQ(Credentials.m_PinCode, "1234");
	EXPECT_EQ(Credentials.m_Password, "hash");
	EXPECT_EQ(Credentials.m_PasswordSalt, "salt");
	ASSERT_TRUE(Credentials.m_HasBan);
	EXPECT_EQ(Credentials.m_BannedUntil, "2026-12-01 00:00:00");
	EXPECT_EQ(Credentials.m_BanReason, "cheating");
}

TEST_F(CAccountDbTest, BanDurationAndReasonAreBoundValues)
{
	const std::string Reason = "text with ' quotes and SQL-looking content";
	auto Result = RunRequest(AccountDb::CreateBan(42, 86400, Reason));
	ASSERT_TRUE(Result.Ok()) << Result.Error().ToString();

	const std::vector<SCall> vCalls = m_Driver.Calls();
	ASSERT_EQ(vCalls.size(), 1u);
	EXPECT_NE(vCalls[0].m_Sql.find("TIMESTAMPADD(SECOND, ?, CURRENT_TIMESTAMP())"), std::string::npos);
	EXPECT_EQ(vCalls[0].m_Bindings.Values().size(), 3u);
	EXPECT_EQ(std::get<int64_t>(vCalls[0].m_Bindings.Values()[0]), 42);
	EXPECT_EQ(std::get<int64_t>(vCalls[0].m_Bindings.Values()[1]), 86400);
	EXPECT_EQ(std::get<std::string>(vCalls[0].m_Bindings.Values()[2]), Reason);
	EXPECT_TRUE(m_Driver.ValueWasNeverSpliced(Reason));
}

TEST_F(CAccountDbTest, AuthenticationComparesTheLoginAndNotTheNickname)
{
	// The nickname is the name the player plays under; the login is what the
	// credentials have to match. Binding the nickname here makes
	// CredentialsFound false for every account whose username differs from its
	// nickname - that is, for almost all of them.
	m_Driver.Rule({ "LEFT JOIN tw_accounts a", { "ID" }, {} });

	RunRequest(AccountDb::Authenticate("Alice", "alice"));

	const std::vector<SCall> vCalls = m_Driver.Calls();
	ASSERT_EQ(vCalls.size(), 1u);
	EXPECT_NE(vCalls[0].m_Sql.find("a.Username = ?"), std::string::npos) << vCalls[0].m_Sql;
	EXPECT_NE(vCalls[0].m_Sql.find("d.Nick = ?"), std::string::npos) << vCalls[0].m_Sql;

	// Both are bound parameters: the login answers the credentials question, the
	// nickname selects the row.
	EXPECT_TRUE(m_Driver.ValueWasBound("alice"));
	EXPECT_TRUE(m_Driver.ValueWasBound("Alice"));
	EXPECT_TRUE(m_Driver.ValueWasNeverSpliced("alice"));
	EXPECT_TRUE(m_Driver.ValueWasNeverSpliced("Alice"));
	ASSERT_EQ(vCalls[0].m_Bindings.Values().size(), 2u);
	ASSERT_TRUE(std::holds_alternative<std::string>(vCalls[0].m_Bindings.Values()[0]));
	EXPECT_EQ(std::get<std::string>(vCalls[0].m_Bindings.Values()[0]), "alice");
	ASSERT_TRUE(std::holds_alternative<std::string>(vCalls[0].m_Bindings.Values()[1]));
	EXPECT_EQ(std::get<std::string>(vCalls[0].m_Bindings.Values()[1]), "Alice");
}

TEST_F(CAccountDbTest, AuthenticationWithoutARowIsNotAFoundAccount)
{
	m_Driver.Rule({ "LEFT JOIN tw_accounts a",
		{ "ID", "AccountNickname", "CredentialsFound", "Username", "TimeoutCode", "LoginDate", "Language",
			"PinCode", "Password", "PasswordSalt", "HasBan", "BannedUntil", "Reason" }, {} });

	auto Result = RunRequest(AccountDb::Authenticate("nobody", "nobody"));
	ASSERT_TRUE(Result.Ok()) << Result.Error().ToString();
	EXPECT_FALSE(AccountDb::ParseCredentials(Result).m_Found);
}

TEST_F(CAccountDbTest, AFailedQueryIsNotAMissingAccount)
{
	m_Driver.Rule({ "LEFT JOIN tw_accounts a", {}, {}, 1146, "42S02", "Table 'tw_accounts' doesn't exist" });

	auto Result = RunRequest(AccountDb::Authenticate("Alice", "alice"));
	EXPECT_FALSE(Result.Ok());
	EXPECT_FALSE(AccountDb::ParseCredentials(Result).m_Found);
	EXPECT_EQ(Result.Error().m_SqlState, "42S02");
}

TEST_F(CAccountDbTest, LoginStampIsWrittenWithBoundValues)
{
	auto Result = RunRequest(AccountDb::UpdateLoginStamp(42, "10.0.0.1", "NL"));
	EXPECT_TRUE(Result.Ok()) << Result.Error().ToString();

	const std::vector<SCall> vCalls = m_Driver.Calls();
	ASSERT_EQ(vCalls.size(), 1u);
	EXPECT_NE(vCalls[0].m_Sql.find("UPDATE tw_accounts SET LoginDate"), std::string::npos);
	EXPECT_TRUE(m_Driver.ValueWasNeverSpliced("10.0.0.1"));
}

// ---------------------------------------------------------------------------
// sessions
// ---------------------------------------------------------------------------
TEST_F(CAccountDbTest, SessionClaimIsACompareAndSwap)
{
	AccountDb::SSessionClaim Claim;
	Claim.m_AccountID = 42;
	Claim.m_ClientID = 3;
	Claim.m_ServerID = 1;
	Claim.m_Token = "new-token";
	Claim.m_Previous.m_Exists = true;
	Claim.m_Previous.m_ServerID = 2;
	Claim.m_Previous.m_ClientID = 9;
	Claim.m_Previous.m_Token = "old-token";
	Claim.m_Previous.m_Heartbeat = 1000;

	// the other server refreshed the row between our read and our write
	m_Driver.Rule({ "UPDATE tw_account_sessions SET ServerID", {}, {}, 0, "", "", 0 });

	auto Result = RunRequest(AccountDb::ClaimSession(Claim, 45));
	EXPECT_TRUE(Result.Ok()) << Result.Error().ToString();
	EXPECT_EQ(Result.AffectedRows(), 0u) << "a lost race must be visible to the caller";

	const std::vector<SCall> vCalls = m_Driver.Calls();
	ASSERT_EQ(vCalls.size(), 1u);
	EXPECT_NE(vCalls[0].m_Sql.find("AND SessionToken = ?"), std::string::npos);
	EXPECT_NE(vCalls[0].m_Sql.find("Heartbeat = 0"), std::string::npos);
	ASSERT_EQ(vCalls[0].m_Bindings.Values().size(), 6u);
	EXPECT_EQ(std::get<std::string>(vCalls[0].m_Bindings.Values()[2]), "new-token");
	EXPECT_EQ(std::get<std::string>(vCalls[0].m_Bindings.Values()[4]), "old-token");
}

TEST_F(CAccountDbTest, SessionClaimOfAFreeAccountCreatesTheRow)
{
	AccountDb::SSessionClaim Claim;
	Claim.m_AccountID = 42;
	Claim.m_ClientID = 3;
	Claim.m_ServerID = 1;
	Claim.m_Token = "new-token";

	auto Result = RunRequest(AccountDb::ClaimSession(Claim, 45));
	EXPECT_TRUE(Result.Ok()) << Result.Error().ToString();

	const std::vector<SCall> vCalls = m_Driver.Calls();
	ASSERT_EQ(vCalls.size(), 1u);
	EXPECT_NE(vCalls[0].m_Sql.find("INSERT INTO tw_account_sessions"), std::string::npos);
	EXPECT_NE(vCalls[0].m_Sql.find("ON DUPLICATE KEY UPDATE"), std::string::npos)
		<< "a row created by another server meanwhile must not be overwritten";
}

TEST_F(CAccountDbTest, SessionActivationReleaseAndRemoval)
{
	AccountDb::SSessionClaim Claim;
	Claim.m_AccountID = 42;
	Claim.m_Token = "token";
	Claim.m_Previous.m_Exists = true;
	Claim.m_Previous.m_ServerID = 2;
	Claim.m_Previous.m_ClientID = 9;
	Claim.m_Previous.m_Token = "old-token";
	Claim.m_Previous.m_Heartbeat = 1000;

	EXPECT_TRUE(RunRequest(AccountDb::ActivateSession(Claim)).Ok());
	EXPECT_TRUE(RunRequest(AccountDb::ReleaseSession(Claim)).Ok());
	EXPECT_TRUE(RunRequest(AccountDb::RemoveSession({ 42, "token" })).Ok());

	const std::vector<SCall> vCalls = m_Driver.Calls();
	ASSERT_EQ(vCalls.size(), 3u);
	EXPECT_NE(vCalls[0].m_Sql.find("SET Heartbeat = UNIX_TIMESTAMP()"), std::string::npos);
	EXPECT_NE(vCalls[0].m_Sql.find("AND Heartbeat = 0"), std::string::npos);

	// releasing restores the previous owner, not an empty row
	EXPECT_NE(vCalls[1].m_Sql.find("UPDATE tw_account_sessions SET ServerID"), std::string::npos);
	ASSERT_EQ(vCalls[1].m_Bindings.Values().size(), 6u);
	EXPECT_EQ(std::get<std::string>(vCalls[1].m_Bindings.Values()[2]), "old-token");

	EXPECT_NE(vCalls[2].m_Sql.find("DELETE FROM tw_account_sessions"), std::string::npos);
}

TEST_F(CAccountDbTest, SessionReadParsesHeartbeatAge)
{
	m_Driver.Rule({ "FROM tw_account_sessions WHERE AccountID = ?",
		{ "ServerID", "ClientID", "SessionToken", "Heartbeat", "CurrentUnixTime" },
		{ { std::string("2"), std::string("9"), std::string("old-token"), std::string("1000"), std::string("1040") } } });

	auto Result = RunRequest(AccountDb::ReadSession(42));
	ASSERT_TRUE(Result.Ok()) << Result.Error().ToString();
	const std::optional<AccountDb::SSessionRecord> Record = AccountDb::ParseSession(Result, 45);
	ASSERT_TRUE(Record.has_value());
	EXPECT_TRUE(Record->m_Exists);
	EXPECT_EQ(Record->m_Token, "old-token");
	EXPECT_EQ(Record->m_HeartbeatAge, 40);
}

TEST_F(CAccountDbTest, AFailedSessionReadIsNotAFreeAccount)
{
	m_Driver.Rule({ "FROM tw_account_sessions WHERE AccountID = ?", {}, {}, 1146, "42S02", "no table" });
	auto Result = RunRequest(AccountDb::ReadSession(42));
	EXPECT_FALSE(Result.Ok());
	EXPECT_FALSE(AccountDb::ParseSession(Result, 45).has_value())
		<< "a failed read must never be mistaken for 'the account has no session'";
}

TEST_F(CAccountDbTest, HeartbeatsAreBatched)
{
	std::vector<AccountDb::SSessionKey> vKeys;
	for(int i = 1; i <= 3; ++i)
		vKeys.push_back({ i, "token-" + std::to_string(i) });

	EXPECT_TRUE(RunRequest(AccountDb::RefreshSessionHeartbeats(vKeys)).Ok());
	const std::vector<SCall> vCalls = m_Driver.Calls();
	// Transactions first set their isolation level, then issue START TRANSACTION,
	// one batched UPDATE, and COMMIT. The isolation setup is not another heartbeat.
	ASSERT_EQ(vCalls.size(), 4u) << "three sessions must be covered by one UPDATE";
	EXPECT_EQ(vCalls[0].m_Sql, "SET TRANSACTION ISOLATION LEVEL REPEATABLE READ");
	EXPECT_EQ(vCalls[1].m_Sql, "START TRANSACTION");
	EXPECT_EQ(vCalls[2].m_Sql, "UPDATE tw_account_sessions SET Heartbeat = UNIX_TIMESTAMP(), UpdatedAt = NOW() "
		"WHERE Heartbeat <> 0 AND ((AccountID = ? AND SessionToken = ?) OR (AccountID = ? AND SessionToken = ?) "
		"OR (AccountID = ? AND SessionToken = ?))");
	EXPECT_EQ(vCalls[2].m_Bindings.Values().size(), 6u);
	EXPECT_EQ(vCalls[3].m_Sql, "COMMIT");
}

TEST_F(CAccountDbTest, NoSessionsMeansNoQuery)
{
	EXPECT_TRUE(RunRequest(AccountDb::RefreshSessionHeartbeats({})).Ok());
	EXPECT_TRUE(RunRequest(AccountDb::FetchSessionTokens({})).Ok());
	EXPECT_EQ(m_Driver.CallCount(), 0u) << "an idle server must not talk to the database";
}

TEST_F(CAccountDbTest, SessionTokensAreParsed)
{
	m_Driver.Rule({ "SELECT AccountID, SessionToken FROM tw_account_sessions",
		{ "AccountID", "SessionToken" },
		{ { std::string("1"), std::string("a") }, { std::string("2"), std::string("b") } } });

	auto Result = RunRequest(AccountDb::FetchSessionTokens({ 1, 2 }));
	ASSERT_TRUE(Result.Ok()) << Result.Error().ToString();
	const std::vector<AccountDb::SSessionKey> vTokens = AccountDb::ParseSessionTokens(Result);
	ASSERT_EQ(vTokens.size(), 2u);
	EXPECT_EQ(vTokens[0].m_AccountID, 1);
	EXPECT_EQ(vTokens[0].m_Token, "a");
	EXPECT_EQ(vTokens[1].m_Token, "b");
}

// ---------------------------------------------------------------------------
// the whole pipeline under load
// ---------------------------------------------------------------------------
TEST_F(CAccountDbTest, ConcurrentLogins)
{
	m_Driver.Rule({ "LEFT JOIN tw_accounts a",
		{ "ID", "AccountNickname", "CredentialsFound", "Username", "TimeoutCode", "LoginDate", "Language",
			"PinCode", "Password", "PasswordSalt", "HasBan", "BannedUntil", "Reason" },
		{ { std::string("42"), std::string("Alice"), std::string("1"), std::string("alice"), std::string("tok"),
			std::string("2026-01-02 03:04:05"), std::string("en"), std::string(""), std::string("hash"),
			std::string("salt"), std::string("0"), std::nullopt, std::nullopt } } });

	constexpr int LOGINS = 20;
	std::atomic<int> Done { 0 };
	std::atomic<int> Found { 0 };

	for(int i = 0; i < LOGINS; ++i)
	{
		AccountDb::Authenticate("Alice", "alice").Then([&](Db::CResultSet&& Result) {
			const AccountDb::SCredentials Credentials = AccountDb::ParseCredentials(Result);
			if(Credentials.m_Found && Credentials.m_CredentialsFound && !Credentials.m_HasBan)
				++Found;
			++Done;
		});
	}

	ASSERT_TRUE(Pump([&] { return Done.load() == LOGINS; }, 15000)) << "only " << Done.load() << " of " << LOGINS << " logins finished";
	EXPECT_EQ(Found.load(), LOGINS);
	EXPECT_EQ(m_Driver.CallCount(), static_cast<size_t>(LOGINS));
	EXPECT_LE(Db::Pool(Db::EKind::Remote)->Stats().m_Created, 2);
}

TEST_F(CAccountDbTest, CancelledLoginDoesNotTouchTheAccount)
{
	m_Driver.Rule({ "LEFT JOIN tw_accounts a", {}, {}, 0, "", "", 1, 0, false, 200 });

	std::atomic<bool> Called { false };
	Db::CResultSet Result;
	Db::CRequest Request = AccountDb::Authenticate("Alice", "alice");
	Request.Then([&](Db::CResultSet&& R) {
		Result = std::move(R);
		Called = true;
	});
	Request.Cancel();

	ASSERT_TRUE(Pump([&] { return Called.load(); }, 10000));
	EXPECT_FALSE(Result.Ok());
	EXPECT_NE(Result.Error().ToString().find("cancelled"), std::string::npos) << Result.Error().ToString();
}

TEST_F(CAccountDbTest, ConnectionLossIsRetriedWithAFreshConnection)
{
	// The MariaDB client reports a dropped TCP connection as error 2013.
	m_Driver.Rule({ "FROM tw_accounts_data d", {}, {}, 2013, "HY000", "Lost connection to MySQL server" });

	auto Result = RunRequest(AccountDb::LookupNickname("alice"));
	EXPECT_FALSE(Result.Ok());
	EXPECT_EQ(Result.Error().m_Code, 2013);
	// the broken connection was not put back into the pool
	EXPECT_GE(Db::Pool(Db::EKind::Remote)->Stats().m_Discarded, 1u);

	// the next statement opens a new connection and works
	m_Driver.ClearRules();
	m_Driver.Rule({ "FROM tw_accounts_data d", { "ID", "Username", "TimeoutCode" },
		{ { std::string("42"), std::string("alice"), std::string("tok") } } });
	auto Retry = RunRequest(AccountDb::LookupNickname("alice"));
	ASSERT_TRUE(Retry.Ok()) << Retry.Error().ToString();
	EXPECT_TRUE(AccountDb::ParseNickname(Retry).m_Found);
}
