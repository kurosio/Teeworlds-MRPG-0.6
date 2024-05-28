#include "localization.h"

#include <engine/storage.h>
#include <engine/shared/config.h>
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
	if(!Tools::Files::loadFile(pFilename, &RawData))
	{
		dbg_msg("Localization", "can't open ./server_lang/index.json");
		return false;
	}

	nlohmann::json Json = nlohmann::json::parse((char*)RawData.data());
	for(auto& JsonLang : Json["language indices"])
	{
		std::string Name = JsonLang.value("name", "\0");
		std::string File = JsonLang.value("file", "\0");
		std::string Parent = JsonLang.value("parent", "\0");

		CLanguage*& pLanguage = m_pLanguages.increment();
		pLanguage = new CLanguage(Name, File, Parent);
		if(str_comp(g_Config.m_SvDefaultLanguage, pLanguage->GetFilename()) == 0)
			m_pMainLanguage = pLanguage;
	}

	return true;
}

bool CLocalization::Reload()
{
	for(int i = 0; i < m_pLanguages.size(); i++)
		delete m_pLanguages[i];
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
		return pText;

	// load and initilize language if is not loaded
	if(!pLanguage->IsLoaded())
		pLanguage->Load();

	// found result in hash map
	if(const char* pResult = pLanguage->Localize(pText))
		return pResult;

	// localize with depth
	if(pLanguage->GetParentFilename()[0] && Depth < 4)
		return LocalizeWithDepth(pLanguage->GetParentFilename(), pText, Depth + 1);

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
			Iter.data()->Free();
		++Iter;
	}
}

void CLocalization::CLanguage::Load()
{
	// untranslate does not load
	if(m_Filename == "en")
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
				free(pEntry->m_apVersions);
			const int Length = p.m_End.length() + 1;
			pEntry->m_apVersions = (char*)malloc(Length);
			if(pEntry->m_apVersions)
				str_copy(pEntry->m_apVersions, p.m_End.c_str(), Length);
		}
	}
	m_Loaded = true;
}

const char* CLocalization::CLanguage::Localize(const char* pText) const
{
	const CEntry* pEntry = m_Translations.get(pText);
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
	std::string aDirLanguageFile = std::string("./server_lang/") + m_pLanguage->GetFilename() + ".txt";
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

			Temp.m_Text = pLine;
			continue;
		}

		// check valid line
		if(!pLine)
		{
			dbg_msg("localization", "localization", "unexpected end of file");
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
		Temp.m_End = pReplacement;
		m_vElements.push_back(Temp);

		// clear tempary data
		Temp.m_Text.clear();
		Temp.m_End.clear();
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
	auto iter = std::find_if(m_vElements.begin(), m_vElements.end(), [&Hash](const Element& pItem)
	{ return pItem.m_Hash == Hash; });
	if (iter != m_vElements.end())
	{
		if(auto& pLocalize = *iter; pLocalize.m_Text != pTextKey)
			pLocalize.m_Text = pLocalize.m_End = pTextKey;
	}
	else
		m_vElements.push_back({ Hash, pTextKey, pTextKey });
}

void CLocalization::CLanguage::CUpdater::Finish()
{
	// skip if not prepared
	if(!m_Prepared)
		return;

	// order non updated translated to up
	std::sort(m_vElements.begin(), m_vElements.end(), [](const Element& p1, const Element& p2)
	{
		return p1.m_End == p1.m_Text && p2.m_End != p2.m_Text;
	});

	// save file
	std::string Data;
	for (const auto& p : m_vElements)
	{
		if (!p.m_Hash.empty())
			Data += "$" + p.m_Hash + "\n";
		Data += p.m_Text + "\n";
		Data += "== " + p.m_End;
		Data += "\n\n";
	}

	// clear data
	m_Prepared = false;
	m_vElements.clear();
	m_vElements.shrink_to_fit();

	// save
	std::string aDirLanguageFile = std::string("./server_lang/") + m_pLanguage->GetFilename() + ".txt";
	Tools::Files::saveFile(aDirLanguageFile.c_str(), (void*)Data.data(), (unsigned)Data.size());
	dbg_msg("localization", "language file %s has been updated!", m_pLanguage->GetFilename());
}