#include "db_async.h"

#include <engine/server/async.h>

#include <base/format.h>
#include <base/system.h>
#include <engine/server/sql_lost_query_logger.h>
#include <engine/shared/config.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace Db
{
	namespace
	{
		long long NowMs()
		{
			using namespace std::chrono;
			return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
		}

		struct STaggedQuery
		{
			size_t m_Active {};
			long long m_StartedMs {};
			std::vector<std::function<void()>> m_vWatchers;
		};
		std::mutex s_TaggedQueryMutex;
		std::unordered_map<std::string, STaggedQuery> s_TaggedQueries;
		std::unordered_set<std::string> s_CompletedQueryTags;

		void RegisterQueryTag(const std::string& Tag)
		{
			if(Tag.empty())
				return;

			std::lock_guard<std::mutex> Lock(s_TaggedQueryMutex);
			auto& Query = s_TaggedQueries[Tag];
			if(Query.m_Active++ == 0)
				Query.m_StartedMs = NowMs();
		}

		void CompleteQueryTag(const std::string& Tag)
		{
			if(Tag.empty())
				return;

			std::vector<std::function<void()>> vWatchers;
			{
				std::lock_guard<std::mutex> Lock(s_TaggedQueryMutex);
				const auto It = s_TaggedQueries.find(Tag);
				if(It == s_TaggedQueries.end() || It->second.m_Active == 0)
					return;

				if(--It->second.m_Active != 0)
					return;

				s_CompletedQueryTags.insert(Tag);
				vWatchers = std::move(It->second.m_vWatchers);
				s_TaggedQueries.erase(It);
			}

			if(!vWatchers.empty())
			{
				Async::PostToMainThread([Watchers = std::move(vWatchers)]() mutable
				{
					for(auto& Watcher : Watchers)
					{
						try { Watcher(); }
						catch(const std::exception& e) { dbg_msg("SQL", "tag completion watcher failed: %s", e.what()); }
						catch(...) { dbg_msg("SQL", "tag completion watcher failed"); }
					}
				});
			}
		}

		SError TimeoutError()
		{
			return SError { 0, "", "request timed out" };
		}

		SError CancelledError()
		{
			return SError { 0, "", "request cancelled" };
		}

		CResultSet MakeFailure(SError Error)
		{
			return CResultSet::Failure(std::move(Error));
		}

		CResultSet MakeTimeout()
		{
			return MakeFailure(TimeoutError());
		}

		// --- manager --------------------------------------------------------
		std::mutex s_ManagerMutex;
		std::shared_ptr<CDbExecutor> s_apExecutors[NUM_KINDS];
		thread_local bool s_InTransactionBody = false;
		bool s_Started = false;
		bool s_Stopping = false;

		std::shared_ptr<CDbExecutor> GetExecutor(EKind Kind)
		{
			if(!IsValidKind(Kind))
				return {};
			std::lock_guard<std::mutex> Lock(s_ManagerMutex);
			return s_apExecutors[KindIndex(Kind)];
		}

		// Remove only leading whitespace for statement classification. Routing is
		// explicit through EKind; SQL comments never select a database endpoint.
		std::string_view StatementBody(std::string_view Sql)
		{
			while(!Sql.empty() && std::isspace(static_cast<unsigned char>(Sql.front())))
				Sql.remove_prefix(1);
			return Sql;
		}

		// A write that generates an id wants to read it back; a read or an UPDATE
		// has none (reading the id is a second round trip). Deciding it here keeps
		// the flag out of the API.
		bool WantsInsertId(std::string_view Sql)
		{
			Sql = StatementBody(Sql);
			return IsSameName(Sql.substr(0, 6), "insert") || IsSameName(Sql.substr(0, 7), "replace");
		}

		// For the failed-query log: the statement's verb, as written.
		const char* StatementType(std::string_view Sql)
		{
			Sql = StatementBody(Sql);
			const std::string_view Verb = Sql.substr(0, Sql.find_first_of(" \t\r\n"));
			if(IsSameName(Verb, "insert")) return "INSERT";
			if(IsSameName(Verb, "replace")) return "REPLACE";
			if(IsSameName(Verb, "update")) return "UPDATE";
			if(IsSameName(Verb, "delete")) return "DELETE";
			if(IsSameName(Verb, "select")) return "SELECT";
			return "STATEMENT";
		}

		bool IsRetrySafeRead(std::string_view Sql);

		// Only conservatively classified read-only SELECT statements retry by
		// default. Writes need an explicit RetrySafe() declaration and transaction
		// bodies are never replayed.
		int DefaultRetries(EKind Kind, std::string_view Sql)
		{
			auto pExecutor = GetExecutor(Kind);
			return pExecutor && IsRetrySafeRead(Sql) ? pExecutor->Config().m_SelectRetries : 0;
		}

		// SQL text in diagnostics may contain inline literals even though bound
		// values are not part of the query. Redact strings, numeric constants and
		// comments before logging; keep table/column names and query shape useful.
		std::string RedactSqlLiterals(std::string_view Sql)
		{
			std::string Safe;
			Safe.reserve(Sql.size());
			for(size_t i = 0; i < Sql.size();)
			{
				const char c = Sql[i];
				const char Next = i + 1 < Sql.size() ? Sql[i + 1] : '\0';
				if(c == '#' || (c == '-' && Next == '-' && (i + 2 == Sql.size() || std::isspace(static_cast<unsigned char>(Sql[i + 2])))))
				{
					Safe.push_back(' ');
					i += c == '#' ? 1 : 2;
					while(i < Sql.size() && Sql[i] != '\n' && Sql[i] != '\r')
						++i;
					continue;
				}
				if(c == '/' && Next == '*')
				{
					Safe.push_back(' ');
					i += 2;
					while(i + 1 < Sql.size() && !(Sql[i] == '*' && Sql[i + 1] == '/'))
						++i;
					i = std::min(Sql.size(), i + 2);
					continue;
				}
				if(c == '\'' || c == '\"')
				{
					const char Quote = c;
					Safe.push_back('?');
					++i;
					while(i < Sql.size())
					{
						if(Sql[i] == '\\' && i + 1 < Sql.size())
							i += 2;
						else if(Sql[i] == Quote && i + 1 < Sql.size() && Sql[i + 1] == Quote)
							i += 2;
						else if(Sql[i++] == Quote)
							break;
					}
					continue;
				}
				if(std::isdigit(static_cast<unsigned char>(c)) && (i == 0 ||
					!(std::isalnum(static_cast<unsigned char>(Sql[i - 1])) || Sql[i - 1] == '_' || Sql[i - 1] == '$')))
				{
					Safe.push_back('?');
					++i;
					while(i < Sql.size() && (std::isalnum(static_cast<unsigned char>(Sql[i])) || Sql[i] == '_' || Sql[i] == '.'))
						++i;
					continue;
				}
				Safe.push_back(c);
				++i;
			}
			return Safe;
		}

		// A short one-line form of a statement, for diagnostics. Never include
		// bound values, inline literals, credentials or SQL comments.
		std::string MakeLabel(std::string_view Sql)
		{
			const std::string SafeSql = RedactSqlLiterals(Sql);
			std::string Label;
			Label.reserve(96);
			bool Space = false;
			for(const char c : SafeSql)
			{
				if(Label.size() >= 96)
				{
					Label.append("...");
					break;
				}
				if(std::isspace(static_cast<unsigned char>(c)))
				{
					Space = !Label.empty();
					continue;
				}
				if(Space)
				{
					Label.push_back(' ');
					Space = false;
				}
				Label.push_back(c);
			}
			return Label;
		}

		bool SqlDebugEnabled()
		{
			return DebugLoggingEnabled();
		}

		// --- coalescing of identical reads --------------------------------
		// A menu asks the same question for every player - the leaderboard, the
		// mailbox, a record list. Two identical SELECTs with identical parameters
		// return identical rows, so while one of them is in flight the others wait
		// for its answer instead of queueing the same work again.
		bool IsCoalescableRead(std::string_view Sql)
		{
			Sql = StatementBody(Sql);
			if(!IsSameName(Sql.substr(0, 6), "select"))
				return false;

			// Locking reads must reach the server independently. Tokenize only
			// unquoted SQL so comments, string contents and whitespace variations
			// cannot hide FOR UPDATE / FOR SHARE / LOCK IN SHARE MODE.
			std::vector<std::string> vTokens;
			for(size_t i = 0; i < Sql.size();)
			{
				const char c = Sql[i];
				const char Next = i + 1 < Sql.size() ? Sql[i + 1] : '\0';
				if(std::isspace(static_cast<unsigned char>(c)))
				{
					++i;
					continue;
				}
				if(c == '#' || (c == '-' && Next == '-' && (i + 2 == Sql.size() || std::isspace(static_cast<unsigned char>(Sql[i + 2])))))
				{
					i += c == '#' ? 1 : 2;
					while(i < Sql.size() && Sql[i] != '\n' && Sql[i] != '\r')
						++i;
					continue;
				}
				if(c == '/' && Next == '*')
				{
					i += 2;
					while(i + 1 < Sql.size() && !(Sql[i] == '*' && Sql[i + 1] == '/'))
						++i;
					i = std::min(Sql.size(), i + 2);
					continue;
				}
				if(c == '\'' || c == '\"' || c == '`')
				{
					const char Quote = c;
					++i;
					while(i < Sql.size())
					{
						if(Sql[i] == '\\' && Quote != '`' && i + 1 < Sql.size())
							i += 2;
						else if(Sql[i] == Quote && i + 1 < Sql.size() && Sql[i + 1] == Quote)
							i += 2;
						else if(Sql[i++] == Quote)
							break;
					}
					continue;
				}
				if(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '$')
				{
					const size_t Begin = i++;
					while(i < Sql.size() && (std::isalnum(static_cast<unsigned char>(Sql[i])) || Sql[i] == '_' || Sql[i] == '$'))
						++i;
					std::string Token(Sql.substr(Begin, i - Begin));
					std::transform(Token.begin(), Token.end(), Token.begin(), [](char Ch) { return ToLowerAscii(Ch); });
					vTokens.emplace_back(std::move(Token));
					continue;
				}
				++i;
			}

			for(size_t i = 0; i < vTokens.size(); ++i)
			{
				if(vTokens[i] == "for" && i + 1 < vTokens.size() &&
					(vTokens[i + 1] == "update" || vTokens[i + 1] == "share"))
					return false;
				if(vTokens[i] == "lock" && i + 3 < vTokens.size() && vTokens[i + 1] == "in" &&
					vTokens[i + 2] == "share" && vTokens[i + 3] == "mode")
					return false;
			}
			return true;
		}

		bool IsRetrySafeRead(std::string_view Sql)
		{
			if(!IsCoalescableRead(Sql))
				return false;

			// SELECT can still have side effects (INTO, locks, session-changing
			// built-ins). Strip comments/literals, then reject known mutators. This
			// is deliberately conservative; stored procedures/UDFs should use an
			// explicit RetrySafe() promise only when their behavior is known.
			const std::string SafeSql = RedactSqlLiterals(Sql);
			if(SafeSql.find('@') != std::string::npos || SafeSql.find(":=") != std::string::npos)
				return false; // session variables and assignment are stateful
			int NextSequenceState = 0;
			for(size_t i = 0; i < SafeSql.size();)
			{
				if(!std::isalpha(static_cast<unsigned char>(SafeSql[i])) && SafeSql[i] != '_')
				{
					++i;
					continue;
				}
				const size_t Begin = i++;
				while(i < SafeSql.size() && (std::isalnum(static_cast<unsigned char>(SafeSql[i])) || SafeSql[i] == '_'))
					++i;
				std::string Token = SafeSql.substr(Begin, i - Begin);
				std::transform(Token.begin(), Token.end(), Token.begin(), [](char Ch) { return ToLowerAscii(Ch); });
				if(Token == "into" || Token == "get_lock" || Token == "release_lock" || Token == "release_all_locks" ||
					Token == "sleep" || Token == "benchmark" || Token == "last_insert_id" || Token == "nextval" || Token == "setval")
					return false;
				if(NextSequenceState == 2 && Token == "for")
					return false; // NEXT VALUE FOR advances a sequence
				NextSequenceState = Token == "next" ? 1 : Token == "value" && NextSequenceState == 1 ? 2 : 0;
			}
			return true;
		}

		class CTransactionConnectionGuard
		{
		public:
			explicit CTransactionConnectionGuard(IDbConnection& Connection) : m_Connection(Connection) {}
			~CTransactionConnectionGuard() { Discard(); }
			CTransactionConnectionGuard(const CTransactionConnectionGuard&) = delete;
			CTransactionConnectionGuard& operator=(const CTransactionConnectionGuard&) = delete;

			void Complete() noexcept { m_Resolved = true; }
			void Discard() noexcept
			{
				if(m_Resolved)
					return;
				m_Resolved = true;
				try
				{
					m_Connection.Close();
				}
				catch(...)
				{
				}
			}

		private:
			IDbConnection& m_Connection;
			bool m_Resolved = false;
		};

		class CTransactionBodyScope
		{
		public:
			CTransactionBodyScope() : m_Previous(s_InTransactionBody) { s_InTransactionBody = true; }
			~CTransactionBodyScope() { s_InTransactionBody = m_Previous; }
			CTransactionBodyScope(const CTransactionBodyScope&) = delete;
			CTransactionBodyScope& operator=(const CTransactionBodyScope&) = delete;

		private:
			bool m_Previous;
		};

		bool HasMultipleStatements(std::string_view Sql)
		{
			const size_t Semicolon = Sql.find(';');
			if(Semicolon == std::string_view::npos)
				return false;
			for(size_t i = Semicolon + 1; i < Sql.size(); ++i)
			{
				if(!std::isspace(static_cast<unsigned char>(Sql[i])))
					return true;
			}
			return false; // one optional terminator is compatible with SQL files
		}

		bool IsForbiddenPoolStatement(std::string_view Sql)
		{
			if(Sql.find("/*!") != std::string_view::npos)
				return true;
			const std::string SafeSql = RedactSqlLiterals(Sql);
			if(HasMultipleStatements(SafeSql) || SafeSql.find('@') != std::string::npos || SafeSql.find(":=") != std::string::npos)
				return true;
			std::vector<std::string> Tokens;
			for(size_t i = 0; i < SafeSql.size();)
			{
				if(!std::isalpha(static_cast<unsigned char>(SafeSql[i])) && SafeSql[i] != '_')
				{
					++i;
					continue;
				}
				const size_t Begin = i++;
				while(i < SafeSql.size() && (std::isalnum(static_cast<unsigned char>(SafeSql[i])) || SafeSql[i] == '_'))
					++i;
				std::string Token = SafeSql.substr(Begin, i - Begin);
				std::transform(Token.begin(), Token.end(), Token.begin(), [](char Ch) { return ToLowerAscii(Ch); });
				Tokens.emplace_back(std::move(Token));
			}
			if(Tokens.empty())
				return false;
			const std::string& Verb = Tokens.front();
			if(Verb == "begin" || Verb == "start" || Verb == "commit" || Verb == "rollback" ||
				Verb == "savepoint" || Verb == "release" || Verb == "use" || Verb == "lock" || Verb == "unlock" || Verb == "set")
				return true;
			for(const std::string& Token : Tokens)
			{
				if(Token == "get_lock" || Token == "release_lock" || Token == "release_all_locks" || Token == "last_insert_id")
					return true;
			}
			return false;
		}

		bool IsAllowedTransactionStatement(std::string_view Sql)
		{
			// MariaDB executable comments can contain a second command, and a
			// semicolon outside literals/comments can escape the transaction API's
			// statement boundary. Reject both before looking at the leading verb.
			if(Sql.find("/*!") != std::string_view::npos)
				return false;
			const std::string SafeSql = RedactSqlLiterals(Sql);
			if(HasMultipleStatements(SafeSql) || SafeSql.find('@') != std::string::npos || SafeSql.find(":=") != std::string::npos)
				return false;

			std::vector<std::string> Tokens;
			for(size_t i = 0; i < SafeSql.size();)
			{
				if(!std::isalpha(static_cast<unsigned char>(SafeSql[i])) && SafeSql[i] != '_')
				{
					++i;
					continue;
				}
				const size_t Begin = i++;
				while(i < SafeSql.size() && (std::isalnum(static_cast<unsigned char>(SafeSql[i])) || SafeSql[i] == '_'))
					++i;
				std::string Token = SafeSql.substr(Begin, i - Begin);
				std::transform(Token.begin(), Token.end(), Token.begin(), [](char Ch) { return ToLowerAscii(Ch); });
				Tokens.emplace_back(std::move(Token));
			}
			if(Tokens.empty() || (Tokens[0] != "select" && Tokens[0] != "insert" && Tokens[0] != "update" &&
				Tokens[0] != "delete" && Tokens[0] != "replace"))
				return false;

			for(size_t i = 0; i < Tokens.size(); ++i)
			{
				if(Tokens[i] == "into" && i + 1 < Tokens.size() &&
					(Tokens[i + 1] == "outfile" || Tokens[i + 1] == "dumpfile"))
					return false; // filesystem side effects are never transaction-safe
				if(Tokens[i] == "get_lock" || Tokens[i] == "release_lock" || Tokens[i] == "release_all_locks" ||
					Tokens[i] == "sleep" || Tokens[i] == "benchmark" || Tokens[i] == "last_insert_id" ||
					Tokens[i] == "nextval" || Tokens[i] == "setval")
					return false;
				if(i + 2 < Tokens.size() && Tokens[i] == "next" && Tokens[i + 1] == "value" && Tokens[i + 2] == "for")
					return false;
			}
			return true;
		}

		// The identity of one read: the endpoint, the exact text and every bound
		// value, in order.
		std::string CoalesceKey(EKind Kind, std::string_view Sql, const CBindings& Bindings)
		{
			std::string Key = fmt_default("{}|", KindIndex(Kind));
			Key.append(Sql);
			for(const SValue& Value : Bindings.Values())
			{
				Key.push_back('\x1f');
				if(std::holds_alternative<std::nullptr_t>(Value))
					Key.push_back('N');
				else if(const auto* pInt = std::get_if<int64_t>(&Value))
					Key += fmt_default("i{}", *pInt);
				else if(const auto* pUInt = std::get_if<uint64_t>(&Value))
					Key += fmt_default("u{}", *pUInt);
				else if(const auto* pDouble = std::get_if<double>(&Value))
					Key += fmt_default("d{:.17g}", *pDouble);
				else if(const auto* pString = std::get_if<std::string>(&Value))
				{
					Key.push_back('s');
					Key += fmt_default("{}", pString->size());
					Key.push_back(':');
					Key.append(*pString);
				}
				else if(const auto* pBlob = std::get_if<SBlob>(&Value))
				{
					Key.push_back('b');
					Key += fmt_default("{}", pBlob->m_Data.size());
					Key.push_back(':');
					Key.append(pBlob->m_Data);
				}
			}
			return Key;
		}

	}

	std::mutex CRequest::s_CoalesceMutex;
	std::unordered_map<std::string, std::shared_ptr<CRequest::SCoalescedGroup>> CRequest::s_Coalesced;

	void CRequest::FinishCoalescedGroup(const std::string& Key, const std::shared_ptr<SCoalescedGroup>& pGroup, const CResultSet& Result)
	{
		std::vector<std::shared_ptr<SState>> vWaiters;
		{
			std::lock_guard<std::mutex> Lock(s_CoalesceMutex);
			const auto It = s_Coalesced.find(Key);
			if(It != s_Coalesced.end() && It->second == pGroup)
				s_Coalesced.erase(It);
			vWaiters = std::move(pGroup->m_vWaiters);
		}

		for(const auto& pWaiter : vWaiters)
		{
			{
				std::lock_guard<std::mutex> Lock(pWaiter->m_Mutex);
				pWaiter->m_Result = Result;
				if(pWaiter->m_Cancelled && pWaiter->m_Result.Ok())
					pWaiter->m_Result = MakeFailure(CancelledError());
				pWaiter->m_Completed = true;
			}
			pWaiter->m_Cv.notify_all();
			Async::PostToMainThread([pWaiter] { CRequest::FromState(pWaiter).Deliver(); });
		}
	}

	// ---------------------------------------------------------------------------
	// CRequest
	// ---------------------------------------------------------------------------
	CRequest CRequest::Submit(EKind Kind, std::function<CResultSet(IDbConnection&)> Job, const SQueryOptions& Options, int Retries, std::string Sql, std::string DedupeKey, bool RetrySafe)
	{
		auto pState = std::make_shared<SState>();
		CRequest Request = FromState(pState);
		if(s_InTransactionBody)
		{
			if(SqlDebugEnabled())
				dbg_msg("SQL Debug", "request rejected inside a transaction body; statement='%s'", MakeLabel(Sql).c_str());
			Request.Complete(MakeFailure(SError { 0, "", "nested database requests are not allowed inside a transaction body" }));
			return Request;
		}
		if(IsForbiddenPoolStatement(Sql))
		{
			if(SqlDebugEnabled())
				dbg_msg("SQL Debug", "request rejected: transaction/session control or multi-statement SQL; statement='%s'", MakeLabel(Sql).c_str());
			Request.Complete(MakeFailure(SError { 0, "", "transaction control, session mutation, and multi-statement SQL are not allowed through Query" }));
			return Request;
		}

		auto pExecutor = GetExecutor(Kind);
		if(!pExecutor)
		{
			if(SqlDebugEnabled())
				dbg_msg("SQL Debug", "request rejected: %s endpoint is not configured; statement='%s'", KindName(Kind), MakeLabel(Sql).c_str());
			Request.Complete(MakeFailure(SError { 0, "", fmt_default("the {} database is not configured", KindName(Kind)) }));
			return Request;
		}

		if(Retries < 0)
			Retries = pExecutor->Config().m_SelectRetries;

		pState->m_Tag = Options.m_Tag;
		if(!pState->m_Tag.empty())
		{
			pState->m_TagActive.store(true);
			RegisterQueryTag(pState->m_Tag);
		}

		// Taken before Sql is moved into the completion: the label names the
		// statement in the slow statement report.
		const std::string Label = MakeLabel(Sql);
		const bool ReadOnly = IsRetrySafeRead(Sql);

		// The same read is already on its way: wait for it instead of queueing it
		// a second time. No statement, no connection, no row copy.
		std::shared_ptr<SCoalescedGroup> pGroup;
		bool JoinedCoalescedRead = false;
		if(!DedupeKey.empty())
		{
			std::lock_guard<std::mutex> Lock(s_CoalesceMutex);
			const auto It = s_Coalesced.find(DedupeKey);
			if(It != s_Coalesced.end())
			{
				It->second->m_vWaiters.push_back(pState);
				pExecutor->CountCoalesced();
				JoinedCoalescedRead = true;
			}
			else
			{
				pGroup = std::make_shared<SCoalescedGroup>();
				s_Coalesced.emplace(DedupeKey, pGroup);
			}
		}
		if(JoinedCoalescedRead)
		{
			if(SqlDebugEnabled())
				dbg_msg("SQL Debug", "[%s] identical read attached to an in-flight request; statement='%s'", KindName(Kind), Label.c_str());
			return Request;
		}

		// The answer is delivered even when the caller does not look at it, so a
		// statement that finally failed is recorded once here: sv_sql_failed_log_file
		// keeps the ones the game cannot retry any more.
		auto Done = [pState, Sql = std::move(Sql), DedupeKey, pGroup](CResultSet&& Result)
		{
			std::string Failed;
			const char* pType = StatementType(Sql);
			CResultSet WaitersResult;
			{
				std::lock_guard<std::mutex> Lock(pState->m_Mutex);
				pState->m_Result = std::move(Result);
				pState->m_Completed = true;
				if(pGroup)
					WaitersResult = pState->m_Result;
				if(pState->m_Cancelled && pState->m_Result.Ok())
					pState->m_Result = MakeFailure(CancelledError());
				if(!pState->m_Result.Ok() && !pState->m_Cancelled)
				{
					const SError& Error = pState->m_Result.Error();
					// Driver error text may echo bound values (for example a duplicate
					// key value). Persist only the safe diagnostic category/code.
					Failed = fmt_default("{} failed, code={} sqlstate={}", pType, Error.m_Code,
						Error.m_SqlState.empty() ? "-" : Error.m_SqlState);
					}
				}
				pState->m_Cv.notify_all();
				Async::PostToMainThread([pState] { CRequest::FromState(pState).Deliver(); });
			if(pGroup)
				FinishCoalescedGroup(DedupeKey, pGroup, WaitersResult);
			if(!Failed.empty())
			{
				try
				{
					CSqlLostQueryLogger::Instance().LogLostQuery(Failed.c_str(), pType, RedactSqlLiterals(Sql));
				}
				catch(const std::exception& e)
				{
					dbg_msg("SQL Error", "Could not record the failed statement: %s", e.what());
				}
				catch(...)
				{
					dbg_msg("SQL Error", "Could not record the failed statement.");
				}
			}
		};

		std::string Rejected;
		try
		{
			if(!pExecutor->Submit(std::move(Job), Options, Retries, std::move(Done), &pState->m_CancelToken, Label, ReadOnly, RetrySafe))
				Rejected = "the database queue is full or stopping";
		}
		catch(const std::exception& e)
		{
			Rejected = fmt_default("could not queue database request: {}", e.what());
		}
		catch(...)
		{
			Rejected = "could not queue database request";
		}
		if(!Rejected.empty())
		{
			// A rejected leader must wake every coalesced waiter with the same
			// error; it must never leave a stale key that captures future reads.
			CResultSet Failure = MakeFailure(SError { 0, "", Rejected });
			if(pGroup)
				FinishCoalescedGroup(DedupeKey, pGroup, Failure);
			Request.Complete(std::move(Failure));
		}
		return Request;
	}

	// ---------------------------------------------------------------------------
	// CRequest
	// ---------------------------------------------------------------------------
	CRequest& CRequest::Then(CCallback Callback)
	{
		if(!m_pState)
			return *this;

		bool DeliverNow = false;
		{
			std::lock_guard<std::mutex> Lock(m_pState->m_Mutex);
			m_pState->m_Callback = std::move(Callback);
			DeliverNow = m_pState->m_Completed && !m_pState->m_Delivered;
		}
		// A completion that already arrived must still run on the game thread; the
		// worker posted it, this is only a safety net for late attachment.
		if(DeliverNow)
			Async::PostToMainThread([pState = m_pState] { CRequest::FromState(pState).Deliver(); });
		return *this;
	}

	void CRequest::Cancel()
	{
		if(!m_pState)
			return;
		std::lock_guard<std::mutex> Lock(m_pState->m_Mutex);
		if(m_pState->m_Delivered)
			return;
		m_pState->m_Cancelled = true;
		m_pState->m_CancelToken.m_CancelRequested.store(true);
		int Queued = 0;
		m_pState->m_CancelToken.m_Phase.compare_exchange_strong(Queued, 2);
		if(m_pState->m_Completed && m_pState->m_Result.Ok())
			m_pState->m_Result = MakeFailure(CancelledError());
	}

	CResultSet CRequest::Result(int TimeoutMs)
	{
		// A default constructed request is "nothing to do": no statement ran, so
		// there is nothing to fail and the answer is empty.
		if(!m_pState)
			return CResultSet();

		CResultSet Result;
		{
			std::unique_lock<std::mutex> Lock(m_pState->m_Mutex);
			if(!m_pState->m_Completed)
			{
				if(TimeoutMs < 0)
					m_pState->m_Cv.wait(Lock, [this] { return m_pState->m_Completed; });
				else if(!m_pState->m_Cv.wait_for(Lock, std::chrono::milliseconds(TimeoutMs), [this] { return m_pState->m_Completed; }))
					return MakeFailure(TimeoutError());
			}
			// Reading the answer counts as consuming it: a callback attached later
			// does not run for the same answer. The answer itself is a copy, so
			// asking twice gives the same result instead of an empty one.
			m_pState->m_Delivered = true;
			Result = m_pState->m_Result;
		}
		FinishTag(m_pState);
		return Result;
	}

	CRequest CRequest::FromState(std::shared_ptr<SState> pState)
	{
		CRequest Request;
		Request.m_pState = std::move(pState);
		return Request;
	}

	void CRequest::Complete(CResultSet&& Result)
	{
		if(!m_pState)
			return;
		{
			std::lock_guard<std::mutex> Lock(m_pState->m_Mutex);
			m_pState->m_Result = std::move(Result);
			m_pState->m_Completed = true;
			if(m_pState->m_Cancelled && m_pState->m_Result.Ok())
				m_pState->m_Result = MakeFailure(CancelledError());
		}
		m_pState->m_Cv.notify_all();
		Async::PostToMainThread([pState = m_pState] { CRequest::FromState(pState).Deliver(); });
	}

	void CRequest::FinishTag(const std::shared_ptr<SState>& pState)
	{
		if(pState && pState->m_TagActive.exchange(false))
			CompleteQueryTag(pState->m_Tag);
	}

	void CRequest::Deliver()
	{
		CCallback Callback;
		CResultSet Result;
		{
			std::lock_guard<std::mutex> Lock(m_pState->m_Mutex);
			// A request may finish before its owner attaches Then() (for example,
			// account-row prefetch during session takeover). Keep the result
			// available in that case; Then() will enqueue another delivery attempt.
			if(m_pState->m_Delivered || !m_pState->m_Completed)
				return;
			if(!m_pState->m_Callback)
			{
				FinishTag(m_pState);
				return;
			}
			m_pState->m_Delivered = true;
			Callback = std::move(m_pState->m_Callback);
			// Callback consumption is exclusive with Result(); move the potentially
			// large row set instead of retaining and copying it in request state.
			Result = std::move(m_pState->m_Result);
		}
		try
		{
			Callback(std::move(Result));
		}
		catch(...)
		{
			FinishTag(m_pState);
			throw;
		}
		FinishTag(m_pState);
	}

	// ---------------------------------------------------------------------------
	// CTransaction
	// ---------------------------------------------------------------------------
	CResultSet CTransaction::Execute(std::string_view Sql, const CBindings& Bindings)
	{
		if(m_Error)
			return MakeFailure(m_Error);
		if(!IsAllowedTransactionStatement(Sql))
		{
			m_Error = SError { 0, "", "statement type is not allowed within a transaction" };
			return MakeFailure(m_Error);
		}

		const std::string Label = MakeLabel(Sql);
		const long long StartedMs = NowMs();
		CResultSet Result = m_Connection.Execute(Sql, Bindings, WantsInsertId(Sql));
		if(!Result.Ok())
		{
			m_Error = Result.Error();
			if(!m_Error)
				m_Error = SError { 0, "", "transaction statement failed" };
			Result.SetError(m_Error);
			if(SqlDebugEnabled())
				dbg_msg("SQL Debug", "[%s] transaction statement failed in %lldms (code=%d sqlstate=%s); statement='%s'",
					KindName(m_Kind), NowMs() - StartedMs, m_Error.m_Code, m_Error.m_SqlState.empty() ? "-" : m_Error.m_SqlState.c_str(), Label.c_str());
			return Result;
		}
		if(SqlDebugEnabled())
			dbg_msg("SQL Debug", "[%s] transaction statement succeeded in %lldms (rows=%zu affected=%zu); statement='%s'",
				KindName(m_Kind), NowMs() - StartedMs, Result.RowCount(), Result.AffectedRows(), Label.c_str());

		// A read inside a transaction has no counts of its own. For a write, the
		// first generated id is the row a create has to keep, and the last count
		// that reports rows wins (MySQL reports 0 when the values did not change).
		if(!Result.HasRows())
		{
			m_AffectedRows = Result.AffectedRows();
			if(Result.InsertId() && !m_InsertId)
				m_InsertId = Result.InsertId();
		}
		return Result;
	}

	// ---------------------------------------------------------------------------
	// CDbExecutor
	// ---------------------------------------------------------------------------
	CDbExecutor::CDbExecutor(std::shared_ptr<CDbPool> pPool, SExecutorConfig Config)
		: m_pPool(std::move(pPool)), m_Config(std::move(Config))
	{
		m_Config.m_MaxConnections = std::max(1, m_Config.m_MaxConnections);
		m_Config.m_Workers = std::clamp(m_Config.m_Workers, 1, m_Config.m_MaxConnections);
		m_Config.m_MaxQueue = std::max(1, m_Config.m_MaxQueue);
		m_Config.m_RequestTimeoutMs = std::max(1, m_Config.m_RequestTimeoutMs);
		m_Config.m_AcquireTimeoutMs = std::max(1, m_Config.m_AcquireTimeoutMs);
		m_Config.m_RetryBaseDelayMs = std::max(0, m_Config.m_RetryBaseDelayMs);
		m_Config.m_RetryMaxDelayMs = std::max(m_Config.m_RetryBaseDelayMs, m_Config.m_RetryMaxDelayMs);
		m_Config.m_SelectRetries = std::clamp(m_Config.m_SelectRetries, 0, 10);
		try
		{
			m_vTaskSlots.resize(static_cast<size_t>(m_Config.m_MaxQueue));
			for(int i = 0; i < m_Config.m_Workers; ++i)
				m_vWorkers.emplace_back([this] { WorkerThread(); });
		}
		catch(...)
		{
			m_Stopping = true;
			m_Cv.notify_all();
			for(std::thread& Worker : m_vWorkers)
				if(Worker.joinable())
					Worker.join();
			throw;
		}
	}

	CDbExecutor::~CDbExecutor()
	{
		Stop();
	}

	size_t CDbExecutor::QueuedCount() const
	{
		std::lock_guard<std::mutex> Lock(m_Mutex);
		return m_Queued;
	}

	CDbExecutor::SStats CDbExecutor::Stats() const
	{
		std::lock_guard<std::mutex> Lock(m_Mutex);
		return m_Stats;
	}

	void CDbExecutor::CountCoalesced()
	{
		std::lock_guard<std::mutex> Lock(m_Mutex);
		m_Stats.m_Coalesced++;
	}

	bool CDbExecutor::Submit(CJob Job, const SQueryOptions& Options, int Retries, CDone Done, CRequest::CCancelToken* pCancelToken, std::string Label, bool ReadOnly, bool RetrySafe)
	{
		const bool Debug = SqlDebugEnabled();
		std::string DebugLabel;
		if(Debug)
			DebugLabel = Label;
		CTask Task;
		Task.m_Job = std::move(Job);
		Task.m_Done = std::move(Done);
		Task.m_pCancelToken = pCancelToken;
		Task.m_Label = std::move(Label);
		Task.m_ReadOnly = ReadOnly;
		Task.m_RetrySafe = RetrySafe;
		Task.m_QueuedMs = NowMs();
		Task.m_DeadlineMs = Task.m_QueuedMs + std::max(1, Options.m_TimeoutMs > 0 ? Options.m_TimeoutMs : m_Config.m_RequestTimeoutMs);
		Task.m_RetriesLeft = std::clamp(Retries, 0, 10);
		Task.m_DelayMs = std::max(0, Options.m_DelayMs);

		size_t Queued = 0;
		bool Accepted = false;
		const int TimeoutMs = std::max(1, Options.m_TimeoutMs > 0 ? Options.m_TimeoutMs : m_Config.m_RequestTimeoutMs);
		{
			std::lock_guard<std::mutex> Lock(m_Mutex);
			if(!m_Stopping.load() && m_Queued < m_vTaskSlots.size())
			{
				const size_t Tail = (m_QueueHead + m_Queued) % m_vTaskSlots.size();
				m_vTaskSlots[Tail].emplace(std::move(Task));
				++m_Queued;
				m_Stats.m_Submitted++;
				if(m_Queued > m_Stats.m_QueueMax)
					m_Stats.m_QueueMax = m_Queued;
				Accepted = true;
			}
			Queued = m_Queued;
		}
		if(!Accepted)
		{
			if(Debug)
				dbg_msg("SQL Debug", "[%s] request refused (stopping or queue full); queue=%zu/%d; statement='%s'",
					m_Config.m_Endpoint.m_Name.c_str(), Queued, m_Config.m_MaxQueue, DebugLabel.c_str());
			return false;
		}
		m_Cv.notify_one();
		if(Debug)
			dbg_msg("SQL Debug", "[%s] queued request; queue=%zu/%d retries=%d timeout=%dms delayed=%dms; statement='%s'",
				m_Config.m_Endpoint.m_Name.c_str(), Queued, m_Config.m_MaxQueue, std::max(0, Retries), TimeoutMs,
				std::max(0, Options.m_DelayMs), DebugLabel.c_str());

		if(m_Config.m_QueueWarnSize > 0 && static_cast<int>(Queued) >= m_Config.m_QueueWarnSize)
		{
			try
			{
				WarnAboutQueue(Queued);
			}
			catch(...)
			{
				// Diagnostics must not turn accepted work into an apparent rejection.
			}
		}
		return true;
	}

	// A queue that stays long is the symptom of a slow statement or of a pool that
	// is too small; the report therefore carries the numbers needed to tell those
	// apart, and is limited to one line per ten seconds instead of one per request.
	void CDbExecutor::WarnAboutQueue(size_t Queued)
	{
		int Suppressed = 0;
		uint64_t Slowest = 0;
		std::string SlowestLabel;
		size_t High = 0;
		{
			std::lock_guard<std::mutex> Lock(m_Mutex);
			m_QueueWarned = true;
			High = m_Stats.m_QueueMax;
			const long long Now = NowMs();
			if(Now - m_LastQueueWarnMs < 10000)
			{
				m_SuppressedQueueWarns++;
				return;
			}
			m_LastQueueWarnMs = Now;
			Suppressed = std::exchange(m_SuppressedQueueWarns, 0);
			Slowest = m_Stats.m_SlowestMs;
			SlowestLabel = m_Stats.m_SlowestLabel;
		}

		const CDbPool::SStats PoolStats = m_pPool->Stats();
		dbg_msg("SQL Worker", "Database queue is long: %zu requests waiting (%d/%d connections busy, high water mark %zu, slowest statement %llums%s%s%s).",
			Queued, PoolStats.m_Busy, PoolStats.m_Open, High, static_cast<unsigned long long>(Slowest),
			SlowestLabel.empty() ? "" : " '", SlowestLabel.empty() ? "" : SlowestLabel.c_str(), SlowestLabel.empty() ? "" : "'");
		if(Suppressed > 0)
			dbg_msg("SQL Worker", "%d queue warnings were suppressed in the last ten seconds.", Suppressed);
	}

	void CDbExecutor::CountExpired()
	{
		std::lock_guard<std::mutex> Lock(m_Mutex);
		m_Stats.m_Expired++;
	}

	// One line per second at most: a slow database produces these in bursts, and
	// a burst of identical lines hides the statement that causes it.
	void CDbExecutor::ReportLongStatement(const std::string& Label, long long ElapsedMs, bool Late)
	{
		int Suppressed = 0;
		{
			std::lock_guard<std::mutex> Lock(m_Mutex);
			const long long Now = NowMs();
			if(Now - m_LastLongStatementWarnMs < 1000)
			{
				m_SuppressedLongStatements++;
				return;
			}
			m_LastLongStatementWarnMs = Now;
			Suppressed = std::exchange(m_SuppressedLongStatements, 0);
		}

		if(Late)
			dbg_msg("SQL", "A statement finished after its request timed out: %lldms, '%s'%s", ElapsedMs, Label.c_str(),
				Suppressed > 0 ? fmt_default(" ({} similar statements were not reported)", Suppressed).c_str() : "");
		else
			dbg_msg("SQL Slow", "A statement took %lldms: '%s'%s", ElapsedMs, Label.c_str(),
				Suppressed > 0 ? fmt_default(" ({} similar statements were not reported)", Suppressed).c_str() : "");
	}

	void CDbExecutor::Stop(int WaitMs)
	{
		// Manager shutdown and the destructor can race on a shared endpoint handle.
		// Serialize the join so no std::thread is ever joined twice concurrently.
		std::lock_guard<std::mutex> StopLock(m_StopMutex);

		// Stop() runs twice - the manager stops the executor and its destructor runs
		// again - the summary belongs to the first one only.
		bool First = false;
		{
			std::lock_guard<std::mutex> Lock(m_Mutex);
			First = !m_Stopping.exchange(true);
		}
		m_Cv.notify_all();

		// The numbers of the whole run: what the log of a busy evening is read with.
		const SStats Summary = Stats();
		if(First && Summary.m_Submitted > 0)
			dbg_msg("SQL Worker", "The %s database ran %llu statements (%llu timed out, %llu dropped before running, %llu safe retries, %llu answered by an identical read, longest wait %llums, slowest '%s' %llums).",
				m_Config.m_Endpoint.m_Name.c_str(), static_cast<unsigned long long>(Summary.m_Submitted),
				static_cast<unsigned long long>(Summary.m_TimedOut), static_cast<unsigned long long>(Summary.m_Expired),
				static_cast<unsigned long long>(Summary.m_Retries), static_cast<unsigned long long>(Summary.m_Coalesced),
				static_cast<unsigned long long>(Summary.m_WaitMaxMs),
				Summary.m_SlowestLabel.c_str(), static_cast<unsigned long long>(Summary.m_SlowestMs));

		for(std::thread& Worker : m_vWorkers)
		{
			if(Worker.joinable())
				Worker.join();
		}
		m_vWorkers.clear();

		if(m_pPool)
			m_pPool->Shutdown(WaitMs);
	}

	CResultSet CDbExecutor::RunTask(CTask& Task)
	{
		const long long QueueWaitMs = std::max<long long>(0, NowMs() - Task.m_QueuedMs);
		if(SqlDebugEnabled())
		{
			const CDbPool::SStats Stats = m_pPool->Stats();
			dbg_msg("SQL Debug", "[%s] worker picked request after %lldms in queue; pool open=%d idle=%d busy=%d; statement='%s'",
				m_Config.m_Endpoint.m_Name.c_str(), QueueWaitMs, Stats.m_Open, Stats.m_Idle, Stats.m_Busy, Task.m_Label.c_str());
		}

		// Delayed writes: the wait happens here, on the worker, never on the game
		// thread and never while a connection is held.
		if(Task.m_DelayMs > 0)
		{
			const long long DelayUntil = NowMs() + std::min<long long>(Task.m_DelayMs, std::max<long long>(0, Task.m_DeadlineMs - NowMs()));
			while(NowMs() < DelayUntil && !Task.m_pCancelToken->m_CancelRequested.load())
				std::this_thread::sleep_for(std::chrono::milliseconds(std::min<long long>(10, DelayUntil - NowMs())));
			Task.m_DelayMs = 0;
		}

		bool ExecutionStarted = false;
		int AcquireRetries = 0;
		while(true)
		{
			if(Task.m_pCancelToken->m_CancelRequested.load())
				return MakeFailure(CancelledError());
			if(NowMs() >= Task.m_DeadlineMs)
			{
				CountExpired();
				if(SqlDebugEnabled())
					dbg_msg("SQL Debug", "[%s] request expired before execution; statement='%s'", m_Config.m_Endpoint.m_Name.c_str(), Task.m_Label.c_str());
				return MakeTimeout();
			}
			if(Task.m_Attempt > 0)
			{
				std::lock_guard<std::mutex> Lock(m_Mutex);
				++m_Stats.m_Retries;
			}

			// A read waits for a connection no longer than sv_sql_queue_wait_timeout_ms:
			// a menu must not keep a worker standing in line for half a minute. A write
			// waits up to its deadline, because losing it is worse than a late answer.
			const long long Remaining = Task.m_DeadlineMs - NowMs();
			const long long Budget = (Task.m_ReadOnly && m_Config.m_AcquireTimeoutMs > 0)
				? std::min<long long>(Remaining, m_Config.m_AcquireTimeoutMs)
				: Remaining;
			auto Lease = m_pPool->Acquire(static_cast<int>(std::max<long long>(1, Budget)));
			if(!Lease)
			{
				if(Task.m_pCancelToken->m_CancelRequested.load())
					return MakeFailure(CancelledError());
				const CDbPool::SStats Stats = m_pPool->Stats();
				if(SqlDebugEnabled())
					dbg_msg("SQL Debug", "[%s] connection acquisition failed; open=%d idle=%d busy=%d stopping=%d; statement='%s'",
					m_Config.m_Endpoint.m_Name.c_str(), Stats.m_Open, Stats.m_Idle, Stats.m_Busy, m_Stopping.load(), Task.m_Label.c_str());
				if(m_Stopping.load() || m_pPool->IsShutdown())
					return MakeFailure(CancelledError());
				const long long Now = NowMs();
				if(Now >= Task.m_DeadlineMs)
				{
					CountExpired();
					dbg_msg("SQL Error", "The %s database pool could not provide a connection before the request deadline (open=%d idle=%d busy=%d); statement='%s'",
						m_Config.m_Endpoint.m_Name.c_str(), Stats.m_Open, Stats.m_Idle, Stats.m_Busy, Task.m_Label.c_str());
					return MakeTimeout();
				}
				if(m_Config.m_RetryBaseDelayMs <= 0)
				{
					dbg_msg("SQL Error", "The %s database pool has no connection available (open=%d idle=%d busy=%d); statement='%s'",
						m_Config.m_Endpoint.m_Name.c_str(), Stats.m_Open, Stats.m_Idle, Stats.m_Busy, Task.m_Label.c_str());
					return MakeFailure(SError { 0, "", fmt_default("no {} database connection available", m_pPool->Config().m_Endpoint.m_Name) });
				}

				// No lease means the SQL job has not run. Retrying acquisition is
				// therefore safe for reads and writes alike; keep it within the
				// original request deadline and back off instead of failing a login
				// during a short pool-capacity spike.
				++AcquireRetries;
				long long DelayMs = m_Config.m_RetryBaseDelayMs;
				for(int i = 1; i < AcquireRetries && DelayMs < m_Config.m_RetryMaxDelayMs; ++i)
					DelayMs = std::min<long long>(m_Config.m_RetryMaxDelayMs, DelayMs * 2);
				DelayMs = std::min<long long>(DelayMs, m_Config.m_RetryMaxDelayMs);
				{
					std::lock_guard<std::mutex> Lock(m_Mutex);
					++m_Stats.m_Retries;
				}
				if(SqlDebugEnabled())
					dbg_msg("SQL Debug", "[%s] waiting %lldms before retrying connection acquisition (retry=%d); pool open=%d idle=%d busy=%d; statement='%s'",
						m_Config.m_Endpoint.m_Name.c_str(), DelayMs, AcquireRetries, Stats.m_Open, Stats.m_Idle, Stats.m_Busy, Task.m_Label.c_str());
				const long long RetryUntil = NowMs() + std::min<long long>(DelayMs, std::max<long long>(0, Task.m_DeadlineMs - NowMs()));
				while(NowMs() < RetryUntil && !Task.m_pCancelToken->m_CancelRequested.load())
					std::this_thread::sleep_for(std::chrono::milliseconds(std::min<long long>(10, RetryUntil - NowMs())));
				continue;
			}

			// Waiting for a connection may have used the whole budget: running the
			// statement now would only produce an answer nobody waits for and hold
			// a connection away from work that is still wanted.
			if(NowMs() >= Task.m_DeadlineMs)
			{
				Lease->Release();
				CountExpired();
				return MakeTimeout();
			}
			if(Task.m_pCancelToken->m_CancelRequested.load())
			{
				Lease->Release();
				return MakeFailure(CancelledError());
			}
			if(!ExecutionStarted)
			{
				int Queued = 0;
				if(!Task.m_pCancelToken->m_Phase.compare_exchange_strong(Queued, 1))
				{
					Lease->Release();
					return MakeFailure(CancelledError());
				}
				ExecutionStarted = true;
			}

			const long long StartedMs = NowMs();
			CResultSet Result;
			try
			{
				Result = Task.m_Job(*Lease->Get());
			}
			catch(...)
			{
				// A throwing driver may have left the protocol state uncertain. Do not
				// return that session to the idle pool while the worker reports failure.
				Lease->Discard();
				throw;
			}
			const long long ElapsedMs = NowMs() - StartedMs;
			if(SqlDebugEnabled())
			{
				const CDbPool::SStats Stats = m_pPool->Stats();
				if(Result.Ok())
					dbg_msg("SQL Debug", "[%s] statement succeeded in %lldms (attempt=%d, queue-wait=%lldms, rows=%zu, affected=%zu, insert-id=%lld, pool open=%d idle=%d busy=%d); statement='%s'",
						m_Config.m_Endpoint.m_Name.c_str(), ElapsedMs, Task.m_Attempt + 1, QueueWaitMs, Result.RowCount(), Result.AffectedRows(),
						static_cast<long long>(Result.InsertId()), Stats.m_Open, Stats.m_Idle, Stats.m_Busy, Task.m_Label.c_str());
				else
					dbg_msg("SQL Debug", "[%s] statement failed in %lldms (attempt=%d, queue-wait=%lldms, code=%d sqlstate=%s, pool open=%d idle=%d busy=%d); statement='%s'",
						m_Config.m_Endpoint.m_Name.c_str(), ElapsedMs, Task.m_Attempt + 1, QueueWaitMs, Result.Error().m_Code,
						Result.Error().m_SqlState.empty() ? "-" : Result.Error().m_SqlState.c_str(), Stats.m_Open, Stats.m_Idle, Stats.m_Busy, Task.m_Label.c_str());
			}
			{
				std::lock_guard<std::mutex> Lock(m_Mutex);
				if(static_cast<uint64_t>(ElapsedMs) > m_Stats.m_SlowestMs)
				{
					m_Stats.m_SlowestMs = static_cast<uint64_t>(std::max<long long>(0, ElapsedMs));
					m_Stats.m_SlowestLabel = Task.m_Label;
				}
			}

			if(ElapsedMs >= m_Config.m_SlowStatementWarnMs && m_Config.m_SlowStatementWarnMs > 0)
				ReportLongStatement(Task.m_Label, ElapsedMs);

			if(Result.Ok())
			{
				if(NowMs() >= Task.m_DeadlineMs)
				{
					// The statement did run, but nobody waits for it any more: the
					// answer is late, the deadline of the request already answered.
					{
						std::lock_guard<std::mutex> Lock(m_Mutex);
						m_Stats.m_TimedOut++;
					}
					ReportLongStatement(Task.m_Label, ElapsedMs, true);
					return MakeTimeout();
				}
				return Result;
			}

			const bool ConnectionLost = IsConnectionLostError(Result.Error().m_Code, Result.Error().m_SqlState);
			const bool Deadlock = IsDeadlockError(Result.Error().m_Code, Result.Error().m_SqlState);
			const bool LockWaitTimedOut = IsLockWaitTimeoutError(Result.Error().m_Code, Result.Error().m_SqlState);
			bool ConnectionUsable = false;
			try
			{
				ConnectionUsable = Lease->Get()->IsUsable();
			}
			catch(...)
			{
				// A health-check exception is a broken session, not a reason to
				// unwind the worker while leaving the lease eligible for reuse.
			}
			if(ConnectionLost || !ConnectionUsable)
			{
				Lease->Discard();
				if(SqlDebugEnabled())
					dbg_msg("SQL Debug", "[%s] discarded failed connection (lost-error=%d usable=%d); statement='%s'",
						m_Config.m_Endpoint.m_Name.c_str(), ConnectionLost, ConnectionUsable, Task.m_Label.c_str());
			}
			else
				Lease->Release();

			const bool Retryable = ConnectionLost || !ConnectionUsable || Deadlock || LockWaitTimedOut;
			if(!Task.m_RetrySafe || !Retryable || Task.m_RetriesLeft <= 0)
				return Result;

			Task.m_RetriesLeft--;
			Task.m_Attempt++;
			long long DelayMs = m_Config.m_RetryBaseDelayMs;
			for(int i = 1; i < Task.m_Attempt && DelayMs < m_Config.m_RetryMaxDelayMs; ++i)
				DelayMs = std::min<long long>(m_Config.m_RetryMaxDelayMs, DelayMs * 2);
			DelayMs = std::min<long long>(DelayMs, m_Config.m_RetryMaxDelayMs);
			dbg_msg("SQL Worker", "Retrying a retry-safe statement in %lldms (%d attempts left; code=%d sqlstate=%s): %s",
				DelayMs, Task.m_RetriesLeft, Result.Error().m_Code,
				Result.Error().m_SqlState.empty() ? "-" : Result.Error().m_SqlState.c_str(), Task.m_Label.c_str());
			const long long RetryUntil = NowMs() + std::min<long long>(DelayMs, std::max<long long>(0, Task.m_DeadlineMs - NowMs()));
			while(NowMs() < RetryUntil && !Task.m_pCancelToken->m_CancelRequested.load())
				std::this_thread::sleep_for(std::chrono::milliseconds(std::min<long long>(10, RetryUntil - NowMs())));
		}
	}

	void CDbExecutor::WorkerThread()
	{
		while(true)
		{
			CTask Task;
			bool Drained = false;
			size_t DrainedQueued = 0;
			size_t DrainedHigh = 0;
			uint64_t DrainedWait = 0;
			{
				std::unique_lock<std::mutex> Lock(m_Mutex);
				m_Cv.wait(Lock, [this] { return m_Stopping.load() || m_Queued > 0; });
				if(m_Stopping.load() && m_Queued == 0)
					return;
				Task = std::move(*m_vTaskSlots[m_QueueHead]);
				m_vTaskSlots[m_QueueHead].reset();
				m_QueueHead = (m_QueueHead + 1) % m_vTaskSlots.size();
				--m_Queued;
				const uint64_t WaitMs = static_cast<uint64_t>(std::max<long long>(0, NowMs() - Task.m_QueuedMs));
				if(WaitMs > m_Stats.m_WaitMaxMs)
					m_Stats.m_WaitMaxMs = WaitMs;
				if(m_QueueWarned && static_cast<int>(m_Queued) < m_Config.m_QueueWarnSize)
				{
					m_QueueWarned = false;
					Drained = true;
					DrainedQueued = m_Queued;
					DrainedHigh = m_Stats.m_QueueMax;
					DrainedWait = m_Stats.m_WaitMaxMs;
				}
			}

			if(Drained)
				dbg_msg("SQL Worker", "The database queue is back to normal: %zu requests waiting (high water mark %zu, longest wait %llums).",
					DrainedQueued, DrainedHigh, static_cast<unsigned long long>(DrainedWait));

			CResultSet Result;
			try
			{
				Result = RunTask(Task);
			}
			catch(const std::exception&)
			{
				// Arbitrary exception text can contain query parameters or row data.
				dbg_msg("SQL Error", "A database job failed with an unexpected exception; details suppressed.");
				Result = MakeFailure(SError { 0, "", "database job failed" });
			}
			catch(...)
			{
				Result = MakeFailure(SError { 0, "", "unknown database error" });
			}
			Task.m_pCancelToken->m_Phase.store(3);
			if(Task.m_Done)
			{
				try
				{
					Task.m_Done(std::move(Result));
				}
				catch(const std::exception&)
				{
					dbg_msg("SQL Error", "Delivering a database result failed with an unexpected exception; details suppressed.");
				}
				catch(...)
				{
					dbg_msg("SQL Error", "Delivering a database result failed.");
				}
			}
		}
	}

	// ---------------------------------------------------------------------------
	// manager
	// ---------------------------------------------------------------------------
	namespace
	{
		// The pool config is derived from the endpoint config: the application
		// fills one struct, the pool gets what it needs.
		SPoolConfig MakePoolConfig(const SExecutorConfig& Config)
		{
			SPoolConfig PoolConfig;
			PoolConfig.m_Endpoint = Config.m_Endpoint;
			PoolConfig.m_MaxConnections = Config.m_MaxConnections;
			PoolConfig.m_AcquireTimeoutMs = Config.m_AcquireTimeoutMs;
			PoolConfig.m_ConnectTimeoutMs = Config.m_ConnectTimeoutMs;
			PoolConfig.m_ConnectRetryDelayMs = Config.m_ConnectRetryDelayMs;
			PoolConfig.m_ConnectRetryMaxDelayMs = Config.m_ConnectRetryMaxDelayMs;
			PoolConfig.m_IdlePingMs = Config.m_IdlePingMs;
			return PoolConfig;
		}

		std::shared_ptr<CDbExecutor> MakeExecutor(const SExecutorConfig& Config)
		{
			if(!Config.m_pDriver)
			{
				dbg_msg("SQL Error", "The %s database has no client driver.", KindName(Config.m_Endpoint.m_Name == "local" ? EKind::Local : EKind::Remote));
				return nullptr;
			}
			auto pPool = std::make_shared<CDbPool>(*Config.m_pDriver, MakePoolConfig(Config));
			return std::make_shared<CDbExecutor>(std::move(pPool), Config);
		}
	}

	bool Start(const SExecutorConfig& Remote, const SExecutorConfig& Local)
	{
		std::lock_guard<std::mutex> Lock(s_ManagerMutex);
		if(s_Started || s_Stopping)
			return false;
		if(!Remote.m_pDriver || !Local.m_pDriver)
		{
			dbg_msg("SQL Error", "Both remote and local database drivers are required.");
			return false;
		}

		std::shared_ptr<CDbExecutor> pRemoteExecutor;
		std::shared_ptr<CDbExecutor> pLocalExecutor;
		try
		{
			pRemoteExecutor = MakeExecutor(Remote);
			pLocalExecutor = MakeExecutor(Local);
		}
		catch(const std::exception& e)
		{
			dbg_msg("SQL Error", "Failed to create the remote/local database executors: %s", e.what());
			return false;
		}
		catch(...)
		{
			dbg_msg("SQL Error", "Failed to create the remote/local database executors.");
			return false;
		}
		if(!pRemoteExecutor || !pLocalExecutor)
			return false;

		s_apExecutors[KindIndex(EKind::Remote)] = std::move(pRemoteExecutor);
		s_apExecutors[KindIndex(EKind::Local)] = std::move(pLocalExecutor);
		s_Started = true;
		Async::StartMainThreadTasks();
		return true;
	}

	void Stop(int WaitMs)
	{
		std::shared_ptr<CDbExecutor> apExecutors[NUM_KINDS];
		{
			std::lock_guard<std::mutex> Lock(s_ManagerMutex);
			if(!s_Started || s_Stopping)
				return;
			s_Stopping = true;
			for(int i = 0; i < NUM_KINDS; ++i)
			{
				apExecutors[i] = std::move(s_apExecutors[i]);
			}
			s_Started = false;
		}
		// Both are stopped before any of them is destroyed: a completion that runs
		// during the stop may still queue work on the other endpoint.
		for(auto& pExecutor : apExecutors)
		{
			if(pExecutor)
				pExecutor->Stop(WaitMs);
		}
		{
			std::lock_guard<std::mutex> Lock(s_ManagerMutex);
			s_Stopping = false;
		}
	}

	bool IsStarted()
	{
		std::lock_guard<std::mutex> Lock(s_ManagerMutex);
		return s_Started;
	}

	std::shared_ptr<CDbExecutor> Executor(EKind Kind)
	{
		return GetExecutor(Kind);
	}

	std::shared_ptr<CDbPool> Pool(EKind Kind)
	{
		auto pExecutor = Executor(Kind);
		return pExecutor ? pExecutor->Pool() : std::shared_ptr<CDbPool>();
	}

	bool IsTagRunning(std::string_view Tag, int MaxAgeSeconds)
	{
		if(Tag.empty())
			return false;

		std::lock_guard<std::mutex> Lock(s_TaggedQueryMutex);
		const auto It = s_TaggedQueries.find(std::string(Tag));
		if(It == s_TaggedQueries.end() || It->second.m_Active == 0)
			return false;

		return MaxAgeSeconds <= 0 || NowMs() - It->second.m_StartedMs < static_cast<long long>(MaxAgeSeconds) * 1000;
	}

	bool HasTagCompleted(std::string_view Tag)
	{
		if(Tag.empty())
			return false;

		std::lock_guard<std::mutex> Lock(s_TaggedQueryMutex);
		return s_CompletedQueryTags.contains(std::string(Tag));
	}

	bool WatchTag(std::string_view Tag, std::function<void()> Callback)
	{
		if(Tag.empty() || !Callback)
			return false;

		std::lock_guard<std::mutex> Lock(s_TaggedQueryMutex);
		const auto It = s_TaggedQueries.find(std::string(Tag));
		if(It == s_TaggedQueries.end() || It->second.m_Active == 0)
			return false;

		It->second.m_vWatchers.push_back(std::move(Callback));
		return true;
	}

	// ---------------------------------------------------------------------------
	// statements
	// ---------------------------------------------------------------------------
	CRequest Query(EKind Kind, std::string_view Sql, const CBindings& Bindings, const SQueryOptions& Options)
	{
		SetDebugLoggingEnabled(g_Config.m_SvSqlDebug != 0);
		std::string Text(Sql);
		const bool InsertId = Options.m_FetchInsertId && WantsInsertId(Text);
		auto Job = [Text, Bindings, InsertId](IDbConnection& Connection) -> CResultSet
		{
			return Connection.Execute(Text, Bindings, InsertId);
		};

		// Identical reads share one statement when the caller says the answer may be
		// shared; anything else is submitted as is.
		std::string DedupeKey;
		if(Options.m_Coalesce && Options.m_DelayMs <= 0 && IsRetrySafeRead(Text))
			DedupeKey = CoalesceKey(Kind, Text, Bindings);

		const bool RetrySafeRead = IsRetrySafeRead(Text);
		const bool RetrySafe = RetrySafeRead || Options.m_RetrySafe;
		const int Retries = !RetrySafe ? 0 : Options.m_Retries >= 0 ? Options.m_Retries :
			(RetrySafeRead ? DefaultRetries(Kind, Text) : 0);
		return CRequest::Submit(Kind, std::move(Job), Options, Retries, std::move(Text), std::move(DedupeKey), RetrySafe);
	}

	CRequest Transaction(EKind Kind, std::function<bool(CTransaction&)> Body, const SQueryOptions& Options)
	{
		SetDebugLoggingEnabled(g_Config.m_SvSqlDebug != 0);
		// The statements of the transaction, plus its delay: another subsystem
		// may have to land its write first.
		auto Job = [Body = std::move(Body), Kind](IDbConnection& Connection) -> CResultSet
		{
			CTransactionConnectionGuard ConnectionGuard(Connection);
			CResultSet Result;
			// Pin a known isolation level for this transaction only. SET TRANSACTION
			// (without SESSION) applies to the next transaction, so it cannot leak
			// into the pooled session after COMMIT/ROLLBACK.
			Result = Connection.Execute("SET TRANSACTION ISOLATION LEVEL REPEATABLE READ", {}, false);
			if(!Result.Ok())
			{
				ConnectionGuard.Discard();
				return Result;
			}
			if(SqlDebugEnabled())
				dbg_msg("SQL Debug", "[%s] transaction BEGIN (isolation=REPEATABLE READ)", KindName(Kind));
			Result = Connection.Execute("START TRANSACTION", {}, false);
			if(!Result.Ok())
			{
				if(SqlDebugEnabled())
					dbg_msg("SQL Debug", "[%s] transaction BEGIN failed (code=%d sqlstate=%s); connection will be discarded",
						KindName(Kind), Result.Error().m_Code, Result.Error().m_SqlState.empty() ? "-" : Result.Error().m_SqlState.c_str());
				// The transaction state is uncertain after a failed BEGIN. Never
				// return this session to the pool in case the server did begin it.
				ConnectionGuard.Discard();
				return Result;
			}
			if(SqlDebugEnabled())
				dbg_msg("SQL Debug", "[%s] transaction BEGIN succeeded", KindName(Kind));

			CTransaction Tx(Connection, Kind);
			bool Commit = false;
			try
			{
				CTransactionBodyScope Scope;
				Commit = Body(Tx) && !Tx.m_Error;
			}
			catch(const std::exception&)
			{
				// Transaction callback exceptions may include application data.
				Tx.m_Error = SError { 0, "", "the transaction body failed" };
			}
			catch(...)
			{
				Tx.m_Error = SError { 0, "", "the transaction body failed" };
			}

			CResultSet Outcome;
			if(!Commit)
			{
				if(SqlDebugEnabled())
					dbg_msg("SQL Debug", "[%s] transaction rolling back%s", KindName(Kind), Tx.m_Error ? " after a statement/body failure" : " because the body declined commit");
				Outcome = Connection.Execute("ROLLBACK", {}, false);
				const bool RollbackSucceeded = Outcome.Ok();
				if(!RollbackSucceeded)
				{
					if(SqlDebugEnabled())
						dbg_msg("SQL Debug", "[%s] transaction ROLLBACK failed (code=%d sqlstate=%s); connection will be discarded",
							KindName(Kind), Outcome.Error().m_Code, Outcome.Error().m_SqlState.empty() ? "-" : Outcome.Error().m_SqlState.c_str());
					// A failed rollback may leave locks and uncommitted state on the
					// session. Closing it is safer than handing it to another request.
					ConnectionGuard.Discard();
				}
				else
				{
					ConnectionGuard.Complete();
					if(SqlDebugEnabled())
						dbg_msg("SQL Debug", "[%s] transaction ROLLBACK succeeded", KindName(Kind));
				}
				if(Tx.m_Error)
					return MakeFailure(Tx.m_Error);
				if(!RollbackSucceeded)
					return Outcome;
				return MakeFailure(SError { 0, "", "transaction rolled back" });
			}

			if(SqlDebugEnabled())
				dbg_msg("SQL Debug", "[%s] transaction COMMIT", KindName(Kind));
			Outcome = Connection.Execute("COMMIT", {}, false);
			if(!Outcome.Ok())
			{
				if(SqlDebugEnabled())
					dbg_msg("SQL Debug", "[%s] transaction COMMIT failed (code=%d sqlstate=%s); outcome unknown, connection will be discarded",
						KindName(Kind), Outcome.Error().m_Code, Outcome.Error().m_SqlState.empty() ? "-" : Outcome.Error().m_SqlState.c_str());
				// The server may have committed before the connection failed. Do not
				// retry this transaction or reuse a session with unknown state.
				ConnectionGuard.Discard();
				return Outcome;
			}
			ConnectionGuard.Complete();

			// The answer of a committed transaction: the ids and rows its
			// statements collected, so a create can keep the generated id.
			Outcome = CResultSet();
			Outcome.SetAffectedRows(Tx.m_AffectedRows);
			Outcome.SetInsertId(Tx.m_InsertId);
			if(SqlDebugEnabled())
				dbg_msg("SQL Debug", "[%s] transaction committed (affected=%zu insert-id=%lld)", KindName(Kind), Tx.m_AffectedRows, static_cast<long long>(Tx.m_InsertId));
			return Outcome;
		};

		// The commit result can be ambiguous after a connection loss. Never replay
		// a transaction body: doing so can duplicate writes even if a caller sets
		// the generic per-query retry option.
		return CRequest::Submit(Kind, std::move(Job), Options, 0, "transaction body");
	}
}

