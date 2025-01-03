#ifndef ENGINE_SERVER_SQLITE3_SQLITE_STATEMENT_H
#define ENGINE_SERVER_SQLITE3_SQLITE_STATEMENT_H

#include <sqlite3.h>

namespace sqlitedb
{
	class Statement
	{
		mutable size_t m_currentRow;
		int getColumnByName(const char* columnName) const
		{
			int columnCount = sqlite3_column_count(m_pStmt);

			for(int i = 0; i < columnCount; ++i)
			{
				const char* colName = sqlite3_column_name(m_pStmt, i);
				if(str_comp(colName, columnName) == 0)
					return i;
			}

			dbg_msg("sqlite", "column '%s' can't founded.", columnName);
			return -1;
		}

	public:
		explicit Statement(sqlite3_stmt* pStmt) : m_pStmt(pStmt) { }
		~Statement()
		{
			sqlite3_finalize(m_pStmt);
		}

		Statement(const Statement&) = delete;
		Statement& operator=(const Statement&) = delete;

		bool getBoolean(const char* columnName) const
		{
			return (bool)sqlite3_column_int(m_pStmt, getColumnByName(columnName));
		}

		int getInt(const char* columnName) const
		{
			return sqlite3_column_int(m_pStmt, getColumnByName(columnName));
		}

		int64_t getInt64(const char* columnName) const
		{
			return sqlite3_column_int64(m_pStmt, getColumnByName(columnName));
		}

		float getFloat(const char* columnName) const
		{
			return (float)sqlite3_column_double(m_pStmt, getColumnByName(columnName));
		}

		double getDouble(const char* columnName) const
		{
			return sqlite3_column_double(m_pStmt, getColumnByName(columnName));
		}

		std::string getString(const char* columnName) const
		{
			const auto res = sqlite3_column_text(m_pStmt, getColumnByName(columnName));
			if(!res) return "";
			return std::string(reinterpret_cast<const char*>(res));
		}

		bool next()
		{
			if(!m_pStmt)
				return false;

			// Execute the statement and handle potential errors
			int result = sqlite3_step(m_pStmt);
			switch(result)
			{
				case SQLITE_ROW:
					++m_currentRow;
					return true;

				case SQLITE_DONE:
					// No more rows to process
					return false;

				default:
					// Log error details
					dbg_msg("sqlite", "Error while stepping through the query: %s", sqlite3_errstr(result));
					return false;
			}
		}

		size_t rowsCount() const
		{
			int rows = sqlite3_data_count(m_pStmt);
			return static_cast<size_t>(rows);
		}

		size_t getRow() const
		{
			return m_currentRow;
		}

		BigInt getBigInt(const char* columnName) const
		{
			const std::string stringValue = getString(columnName);
			return BigInt(stringValue);
		}

	private:
		sqlite3_stmt* m_pStmt;
	};
	using StatementPtr = std::unique_ptr<Statement>;
}

#endif