#include <engine/external/json-parser/json.h>

#include <engine/storage.h>

#include "localization.h"

CLocalization::CLanguage::CLanguage() : m_Loaded(false), m_Direction(CLocalization::DIRECTION_LTR)
{
	m_aName[0] = 0;
	m_aFilename[0] = 0;
	m_aParentFilename[0] = 0;
}

CLocalization::CLanguage::CLanguage(const char* pName, const char* pFilename, const char* pParentFilename) : m_Loaded(false), m_Direction(CLocalization::DIRECTION_LTR)
{
	str_copy(m_aName, pName, sizeof(m_aName));
	str_copy(m_aFilename, pFilename, sizeof(m_aFilename));
	str_copy(m_aParentFilename, pParentFilename, sizeof(m_aParentFilename));
}

CLocalization::CLanguage::~CLanguage()
{
	hashtable< CEntry, 128 >::iterator Iter = m_Translations.begin();
	while(Iter != m_Translations.end())
	{
		if(Iter.data())
			Iter.data()->Free();

		++Iter;
	}
}

bool CLocalization::CLanguage::Load(CLocalization* pLocalization, IStorageEngine* pStorage)
{
	// read file data into buffer
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "./server_lang/%s.json", m_aFilename);
	const IOHANDLE File = pStorage->OpenFile(aBuf, IOFLAG_READ, IStorageEngine::TYPE_ALL);
	if(!File)
		return false;

	const int FileSize = (int)io_length(File);
	char* pFileData = (char*)malloc(FileSize);
	io_read(File, pFileData, FileSize);
	io_close(File);

	// parse json data
	json_settings JsonSettings;
	mem_zero(&JsonSettings, sizeof(JsonSettings));
	char aError[256];
	json_value* pJsonData = json_parse_ex(&JsonSettings, pFileData, FileSize, aError);
	free(pFileData);

	if(pJsonData == nullptr)
	{
		dbg_msg("Localization", "Can't load the localization file %s : %s", aBuf, aError);
		return false;
	}

	dynamic_string Buffer;
	int Length;

	// extract data
	const json_value& rStart = (*pJsonData)["translation"];
	if(rStart.type == json_array)
	{
		for(unsigned i = 0; i < rStart.u.array.length; ++i)
		{
			const char* pKey = rStart[i]["key"];
			if(pKey && pKey[0])
			{
				CEntry* pEntry = m_Translations.set(pKey);

				const char* pSingular = rStart[i]["value"];
				if(pSingular && pSingular[0])
				{
					Length = str_length(pSingular) + 1;
					pEntry->m_apVersions = new char[Length];
					str_copy(pEntry->m_apVersions, pSingular, Length);
				}
			}
		}
	}

	// clean up
	json_value_free(pJsonData);
	m_Loaded = true;

	return true;
}

const char* CLocalization::CLanguage::Localize(const char* pText) const
{
	const CEntry* pEntry = m_Translations.get(pText);
	if(!pEntry)
		return nullptr;

	return pEntry->m_apVersions;
}

CLocalization::CLocalization(IStorageEngine* pStorage) : m_pStorage(pStorage), m_pMainLanguage(nullptr)
{ }

CLocalization::~CLocalization()
{
	for(int i = 0; i < m_pLanguages.size(); i++)
		delete m_pLanguages[i];
}

bool CLocalization::InitConfig(int argc, const char** argv)
{
	m_Cfg_MainLanguage.copy("en");
	return true;
}

bool CLocalization::Init()
{
	// read file data into buffer
	const char* pFilename = "./server_lang/index.json";
	IOHANDLE File = Storage()->OpenFile(pFilename, IOFLAG_READ, IStorageEngine::TYPE_ALL);
	if(!File)
	{
		dbg_msg("Localization", "can't open ./server_lang/index.json");
		return false;
	}

	const int FileSize = (int)io_length(File);
	char* pFileData = (char*)malloc(FileSize);
	io_read(File, pFileData, FileSize);
	io_close(File);

	// parse json data
	json_settings JsonSettings;
	mem_zero(&JsonSettings, sizeof(JsonSettings));
	char aError[256];
	json_value* pJsonData = json_parse_ex(&JsonSettings, pFileData, FileSize, aError);
	free(pFileData);
	if(pJsonData == nullptr)
	{
		delete[] pFileData;
		return true; // return true because it's not a critical error
	}

	// extract data
	m_pMainLanguage = nullptr;
	const json_value& rStart = (*pJsonData)["language indices"];
	if(rStart.type == json_array)
	{
		for(unsigned i = 0; i < rStart.u.array.length; ++i)
		{
			CLanguage*& pLanguage = m_pLanguages.increment();
			pLanguage = new CLanguage((const char*)rStart[i]["name"], (const char*)rStart[i]["file"], (const char*)rStart[i]["parent"]);

			if(m_Cfg_MainLanguage == pLanguage->GetFilename())
			{
				pLanguage->Load(this, Storage());
				m_pMainLanguage = pLanguage;
			}
		}
	}

	// clean up
	json_value_free(pJsonData);
	return true;
}

const char* CLocalization::LocalizeWithDepth(const char* pLanguageCode, const char* pText, int Depth)
{
	CLanguage* pLanguage = m_pMainLanguage;
	if(pLanguageCode)
	{
		for(int i = 0; i < m_pLanguages.size(); i++)
		{
			if(str_comp(m_pLanguages[i]->GetFilename(), pLanguageCode) == 0)
			{
				pLanguage = m_pLanguages[i];
				break;
			}
		}
	}

	if(!pLanguage)
		return pText;

	if(!pLanguage->IsLoaded())
		pLanguage->Load(this, Storage());

	const char* pResult = pLanguage->Localize(pText);
	if(pResult)
		return pResult;
	if(pLanguage->GetParentFilename()[0] && Depth < 4)
		return LocalizeWithDepth(pLanguage->GetParentFilename(), pText, Depth + 1);
	return pText;
}

const char* CLocalization::Localize(const char* pLanguageCode, const char* pText)
{
	return LocalizeWithDepth(pLanguageCode, pText, 0);
}

static char* format_integer_with_commas(char commas, int n)
{
	char _number_array[64] = { '\0' };
	str_format(_number_array, sizeof(_number_array), "%d", n); // %ll

	const char* _number_pointer = _number_array;
	int _number_of_digits = 0;
	while (*(_number_pointer + _number_of_digits++));
	--_number_of_digits;

	/*
	*	count the number of digits
	*	calculate the position for the first comma separator
	*	calculate the final length of the number with commas
	*
	*	the starting position is a repeating sequence 123123... which depends on the number of digits
	*	the length of the number with commas is the sequence 111222333444...
	*/
	const int _starting_separator_position = _number_of_digits < 4 ? 0 : _number_of_digits % 3 == 0 ? 3 : _number_of_digits % 3;
	const int _formatted_number_length = _number_of_digits + _number_of_digits / 3 - (_number_of_digits % 3 == 0 ? 1 : 0);

	// create formatted number array based on calculated information.
	char* _formatted_number = new char[20 * 3 + 1];

	// place all the commas
	for (int i = _starting_separator_position; i < _formatted_number_length - 3; i += 4)
		_formatted_number[i] = commas;

	// place the digits
	for (int i = 0, j = 0; i < _formatted_number_length; i++)
		if (_formatted_number[i] != commas)
			_formatted_number[i] = _number_pointer[j++];

	/* close the string */
	_formatted_number[_formatted_number_length] = '\0';
	return _formatted_number;
}

void CLocalization::Format_V(dynamic_string& Buffer, const char* pLanguageCode, const char* pText, va_list VarArgs)
{
	CLanguage* pLanguage = m_pMainLanguage;
	if(pLanguageCode)
	{
		for(int i = 0; i < m_pLanguages.size(); i++)
		{
			if(str_comp(m_pLanguages[i]->GetFilename(), pLanguageCode) != 0)
				continue;

			pLanguage = m_pLanguages[i];
			break;
		}
	}

	if(!pLanguage)
	{
		Buffer.append(pText);
		return;
	}

	// start parameters of the end of the name and type string
	const int BufferStart = Buffer.length();
	int BufferIter = BufferStart;
	int ParamTypeStart = -1;

	// argument parsing
	va_list VarArgsIter;
	va_copy(VarArgsIter, VarArgs);

	// character positions
	int Iter = 0;
	int Start = 0;

	// parse text to search for positions
	while(pText[Iter])
	{
		if(ParamTypeStart >= 0)
		{
			if(pText[Iter] != '}')
			{
				Iter = str_utf8_forward(pText, Iter);
				continue;
			}

			// we get data from an argument parsing arguments
			if(str_comp_num("STR", pText + ParamTypeStart, 3) == 0) // string
			{
				const char* pVarArgValue = va_arg(VarArgsIter, const char*);
				const char* pTranslatedValue = pLanguage->Localize(pVarArgValue);
				BufferIter = Buffer.append_at(BufferIter, (pTranslatedValue ? pTranslatedValue : pVarArgValue));
			}
			else if(str_comp_num("INT", pText + ParamTypeStart, 3) == 0) // intiger
			{
				char aBuf[128];
				const int pVarArgValue = va_arg(VarArgsIter, int);
				str_format(aBuf, sizeof(aBuf), "%d", pVarArgValue); // %ll
				BufferIter = Buffer.append_at(BufferIter, aBuf);
			}
			else if(str_comp_num("VAL", pText + ParamTypeStart, 3) == 0) // value
			{
				const int pVarArgValue = va_arg(VarArgsIter, int);
				char* aBuffer = format_integer_with_commas(',', pVarArgValue);
				BufferIter = Buffer.append_at(BufferIter, aBuffer);
				delete[] aBuffer;
			}

			//
			Start = Iter + 1;
			ParamTypeStart = -1;
		}

		// parameter parsing start
		else
		{
			if(pText[Iter] == '{')
			{
				BufferIter = Buffer.append_at_num(BufferIter, pText + Start, Iter - Start);
				Iter++;
				ParamTypeStart = Iter;
			}
		}

		Iter = str_utf8_forward(pText, Iter);
	}

	// close the argument macro
	va_end(VarArgsIter);

	if(Iter > 0 && ParamTypeStart == -1)
		Buffer.append_at_num(BufferIter, pText + Start, Iter - Start);
}

void CLocalization::Format(dynamic_string& Buffer, const char* pLanguageCode, const char* pText, ...)
{
	va_list VarArgs;
	va_start(VarArgs, pText);

	Format_V(Buffer, pLanguageCode, pText, VarArgs);

	va_end(VarArgs);
}

void CLocalization::Format_VL(dynamic_string& Buffer, const char* pLanguageCode, const char* pText, va_list VarArgs)
{
	const char* pLocalText = Localize(pLanguageCode, pText);

	Format_V(Buffer, pLanguageCode, pLocalText, VarArgs);
}

void CLocalization::Format_L(dynamic_string& Buffer, const char* pLanguageCode, const char* pText, ...)
{
	va_list VarArgs;
	va_start(VarArgs, pText);

	Format_VL(Buffer, pLanguageCode, pText, VarArgs);

	va_end(VarArgs);
}
