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
		return true; // return true because it's not a critical error

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