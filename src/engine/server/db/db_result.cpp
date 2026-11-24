#include "db_result.h"

#include <base/system.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace Db
{
	namespace
	{
		int64_t ToInt64(std::string_view Text)
		{
			return std::strtoll(std::string(Text).c_str(), nullptr, 10);
		}

		uint64_t ToUInt64(std::string_view Text)
		{
			return std::strtoull(std::string(Text).c_str(), nullptr, 10);
		}
	}

	CResultSet CResultSet::Failure(SError Error)
	{
		CResultSet Result;
		Result.m_Error = std::move(Error);
		return Result;
	}

	bool CResultSet::Next()
	{
		// A first row that was already read (the getters read it without
		// advancing) is behind us; Next() continues with the second one.
		if(m_Row == 0 && m_Peeked && !m_vRows.empty())
		{
			m_Peeked = false;
			m_Row = 1;
		}
		if(m_Row >= m_vRows.size())
			return false;
		++m_Row;
		return true;
	}

	std::string CResultSet::GetStringAt(size_t Index) const
	{
		return ValueAt(static_cast<int>(Index)).value_or(std::string());
	}

	int CResultSet::ColumnIndex(std::string_view Column) const
	{
		for(size_t i = 0; i < m_vColumns.size(); ++i)
		{
			if(IsSameName(m_vColumns[i], Column))
				return static_cast<int>(i);
		}
		return -1;
	}

	const std::optional<std::string>& CResultSet::ValueAt(int Index) const
	{
		static const std::optional<std::string> s_Empty;
		// No row is a normal answer (a SELECT that found nothing), not a crash:
		// the getters simply yield empty values. Only an unknown column - a typo
		// in the code - is an error.
		const size_t RowIndex = m_Row == 0 ? 0 : m_Row - 1;
		if(m_Row == 0)
			m_Peeked = true;
		if(RowIndex >= m_vRows.size())
			return s_Empty;
		const auto& Row = m_vRows[RowIndex];
		if(Index < 0 || static_cast<size_t>(Index) >= Row.size())
		{
			dbg_assert(false, "SQL result read of an unknown column");
			return s_Empty;
		}
		return Row[Index];
	}

	std::string CResultSet::GetString(std::string_view Column) const
	{
		const auto& Value = ValueAt(ColumnIndex(Column));
		return Value.value_or(std::string());
	}

	bool CResultSet::GetBoolean(std::string_view Column) const
	{
		const auto Text = GetString(Column);
		// MariaDB returns booleans as 1/0; be forgiving about the text form.
		return ToInt64(Text) != 0;
	}

	int CResultSet::GetInt(std::string_view Column) const
	{
		return static_cast<int>(ToInt64(GetString(Column)));
	}

	unsigned int CResultSet::GetUInt(std::string_view Column) const
	{
		return static_cast<unsigned int>(ToUInt64(GetString(Column)));
	}

	int64_t CResultSet::GetInt64(std::string_view Column) const
	{
		return ToInt64(GetString(Column));
	}

	uint64_t CResultSet::GetUInt64(std::string_view Column) const
	{
		return ToUInt64(GetString(Column));
	}

	double CResultSet::GetDouble(std::string_view Column) const
	{
		return std::strtod(GetString(Column).c_str(), nullptr);
	}

	BigInt CResultSet::GetBigInt(std::string_view Column) const
	{
		const std::string Text = GetString(Column);
		if(Text.empty())
			return BigInt();
		try
		{
			return BigInt(Text);
		}
		catch(const std::exception&)
		{
			dbg_msg("SQL Error", "Failed to convert column '%.*s' to BigInt; value and exception details suppressed.", static_cast<int>(Column.size()), Column.data());
			return BigInt();
		}
	}

	nlohmann::json CResultSet::GetJson(std::string_view Column) const
	{
		const std::string Text = GetString(Column);
		if(Text.empty())
			return nullptr;
		try
		{
			return nlohmann::json::parse(Text);
		}
		catch(const nlohmann::json::parse_error&)
		{
			dbg_msg("SQL Error", "JSON from DB for column '%.*s' failed to parse; value and exception details suppressed.", static_cast<int>(Column.size()), Column.data());
			return nullptr;
		}
	}

	void CResultSet::SetColumns(std::vector<std::string> vColumns)
	{
		m_vColumns = std::move(vColumns);
		m_vRows.clear();
		m_Row = 0;
		m_Peeked = false;
		m_AffectedRows = 0;
		m_InsertId = 0;
		m_Error = {};
	}

	void CResultSet::AddRow(std::vector<std::optional<std::string>> vValues)
	{
		m_vRows.emplace_back(std::move(vValues));
	}

}
