#include "db_mariadb.h"

#include <base/format.h>
#include <base/system.h>

#include <mariadb/conncpp.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <exception>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <typeinfo>

namespace Db
{
	namespace
	{
		// Count only actual placeholders, ignoring SQL strings, quoted identifiers
		// and comments. This catches API binding mistakes before a driver executes
		// a statement or interprets an unbound placeholder unexpectedly.
		size_t PlaceholderCount(std::string_view Sql)
		{
			enum class EState { Normal, SingleQuote, DoubleQuote, Backtick, LineComment, BlockComment };
			EState State = EState::Normal;
			size_t Count = 0;
			for(size_t i = 0; i < Sql.size(); ++i)
			{
				const char c = Sql[i];
				const char Next = i + 1 < Sql.size() ? Sql[i + 1] : '\0';
				if(State == EState::Normal)
				{
					if(c == '\'') State = EState::SingleQuote;
					else if(c == '\"') State = EState::DoubleQuote;
					else if(c == '`') State = EState::Backtick;
					else if(c == '#' || (c == '-' && Next == '-' && (i + 2 == Sql.size() || std::isspace(static_cast<unsigned char>(Sql[i + 2])))))
					{
						State = EState::LineComment;
						if(c == '-') ++i;
					}
					else if(c == '/' && Next == '*')
					{
						State = EState::BlockComment;
						++i;
					}
					else if(c == '?')
						++Count;
				}
				else if(State == EState::LineComment)
				{
					if(c == '\n' || c == '\r') State = EState::Normal;
				}
				else if(State == EState::BlockComment)
				{
					if(c == '*' && Next == '/') { State = EState::Normal; ++i; }
				}
				else
				{
					const char End = State == EState::SingleQuote ? '\'' : State == EState::DoubleQuote ? '\"' : '`';
					if(c == '\\' && State != EState::Backtick && i + 1 < Sql.size()) ++i;
					else if(c == End && Next == End) ++i;
					else if(c == End) State = EState::Normal;
				}
			}
			return Count;
		}

		SError FromException(sql::SQLException& Exception)
		{
			SError Error;
			Error.m_Code = Exception.getErrorCode();
			if(const char* pState = Exception.getSQLStateCStr())
				Error.m_SqlState = pState;
			// Connector messages can include SQL text or parameter values. Keep
			// the stable diagnostics while never exposing bound data in logs/API.
			Error.m_Message = "database operation failed";
			return Error;
		}

		// Copies one result set into detached rows. Nothing of the client outlives
		// this call, so the result can travel to another thread.
		void CopyResult(sql::ResultSet* pResult, CResultSet& Out, const char*& pStage, uint32_t& ColumnIndex)
		{
			pStage = "metadata";
			sql::ResultSetMetaData* pMeta = pResult->getMetaData();
			const uint32_t Columns = pMeta ? pMeta->getColumnCount() : 0;

			std::vector<std::string> vColumns;
			vColumns.reserve(Columns);
			for(uint32_t i = 1; i <= Columns; ++i)
			{
				ColumnIndex = i;
				pStage = "metadata_column";
				vColumns.emplace_back(pMeta->getColumnLabel(i).c_str());
			}
			Out.SetColumns(std::move(vColumns));

			pStage = "fetch_row";
			while(pResult->next())
			{
				std::vector<std::optional<std::string>> vRow;
				vRow.reserve(Columns);
				for(uint32_t i = 1; i <= Columns; ++i)
				{
					ColumnIndex = i;
					pStage = "read_null";
					if(pResult->isNull(static_cast<int32_t>(i)))
						vRow.emplace_back(std::nullopt);
					else
					{
						pStage = "read_column";
						const auto Value = pResult->getString(static_cast<int32_t>(i));
						pStage = "copy_column";
						vRow.emplace_back(std::string(Value.c_str(), Value.length()));
					}
				}
				pStage = "append_row";
				Out.AddRow(std::move(vRow));
				pStage = "fetch_row";
			}
		}

		class CMariaDbConnection final : public IDbConnection
		{
		public:
			explicit CMariaDbConnection(std::unique_ptr<sql::Connection> pConnection)
				: m_pConnection(std::move(pConnection))
			{
			}

			~CMariaDbConnection() override { Close(); }

			bool IsUsable() const override
			{
				if(!m_Usable || !m_pConnection)
					return false;
				try
				{
					return !m_pConnection->isClosed();
				}
				catch(const sql::SQLException&)
				{
					return false;
				}
			}

			bool Ping() override
			{
				if(!IsUsable())
				{
					m_Usable = false;
					return false;
				}
				try
				{
					std::unique_ptr<sql::Statement> pStmt(m_pConnection->createStatement());
					std::unique_ptr<sql::ResultSet> pResult(pStmt->executeQuery("SELECT 1"));
					const bool Ok = pResult && pResult->next();
					pStmt.reset();
					if(!Ok)
						m_Usable = false;
					return Ok;
				}
				catch(const sql::SQLException&)
				{
					m_Usable = false;
					dbg_msg("SQL", "Database connection ping failed; driver message suppressed.");
					return false;
				}
			}

			CResultSet Execute(std::string_view Sql, const CBindings& Bindings, bool WantInsertId) override
			{
				if(!IsUsable())
				{
					m_Usable = false;
					return CResultSet::Failure(SError { 2013, "HY000", "connection is closed" });
				}
				const size_t ExpectedBindings = PlaceholderCount(Sql);
				if(ExpectedBindings != Bindings.Values().size())
					return CResultSet::Failure(SError { 0, "HY093", fmt_default("SQL placeholder count ({}) does not match binding count ({})", ExpectedBindings, Bindings.Values().size()) });

				const char *pStage = "prepare";
				uint32_t ColumnIndex = 0;
				try
				{
					pStage = "copy_query";
					const std::string Query(Sql);
					CResultSet Out;
					bool HasResult = false;
					int32_t UpdateCount = -1;
					SError DrainError;

					// Read execute() outcomes identically for plain and prepared statements.
					const auto ReadOutcome = [&](sql::Statement* pStmt, bool Result)
					{
						HasResult = Result;
						if(!Result)
						{
							pStage = "update_count";
							UpdateCount = pStmt->getUpdateCount();
							return true;
						}

						pStage = "get_result";
						std::unique_ptr<sql::ResultSet> pResult(pStmt->getResultSet());
						if(pResult)
							CopyResult(pResult.get(), Out, pStage, ColumnIndex);
						pStage = "drain_results";
						return DrainResults(pStmt, DrainError);
					};

					bool OutcomeReady = false;
					if(Bindings.Empty())
					{
						pStage = "create_statement";
						std::unique_ptr<sql::Statement> pStmt(m_pConnection->createStatement());
						pStage = "execute_statement";
						OutcomeReady = ReadOutcome(pStmt.get(), pStmt->execute(Query.c_str()));
					}
					else
					{
						pStage = "prepare_statement";
						std::unique_ptr<sql::PreparedStatement> pStmt(m_pConnection->prepareStatement(Query.c_str()));
						std::vector<std::unique_ptr<std::istringstream>> vBlobStreams;
						pStage = "bind_parameters";
						BindParameters(pStmt.get(), Bindings, vBlobStreams);
						pStage = "execute_prepared";
						OutcomeReady = ReadOutcome(pStmt.get(), pStmt->execute());
					}
					if(!OutcomeReady)
					{
						m_Usable = false;
						return CResultSet::Failure(std::move(DrainError));
					}

					if(!HasResult)
						Out.SetAffectedRows(UpdateCount > 0 ? static_cast<size_t>(UpdateCount) : 0);
					if(WantInsertId && !HasResult)
					{
						int64_t InsertId = 0;
						SError Error;
						pStage = "last_insert_id";
						if(!ReadLastInsertId(InsertId, Error))
							return CResultSet::Failure(std::move(Error));
						Out.SetInsertId(InsertId);
					}
					return Out;
				}
				catch(sql::SQLException& e)
				{
					const SError Error = FromException(e);
					if(IsConnectionLostError(Error.m_Code, Error.m_SqlState))
						m_Usable = false;
					return CResultSet::Failure(Error);
				}
				catch(const std::exception& e)
				{
					m_Usable = false;
					dbg_msg("SQL", "Non-SQL exception in stage=%s column=%u type=%s; driver message suppressed.",
						pStage, ColumnIndex, typeid(e).name());
					return CResultSet::Failure(SError { -1, "", "database operation failed" });
				}
			}
			void Close() override
			{
				if(!m_pConnection)
					return;
				try
				{
					m_pConnection->close();
				}
				catch(sql::SQLException&)
				{
					dbg_msg("SQL", "Closing a database connection failed; driver message suppressed.");
				}
				catch(const std::exception&)
				{
				}
				m_pConnection.reset();
				m_Usable = false;
			}

		private:
			static void BindParameters(sql::PreparedStatement* pStmt, const CBindings& Bindings,
				std::vector<std::unique_ptr<std::istringstream>>& vBlobStreams)
			{
				int Index = 1;
				for(const SValue& Value : Bindings.Values())
				{
					if(std::holds_alternative<std::nullptr_t>(Value))
						pStmt->setNull(Index, sql::VARCHAR);
					else if(const auto* pInt = std::get_if<int64_t>(&Value))
						pStmt->setInt64(Index, *pInt);
					else if(const auto* pUInt = std::get_if<uint64_t>(&Value))
						pStmt->setUInt64(Index, *pUInt);
					else if(const auto* pDouble = std::get_if<double>(&Value))
						pStmt->setDouble(Index, *pDouble);
					else if(const auto* pString = std::get_if<std::string>(&Value))
						pStmt->setString(Index, sql::SQLString(pString->data(), pString->size()));
					else if(const auto* pBlob = std::get_if<SBlob>(&Value))
					{
						auto pStream = std::make_unique<std::istringstream>(pBlob->m_Data, std::ios::in | std::ios::binary);
						pStmt->setBlob(Index, pStream.get());
						vBlobStreams.emplace_back(std::move(pStream));
					}
					++Index;
				}
			}

			// Multi statements and stored procedures can leave more results in the
			// wire; consume them so the next query starts clean.
			static bool DrainResults(sql::Statement* pStmt, SError& Error)
			{
				try
				{
					while(pStmt->getMoreResults())
					{
						std::unique_ptr<sql::ResultSet> pResult(pStmt->getResultSet());
						(void)pResult;
					}
				}
				catch(sql::SQLException& e)
				{
					Error = FromException(e);
					return false;
				}
				return true;
			}

			bool ReadLastInsertId(int64_t& Id, SError& Error)
			{
				try
				{
					std::unique_ptr<sql::Statement> pStmt(m_pConnection->createStatement());
					std::unique_ptr<sql::ResultSet> pResult(pStmt->executeQuery("SELECT LAST_INSERT_ID()"));
					if(pResult && pResult->next())
					{
						Id = pResult->getInt64(1);
						return true;
					}
					Error = SError { -1, "", "database returned no generated id" };
				}
				catch(sql::SQLException& e)
				{
					Error = FromException(e);
					if(IsConnectionLostError(Error.m_Code, Error.m_SqlState))
						m_Usable = false;
				}
				catch(const std::exception& e)
				{
					Error = SError { -1, "", "database operation failed" };
				}
				return false;
			}

			std::unique_ptr<sql::Connection> m_pConnection;
			bool m_Usable = true;
		};
	}

	void CMariaDbDriver::Configure(int StatementTimeoutMs, int SocketTimeoutMs)
	{
		m_StatementTimeoutMs = StatementTimeoutMs;
		m_SocketTimeoutMs = std::max(1000, SocketTimeoutMs);
	}

	std::unique_ptr<IDbConnection> CMariaDbDriver::Connect(const SEndpoint& Endpoint, int TimeoutMs, SError& Error)
	{
		// MariaDB Connector/C++ 1.1.x has shared state in its URL parser: worker
		// threads that call connect() concurrently corrupt it and crash inside
		// UrlParser::parseHaMode(). Creating connections is therefore serialized;
		// queries still run in parallel once the connection exists. The lock wait
		// is part of the caller's acquisition budget, not an unbounded extra wait.
		static std::timed_mutex s_ConnectMutex;
		const auto Deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(std::max(1, TimeoutMs));
		std::unique_lock<std::timed_mutex> Lock(s_ConnectMutex, std::defer_lock);
		if(!Lock.try_lock_until(Deadline))
		{
			Error = SError { 0, "HYT00", "timed out waiting to initialize a database connection" };
			return nullptr;
		}
		const int RemainingMs = static_cast<int>(std::max<int64_t>(1,
			std::chrono::duration_cast<std::chrono::milliseconds>(Deadline - std::chrono::steady_clock::now()).count()));

		try
		{
			const bool HasPortSeparator = Endpoint.m_Host.find(':') != std::string::npos;
			sql::ConnectOptionsMap Properties;
			Properties["hostName"] = HasPortSeparator ? Endpoint.m_Host : fmt_default("{}:{}", Endpoint.m_Host, Endpoint.m_Port);
			Properties["user"] = Endpoint.m_User;
			Properties["password"] = Endpoint.m_Password;
			Properties["schema"] = Endpoint.m_Database;
			// Report changed rows consistently for compare-and-swap updates.
			Properties["useAffectedRows"] = "true";
			// Reconnect only through the pool: transparent client reconnect can
			// silently lose transaction state and make later statements autocommit.
			Properties["autoReconnect"] = "false";
			// Milliseconds, not seconds (Connector/C++ maps these to Connector/C).
			Properties["connectTimeout"] = fmt_default("{}", RemainingMs);
			Properties["socketTimeout"] = fmt_default("{}", m_SocketTimeoutMs);
			Properties["useTls"] = Endpoint.m_UseTls ? "true" : "false";
			// Require server-side prepared statements whenever bindings are present;
			// bound values must never be interpolated into SQL by the client.
			Properties["useServerPrepStmts"] = "true";

			auto pConnection = std::unique_ptr<sql::Connection>(sql::mariadb::get_driver_instance()->connect(Properties));
			// The connector's global parser is only used during connect(). Session
			// initialization is per-connection and should not serialize both pools.
			Lock.unlock();
			if(!pConnection)
			{
				Error = SError { 0, "", "database connection failed" };
				dbg_msg("SQL", "The driver returned no connection for the %s database; credentials suppressed.", Endpoint.m_Name.c_str());
				return nullptr;
			}

			// Server side statement limit (MariaDB 10.1+): a statement that runs
			// longer is aborted by the server instead of holding the worker and the
			// socket until the request that asked for it is long gone. It is set per
			// session, so it costs one round trip per connection, not per statement.
			if(m_StatementTimeoutMs > 0)
			{
				try
				{
					std::unique_ptr<sql::Statement> pStmt(pConnection->createStatement());
					pStmt->execute(fmt_default("SET SESSION max_statement_time = {:.3f}", m_StatementTimeoutMs / 1000.0).c_str());
				}
				catch(const std::exception& e)
				{
					// Not fatal: a MySQL server or an old MariaDB has no such variable,
					// the client side deadline still applies.
					dbg_msg("SQL", "The statement timeout was not applied to the %s connection: %s", Endpoint.m_Name.c_str(), e.what());
				}
			}

			return std::make_unique<CMariaDbConnection>(std::move(pConnection));
		}
		catch(sql::SQLException& e)
		{
			Error = FromException(e);
			Error.m_Message = "database connection failed";
			dbg_msg("SQL", "Failed to open %s database connection (code=%d sqlstate=%s); credentials and driver message suppressed",
				Endpoint.m_Name.c_str(), Error.m_Code, Error.m_SqlState.empty() ? "-" : Error.m_SqlState.c_str());
			return nullptr;
		}
		catch(const std::exception&)
		{
			Error = SError { 0, "", "database connection failed" };
			dbg_msg("SQL", "Failed to open %s database connection (non-SQL driver error); credentials and driver message suppressed",
				Endpoint.m_Name.c_str());
			return nullptr;
		}
	}
}
