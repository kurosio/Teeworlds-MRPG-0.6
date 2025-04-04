#include "localization.h"

#include <engine/shared/linereader.h>

constexpr auto g_pMotherLanguageFile = "en";

CLocalization::~CLocalization()
{
	for(int i = 0; i < m_pLanguages.size(); i++)
		delete m_pLanguages[i];
}

bool CLocalization::Init()
{
	// loading file is not open pereinitilized steps
	ByteArray RawData;
	const char* pFilename = "./server_lang/index.json";
	if(!mystd::file::load(pFilename, &RawData))
	{
		dbg_msg("localization", "can't open ./server_lang/index.json");
		return false;
	}

	try
	{
		auto json = nlohmann::json::parse((char*)RawData.data());
		for(const auto& jsonLang : json["language indices"])
		{
			auto Name = jsonLang.value("name", "");
			auto File = jsonLang.value("file", "");
			auto Parent = jsonLang.value("parent", "");

			CLanguage*& pLanguage = m_pLanguages.increment();
			pLanguage = new CLanguage(Name, File, Parent);

			if(str_comp(g_Config.m_SvDefaultLanguage, pLanguage->GetFilename()) == 0)
			{
				m_pMainLanguage = pLanguage;
			}
		}
	}
	catch(const std::exception& e)
	{
		dbg_msg("localization", "JSON parse error: %s", e.what());
		return false;
	}

	return true;
}

bool CLocalization::Reload()
{
	for(int i = 0; i < m_pLanguages.size(); i++)
	{
		delete m_pLanguages[i];
	}

	m_pLanguages.clear();
	m_pMainLanguage = nullptr;
	return Init();
}

const char* CLocalization::LocalizeWithDepth(const char* pLanguageFile, const char* pText, int Depth)
{
	// found language
	CLanguage* pLanguage = m_pMainLanguage;
	if(pLanguageFile)
	{
		for(int i = 0; i < m_pLanguages.size(); i++)
		{
			if(str_comp(m_pLanguages[i]->GetFilename(), pLanguageFile) == 0)
			{
				pLanguage = m_pLanguages[i];
				break;
			}
		}
	}

	// if no language is found or is mother language
	if(!pLanguage || str_comp(pLanguage->GetFilename(), g_pMotherLanguageFile) == 0)
	{
		return pText;
	}

	// load and initilize language if is not loaded
	if(!pLanguage->IsLoaded())
	{
		pLanguage->Load();
	}

	// found result in hash map
	if(const char* pResult = pLanguage->Localize(pText))
	{
		return pResult;
	}

	// recursive localize with depth
	if(pLanguage->GetParentFilename()[0] && Depth < 4)
	{
		return LocalizeWithDepth(pLanguage->GetParentFilename(), pText, Depth + 1);
	}

	// return default text
	return pText;
}

const char* CLocalization::Localize(const char* pLanguageCode, const char* pText)
{
	return LocalizeWithDepth(pLanguageCode, pText, 0);
}

CLocalization::CLanguage::CLanguage(std::string_view Name, std::string_view Filename, std::string_view ParentFilename)
{
	m_Loaded = false;
	m_Name = Name;
	m_Filename = Filename;
	m_ParentFilename = ParentFilename;
	m_Updater.m_pLanguage = this;

	Load();
}

CLocalization::CLanguage::~CLanguage()
{
	auto Iter = m_Translations.begin();
	while(Iter != m_Translations.end())
	{
		if(Iter.data())
		{
			Iter.data()->Free();
		}

		++Iter;
	}
}

void CLocalization::CLanguage::Load()
{
	// untranslate does not load
	if (m_Filename == "en")
	{
		m_Loaded = true;
		return;
	}

	// load language file
	std::vector<CUpdater::Element> vElements;
	if(!m_Updater.LoadDefault(vElements))
	{
		dbg_msg("localization", "failed to load language '%s'", m_Filename.c_str());
		return;
	}

	// initialize localization hashtable
	dbg_msg("localization", "successful loaded language '%s'", m_Filename.c_str());
	for(auto& p : vElements)
	{
		if(CEntry* pEntry = m_Translations.set(p.m_Text.c_str()))
		{
			if(pEntry->m_apVersions)
			{
				free(pEntry->m_apVersions);
			}

			const int Length = p.m_Result.length() + 1;
			pEntry->m_apVersions = (char*)malloc(Length);
			if(pEntry->m_apVersions)
			{
				str_copy(pEntry->m_apVersions, p.m_Result.c_str(), Length);
			}
		}
	}

	m_Loaded = true;
}

const char* CLocalization::CLanguage::Localize(const char* pKey) const
{
	const CEntry* pEntry = m_Translations.get(pKey);
	if(!pEntry)
		return nullptr;

	return pEntry->m_apVersions;
}

bool CLocalization::CLanguage::CUpdater::LoadDefault(std::vector<Element>& vElements)
{
	if(Prepare())
	{
		vElements = m_vElements;
		m_vElements.clear();
		m_Prepared = false;
		return true;
	}
	return false;
}

bool CLocalization::CLanguage::CUpdater::Prepare()
{
	std::string aDirLanguageFile = fmt_default("./server_lang/{}.txt", m_pLanguage->GetFilename());

	CLineReader LineReader;
	if(!LineReader.OpenFile(io_open(aDirLanguageFile.c_str(), IOFLAG_READ)))
		return false;

	m_vElements.clear();
	m_vElements.reserve(512);

	Element Temp;
	while(const char* pReadLine = LineReader.Get())
	{
		std::string Line = pReadLine;

		if(Line.empty() || Line[0] == '#')
			continue;

		if(Line[0] == '$')
		{
			Temp.m_Hash = Line.substr(1);
			continue;
		}

		if(Line.rfind("== ", 0) == 0)
		{
			if(Temp.m_Text.empty())
			{
				dbg_msg("localization", "replacement without default string");
				continue;
			}

			std::string Replacement = Line.substr(3);
			Temp.m_Result = mystd::string::unescape(Replacement);
			m_vElements.push_back(Temp);

			Temp.m_Text.clear();
			Temp.m_Result.clear();
			Temp.m_Hash.clear();
			continue;
		}

		if(!Temp.m_Text.empty())
		{
			dbg_msg("localization", "unexpected default string: '%s'", Line.c_str());
			continue;
		}

		Temp.m_Text = mystd::string::unescape(pReadLine);
	}

	m_Prepared = true;
	return true;
}

void CLocalization::CLanguage::CUpdater::Push(const char* pTextKey, const char* pUnique, int ID)
{
	// skip if empty key or not prepared
	if (pTextKey[0] == '\0' || !m_Prepared)
		return;

	// update localize element
	std::string Hash(pUnique + std::to_string(ID));
	auto iter = std::ranges::find_if(m_vElements, [&Hash](const Element& pItem)
	{
		return pItem.m_Hash == Hash;
	});

	if (iter != m_vElements.end())
	{
		auto& pLocalize = *iter;
		if(pLocalize.m_Text != pTextKey)
		{
			pLocalize.m_Text = pLocalize.m_Result = pTextKey;
		}
	}
	else
	{
		m_vElements.push_back({ Hash, pTextKey, pTextKey });
	}
}

void CLocalization::CLanguage::CUpdater::Finish()
{
	// skip if not prepared
	if(!m_Prepared)
		return;

	//// order non updated translated to up
	std::ranges::sort(m_vElements, [](const Element& p1, const Element& p2)
	{
		return p1.m_Result == p1.m_Text && p2.m_Result != p2.m_Text;
	});

	// save file
	std::string Data;
	for(const auto& p : m_vElements)
	{
		if(!p.m_Hash.empty())
		{
			Data += "$" + p.m_Hash + "\n";
		}

		auto escapedBase = mystd::string::escape(p.m_Text);
		auto escapedResult = mystd::string::escape(p.m_Result);
		Data += escapedBase + "\n";
		Data += "== " + escapedResult + "\n\n";
	}

	// clear data
	m_Prepared = false;
	m_vElements.clear();
	m_vElements.shrink_to_fit();

	// save
	std::string aDirLanguageFile = fmt_default("./server_lang/{}.txt", m_pLanguage->GetFilename());
	mystd::file::save(aDirLanguageFile.c_str(), (void*)Data.data(), (unsigned)Data.size());
	dbg_msg("localization", "language file %s has been updated!", m_pLanguage->GetFilename());
}