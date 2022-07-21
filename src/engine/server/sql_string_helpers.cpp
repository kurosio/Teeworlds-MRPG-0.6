#include "sql_string_helpers.h"

void sqlstr::FuzzyString(char *pString, int size)
{
	char * newString = new char [size * 4 - 1];
	int pos = 0;

	for(int i = 0; i < size; i++)
	{
		if(!pString[i])
			break;

		newString[pos++] = pString[i];
		if (pString[i] != '\\' && str_utf8_isstart(pString[i+1]))
			newString[pos++] = '%';
	}

	newString[pos] = '\0';
	str_copy(pString, newString, size);
	delete [] newString;
}

// anti SQL injection
void sqlstr::ClearString(char *pString, int size)
{
	char *newString = new char [size * 2 - 1];
	int pos = 0;

	for(int i = 0; i < size; i++)
	{
		if(pString[i] == '\\')
		{
			newString[pos++] = '\\';
			newString[pos++] = '\\';
		}
		else if(pString[i] == '\'')
		{
			newString[pos++] = '\\';
			newString[pos++] = '\'';
		}
		else if(pString[i] == '"')
		{
			newString[pos++] = '\\';
			newString[pos++] = '"';
		}
		else
		{
			newString[pos++] = pString[i];
		}
	}

	newString[pos] = '\0';

	str_copy(pString, newString, size);
	delete [] newString;
}
