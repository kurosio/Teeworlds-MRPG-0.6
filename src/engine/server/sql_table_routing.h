#ifndef ENGINE_SERVER_SQL_TABLE_ROUTING_H
#define ENGINE_SERVER_SQL_TABLE_ROUTING_H

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace sqlroute
{
	inline constexpr std::string_view LOCAL_DUMP_TABLES[] = {
		"tw_auction_slots",
		"tw_groups",
		"tw_guilds_decorations",
		"tw_guilds_history",
		"tw_guilds_invites",
		"tw_guilds_ranks",
		"tw_guilds_wars",
		"tw_houses_decorations",
		"tw_links",
	};

	inline char ToLower(char c)
	{
		return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
	}

	inline bool IsIdentifierChar(char c)
	{
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '$';
	}

	inline std::string ToLowerName(std::string_view Name)
	{
		std::string Result(Name);
		for(char& c : Result)
			c = ToLower(c);
		return Result;
	}

	// Compares an identifier with a lower case name, the case is ignored.
	inline bool IsSameName(std::string_view Identifier, std::string_view LowerName)
	{
		if(Identifier.size() != LowerName.size())
			return false;

		for(size_t i = 0; i < LowerName.size(); ++i)
		{
			if(ToLower(Identifier[i]) != LowerName[i])
				return false;
		}
		return true;
	}

	// True if the list of lower case names has the name, the case is ignored.
	inline bool HasName(const std::vector<std::string>& vLowerNames, std::string_view Name)
	{
		for(const std::string& LowerName : vLowerNames)
		{
			if(IsSameName(Name, LowerName))
				return true;
		}
		return false;
	}

	// Returns the position after a string literal that starts at Start.
	// A backslash or a doubled quote escapes the quote.
	inline size_t SkipLiteral(std::string_view Sql, size_t Start)
	{
		const char Quote = Sql[Start];
		for(size_t i = Start + 1; i < Sql.size(); ++i)
		{
			if(Sql[i] == '\\')
			{
				++i;
			}
			else if(Sql[i] == Quote)
			{
				if(i + 1 >= Sql.size() || Sql[i + 1] != Quote)
					return i + 1;
				++i;
			}
		}
		return Sql.size();
	}

	// Returns the position of the backtick that closes the identifier quoted at
	// Start, or the size of the statement if it is not closed.
	inline size_t FindQuotedIdentifierEnd(std::string_view Sql, size_t Start)
	{
		for(size_t i = Start + 1; i < Sql.size(); ++i)
		{
			if(Sql[i] != '`')
				continue;
			if(i + 1 >= Sql.size() || Sql[i + 1] != '`')
				return i;
			++i; // a doubled backtick is a part of the name
		}
		return Sql.size();
	}

	// True if the statement uses a table of the list (lower case names).
	// String literals are skipped: a table name inside a value is not a table.
	inline bool UsesTableOf(std::string_view Sql, const std::vector<std::string>& vLowerNames)
	{
		size_t i = 0;
		while(i < Sql.size())
		{
			const char c = Sql[i];
			if(c == '\'' || c == '"')
			{
				i = SkipLiteral(Sql, i);
			}
			else if(c == '`')
			{
				const size_t End = FindQuotedIdentifierEnd(Sql, i);
				if(End >= Sql.size())
					return false; // the name is not closed, nothing follows it

				if(HasName(vLowerNames, Sql.substr(i + 1, End - i - 1)))
					return true;
				i = End + 1;
			}
			else if(IsIdentifierChar(c))
			{
				const size_t Start = i;
				while(i < Sql.size() && IsIdentifierChar(Sql[i]))
					++i;

				if(HasName(vLowerNames, Sql.substr(Start, i - Start)))
					return true;
			}
			else
			{
				++i;
			}
		}
		return false;
	}
}

#endif // ENGINE_SERVER_SQL_TABLE_ROUTING_H
