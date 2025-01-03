#ifndef ENGINE_SERVER_SQLITE3_SQLITE_HANDLER_H
#define ENGINE_SERVER_SQLITE3_SQLITE_HANDLER_H

#include "sqlite_statement.h"

/*
 * class
 */
enum class DB;

namespace sqlitedb
{
	class Handler
	{
		sqlite3* m_pDB;

	public:
		Handler(const char* pDatabaseFilePath)
		{
			if(sqlite3_open(pDatabaseFilePath, &m_pDB))
			{
				dbg_msg("sqlite", "can't open database: %s", sqlite3_errmsg(m_pDB));
			}
		}

		~Handler()
		{
			sqlite3_close(m_pDB);
		}

	public:
		template <DB T>
		constexpr std::enable_if_t<T == DB::SELECT, ResultPtr> Execute(const char* pSelect, const char* pTable)
		{
			std::string query = fmt_default("SELECT {} FROM {};", pSelect, pTable);
			sqlite3_stmt* stmt;
			if(sqlite3_prepare_v2(m_pDB, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
			{
				dbg_msg("sqlite", "Failed to prepare query: %s", sqlite3_errmsg(m_pDB));
				return nullptr;
			}
			return std::make_unique<Statement>(stmt);
		}

		template <DB T, typename... Ts>
		constexpr std::enable_if_t<T == DB::SELECT, ResultPtr> Execute(const char* pSelect, const char* pTable, const char* pBuffer, Ts&&... args)
		{
			std::string strQuery = fmt_default(pBuffer, std::forward<Ts>(args)...);
			std::string query = fmt_default("SELECT {} FROM {} {}", pSelect, pTable, strQuery);
			sqlite3_stmt* stmt;
			if(sqlite3_prepare_v2(m_pDB, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
			{
				dbg_msg("sqlite", "Failed to prepare query: %s", sqlite3_errmsg(m_pDB));
				return nullptr;
			}
			return std::make_unique<Statement>(stmt);
		}

		template<DB T, typename... Ts>
		constexpr void Execute(const char* pTable, const char* pBuffer, Ts&&... args)
		{
			std::string strQuery = fmt_default(pBuffer, std::forward<Ts>(args)...);

			if constexpr(T == DB::INSERT)
			{
				std::string query = fmt_default("INSERT INTO {} {};", pTable, strQuery);
				sqlite3_stmt* stmt;
				if(sqlite3_prepare_v2(m_pDB, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
				{
					dbg_msg("sqlite", "Failed to prepare insert query: %s", sqlite3_errmsg(m_pDB));
					return;
				}
				if(sqlite3_step(stmt) != SQLITE_DONE)
				{
					dbg_msg("sqlite", "Failed to execute insert query: %s", sqlite3_errmsg(m_pDB));
				}
				sqlite3_finalize(stmt);
			}
			else if constexpr(T == DB::UPDATE)
			{
				std::string query = fmt_default("UPDATE {} SET {};", pTable, strQuery);
				sqlite3_stmt* stmt;
				if(sqlite3_prepare_v2(m_pDB, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
				{
					dbg_msg("sqlite", "Failed to prepare update query: %s", sqlite3_errmsg(m_pDB));
					return;
				}
				if(sqlite3_step(stmt) != SQLITE_DONE)
				{
					dbg_msg("sqlite", "Failed to execute update query: %s", sqlite3_errmsg(m_pDB));
				}
				sqlite3_finalize(stmt);
			}
			else if constexpr(T == DB::REMOVE)
			{
				std::string query = fmt_default("DELETE FROM {} WHERE {}; ", pTable, strQuery);
				sqlite3_stmt* stmt;
				if(sqlite3_prepare_v2(m_pDB, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
				{
					dbg_msg("sqlite", "Failed to prepare remove query: %s", sqlite3_errmsg(m_pDB));
					return;
				}
				if(sqlite3_step(stmt) != SQLITE_DONE)
				{
					dbg_msg("sqlite", "Failed to execute remove query: %s", sqlite3_errmsg(m_pDB));
				}
				sqlite3_finalize(stmt);
			}
		}
	};
}

#endif