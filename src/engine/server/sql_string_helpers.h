#ifndef ENGINE_SERVER_SQL_STRING_HELPERS_H
#define ENGINE_SERVER_SQL_STRING_HELPERS_H

#include <string>
#include <string_view>

namespace sqlstr
{
	// Hex literals can safely carry arbitrary item data without SQL quoting or truncation.
	inline std::string HexEncode(std::string_view Data)
	{
		static constexpr char DIGITS[] = "0123456789abcdef";
		std::string Result;
		Result.reserve(Data.size() * 2);
		for(unsigned char Byte : Data)
		{
			Result.push_back(DIGITS[Byte >> 4]);
			Result.push_back(DIGITS[Byte & 15]);
		}
		return Result;
	}

	// anti SQL injection
	void ClearString(char *pString, int size = 32);

	template<unsigned int size>
	class CSqlString
	{
	public:
		CSqlString() {}

		CSqlString(const char *pStr)
		{
			str_copy(m_aString, pStr, size);
			str_copy(m_aClearString, pStr, size);
			ClearString(m_aClearString, sizeof(m_aClearString));
		}

		const char* str() const { return m_aString; }
		const char* cstr() const { return m_aClearString; }

		CSqlString& operator = (const char *pStr)
		{
			str_copy(m_aString, pStr, size);
			str_copy(m_aClearString, pStr, size);
			ClearString(m_aClearString, sizeof(m_aClearString));
			return *this;
		}

	private:
		char m_aString[size];
		char m_aClearString[size * 2 - 1];
	};
}

#endif
