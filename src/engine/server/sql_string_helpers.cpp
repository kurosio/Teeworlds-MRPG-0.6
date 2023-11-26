#include "sql_string_helpers.h"

// anti SQL injection
void sqlstr::ClearString(char* pString, int size)
{
	if(pString == nullptr || size <= 0)
		return;

	std::string newString;
	newString.reserve(2 * size); // Preallocate memory for the new string

	for(int i = 0; i < size; i++)
	{
		switch(pString[i])
		{
			case '\\':
			newString.push_back('\\');
			newString.push_back('\\');
			break;
			case '\'':
			newString.push_back('\\');
			newString.push_back('\'');
			break;
			case '"':
			newString.push_back('\\');
			newString.push_back('"');
			break;
			default:
			newString.push_back(pString[i]);
			break;
		}
	}

	str_copy(pString, newString.c_str(), size);
}