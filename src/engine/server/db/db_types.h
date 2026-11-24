#ifndef ENGINE_SERVER_DB_DB_TYPES_H
#define ENGINE_SERVER_DB_DB_TYPES_H

// Small value types shared by the database layer: the two databases the server
// talks to, an error, the endpoint settings and query parameters. Nothing here
// knows a specific database client; the pool and the asynchronous API work with
// the interfaces of db_driver.h.

#include <cstdint>
#include <string>
#include <type_traits>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace Db
{
	// Which of the two independently configured database endpoints executes a statement.
	enum class EKind : int
	{
		Remote = 0,
		Local = 1,
	};

	inline constexpr int NUM_KINDS = 2;

	constexpr bool IsValidKind(EKind Kind)
	{
		return Kind == EKind::Remote || Kind == EKind::Local;
	}

	constexpr int KindIndex(EKind Kind)
	{
		return static_cast<int>(Kind);
	}

	constexpr const char* KindName(EKind Kind)
	{
		switch(Kind)
		{
		case EKind::Remote: return "remote";
		case EKind::Local: return "local";
		default: return "invalid";
		}
	}

	// Tables that live in the local database of one server; everything else is
	// in the remote database. Part of the schema contract: moving a table to the
	// other database is a change here, and every statement follows it.
	inline constexpr std::string_view LOCAL_TABLES[] = {
		"tw_groups",
		"tw_guilds_decorations",
		"tw_guilds_history",
		"tw_guilds_invites",
		"tw_houses_decorations",
		"tw_links",
	};

	constexpr char ToLowerAscii(char c)
	{
		return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
	}

	constexpr bool IsSameName(std::string_view a, std::string_view b)
	{
		if(a.size() != b.size())
			return false;
		for(size_t i = 0; i < a.size(); ++i)
		{
			if(ToLowerAscii(a[i]) != ToLowerAscii(b[i]))
				return false;
		}
		return true;
	}

	constexpr bool IsLocalTable(std::string_view Table)
	{
		for(const std::string_view Name : LOCAL_TABLES)
		{
			if(IsSameName(Table, Name))
				return true;
		}
		return false;
	}

	// The database a statement belongs to. Usable at compile time, so a module can
	// name its endpoint once:
	//     static constexpr Db::EKind KIND = Db::KindOfTable(TW_GUILDS_TABLE);
	//     Db::Query(KIND, "UPDATE tw_guilds SET Bank = ? WHERE ID = ?", ...);
	constexpr EKind KindOfTable(std::string_view Table)
	{
		return IsLocalTable(Table) ? EKind::Local : EKind::Remote;
	}

	// A database error. An empty error means success.
	struct SError
	{
		int m_Code = 0;
		std::string m_SqlState;
		std::string m_Message;

		explicit operator bool() const { return m_Code != 0 || !m_Message.empty(); }
		bool operator==(const SError& Other) const = default;


		std::string ToString() const;
	};

	// Endpoint of one database, exactly what a connection needs to open.
	struct SEndpoint
	{
		std::string m_Name = "remote"; // for logs
		std::string m_Host = "127.0.0.1";
		int m_Port = 3306;
		std::string m_User;
		std::string m_Password;
		std::string m_Database;
		bool m_UseTls = false;

		std::string Address() const;
	};

	// Binary parameters are distinct from text so embedded NULs and arbitrary
	// bytes survive binding and result copying without SQL-side hex encoding.
	struct SBlob
	{
		std::string m_Data;
	};

	inline SBlob Blob(std::string_view Data) { return { std::string(Data) }; }

	// One value bound to a '?' placeholder. Strings and blobs are owned: a
	// statement runs later, on a worker thread, and must not point at caller memory.
	using SValue = std::variant<std::nullptr_t, int64_t, uint64_t, double, std::string, SBlob>;

	// Parameters of one statement, in the order of its '?' placeholders.
	//     Db::Query(kind, "SELECT ... WHERE ID = ? AND Nick = ?", Db::Bind(42, Nick))
	// Values are owned: the statement runs later, on a worker thread.
	class CBindings
	{
	public:
		CBindings& Add(std::nullptr_t)
		{
			m_vValues.emplace_back(nullptr);
			return *this;
		}

		// Integers, bools and floats; the variant stores exactly three numeric
		// types, so no overload can be ambiguous.
		template<typename T> requires std::is_integral_v<T>
		CBindings& Add(T Value)
		{
			if constexpr(std::is_signed_v<T>)
				m_vValues.emplace_back(static_cast<int64_t>(Value));
			else
				m_vValues.emplace_back(static_cast<uint64_t>(Value));
			return *this;
		}

		template<typename T> requires std::is_floating_point_v<T>
		CBindings& Add(T Value)
		{
			m_vValues.emplace_back(static_cast<double>(Value));
			return *this;
		}

		// A C string may be null; a std::string_view may not, so it gets its own
		// overload instead of a silent nullptr conversion.
		CBindings& Add(const char* pValue)
		{
			m_vValues.emplace_back(pValue ? std::string(pValue) : std::string());
			return *this;
		}

		CBindings& Add(std::string_view Value)
		{
			m_vValues.emplace_back(std::string(Value));
			return *this;
		}

		CBindings& Add(std::string&& Value)
		{
			m_vValues.emplace_back(std::move(Value));
			return *this;
		}
		CBindings& Add(const std::string& Value) { return Add(std::string_view(Value)); }
		CBindings& Add(SBlob Value)
		{
			m_vValues.emplace_back(std::move(Value));
			return *this;
		}

		bool Empty() const { return m_vValues.empty(); }
		const std::vector<SValue>& Values() const { return m_vValues; }

	private:
		std::vector<SValue> m_vValues;
	};

	template<typename... Ts>
	CBindings Bind(Ts&&... Values)
	{
		CBindings Result;
		(Result.Add(std::forward<Ts>(Values)), ...);
		return Result;
	}

	// How one statement is executed. Everything has a safe default.
	struct SQueryOptions
	{
		// 0: the request timeout of the pool (bounded by the socket timeout).
		int m_TimeoutMs = 0;
		// < 0: configured defaults; >= 0: requested retry count, still bounded
		// by executor policy and the request deadline. Writes require RetrySafe().
		int m_Retries = -1;
		// Explicit promise that replaying this single statement is safe (for
		// example an idempotent operation with no uncertain external side effect).
		// Transactions are never retried, even with this flag.
		bool m_RetrySafe = false;
		// Wait this long on the worker before running the statement. Used to let
		// a write of another subsystem land first; the game thread never waits.
		int m_DelayMs = 0;
		// Answer this read from an identical read that is already in flight. Only
		// for data no one has to see a moment-old write of - a menu that every
		// player opens at once; never for a read that must see our own writes.
		bool m_Coalesce = false;

		SQueryOptions& Timeout(int Milliseconds)
		{
			m_TimeoutMs = Milliseconds;
			return *this;
		}
		SQueryOptions& Retries(int Count)
		{
			m_Retries = Count;
			return *this;
		}
		SQueryOptions& RetrySafe(bool Enabled = true)
		{
			m_RetrySafe = Enabled;
			return *this;
		}
		SQueryOptions& Delay(int Milliseconds)
		{
			m_DelayMs = Milliseconds;
			return *this;
		}
		SQueryOptions& Coalesce(bool Enabled = true)
		{
			m_Coalesce = Enabled;
			return *this;
		}
	};

	// MariaDB/MySQL error classification. Retry policy is applied separately by
	// the executor; these helpers only identify the server's error categories.
	bool IsConnectionLostError(int Code, std::string_view SqlState);
	bool IsAuthenticationError(int Code, std::string_view SqlState);
	bool IsDeadlockError(int Code, std::string_view SqlState);
	bool IsLockWaitTimeoutError(int Code, std::string_view SqlState);
	bool IsServerUnavailableError(int Code, std::string_view SqlState);

	// Thread-safe mirror of sv_sql_debug for worker-side diagnostics.
	void SetDebugLoggingEnabled(bool Enabled);
	bool DebugLoggingEnabled();

}

#endif // ENGINE_SERVER_DB_DB_TYPES_H
