#include "localization.h"

#include <engine/shared/linereader.h>
#include <regex>

constexpr auto g_pMotherLanguageFile = "en";

std::string ProcessUnicodeEscapes(const char* input)
{
	std::string result;
	const char* p = input;

	while(*p)
	{
		// Check for \uXXXX escape sequence
		if(*p == '\\' && *(p + 1) == 'u' &&
			isxdigit(*(p + 2)) && isxdigit(*(p + 3)) &&
			isxdigit(*(p + 4)) && isxdigit(*(p + 5)))
		{
			// Parse unicode escape \uXXXX
			unsigned int unicode_value;
			int num_converted = sscanf(p + 2, "%4x", &unicode_value);

			// Check if parsing was successful and the unicode value is valid
			if(num_converted != 1 || unicode_value > 0x10FFFF)
			{
				++p;
				continue;
			}

			// convert the Unicode value to UTF-8 and append it to the result string
			char utf8_char[4];
			int utf8_length = 0;
			if(unicode_value <= 0x7F)
			{
				utf8_char[0] = static_cast<char>(unicode_value);
				utf8_length = 1;
			}
			else if(unicode_value <= 0x7FF)
			{
				utf8_char[0] = static_cast<char>(0xC0 | (unicode_value >> 6));
				utf8_char[1] = static_cast<char>(0x80 | (unicode_value & 0x3F));
				utf8_length = 2;
			}
			else if(unicode_value <= 0xFFFF)
			{
				utf8_char[0] = static_cast<char>(0xE0 | (unicode_value >> 12));
				utf8_char[1] = static_cast<char>(0x80 | ((unicode_value >> 6) & 0x3F));
				utf8_char[2] = static_cast<char>(0x80 | (unicode_value & 0x3F));
				utf8_length = 3;
			}
			else if(unicode_value <= 0x10FFFF)
			{
				utf8_char[0] = static_cast<char>(0xF0 | (unicode_value >> 18));
				utf8_char[1] = static_cast<char>(0x80 | ((unicode_value >> 12) & 0x3F));
				utf8_char[2] = static_cast<char>(0x80 | ((unicode_value >> 6) & 0x3F));
				utf8_char[3] = static_cast<char>(0x80 | (unicode_value & 0x3F));
				utf8_length = 4;
			}

			// append the UTF-8 bytes
			for(int i = 0; i < utf8_length; ++i)
				result.push_back(utf8_char[i]);

			p += 6;
		}
		else
		{
			result.push_back(*p);
			++p;
		}
	}

	return result;
}

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
		dbg_msg("Localization", "can't open ./server_lang/index.json");
		return false;
	}

	try
	{
		auto json = nlohmann::json::parse((char*)RawData.data());
		for(const auto& jsonLang : json["language indices"])
		{
			std::string Name = jsonLang.value("name", "");
			std::string File = jsonLang.value("file", "");
			std::string Parent = jsonLang.value("parent", "");

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
		dbg_msg("Localization", "JSON parse error: %s", e.what());
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

CLocalization::CLanguage::CLanguage(const std::string& Name, const std::string& Filename, const std::string& ParentFilename)
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
	// try load language file
	std::string aDirLanguageFile = fmt_default("./server_lang/{}.txt", m_pLanguage->GetFilename());
	IOHANDLE File = io_open(aDirLanguageFile.c_str(), IOFLAG_READ | IOFLAG_SKIP_BOM);
	if(!File)
		return false;

	// prepare
	m_vElements.clear();
	m_vElements.reserve(128);

	Element Temp;
	CLineReader LineReader;
	LineReader.Init(File);

	char* pLine;
	while((pLine = LineReader.Get()))
	{
		// try to initialize hash
		if(pLine[0] == '$')
		{
			Temp.m_Hash = pLine + 1;
			continue;
		}

		// first initialize default string
		if(Temp.m_Text.empty())
		{
			// skip if empty or comments
			if(!str_length(pLine) || pLine[0] == '#')
				continue;

			Temp.m_Text = ProcessUnicodeEscapes(pLine);
			continue;
		}

		// check valid line
		if(!pLine)
		{
			dbg_msg("localization", "unexpected end of file");
			break;
		}

		// check is localize field
		if(pLine[0] != '=' || pLine[1] != '=' || pLine[2] != ' ')
		{
			dbg_msg("localization", "malformed replacement line for '%s'", Temp.m_Text.c_str());
			continue;
		}

		// initialize element
		const char* pReplacement = (pLine + 3);
		Temp.m_Result = ProcessUnicodeEscapes(pReplacement);
		m_vElements.push_back(Temp);

		// clear tempary data
		Temp.m_Text.clear();
		Temp.m_Result.clear();
		Temp.m_Hash.clear();
	}

	m_Prepared = true;
	io_close(File);
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

	// order non updated translated to up
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

		std::string EscapedText = std::regex_replace(p.m_Text, std::regex("\n"), "\\n");
		std::string EscapedResult = std::regex_replace(p.m_Result, std::regex("\n"), "\\n");

		Data += EscapedText + "\n";
		Data += "== " + EscapedResult + "\n\n";
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