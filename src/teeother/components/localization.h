#ifndef TEEOTHER_COMPONENTS_LOCALIZATION_H
#define TEEOTHER_COMPONENTS_LOCALIZATION_H

class CLocalization
{
public:
	class CLanguage
	{
		class CEntry
		{
		public:
			char* m_apVersions{};
			void Free()
			{
				if(m_apVersions)
				{
					free(m_apVersions);
					m_apVersions = nullptr;
				}
			}
		};

		class CUpdater
		{
			friend class CLanguage;
			struct Element
			{
				std::string m_Hash {};
				std::string m_Text {};
				std::string m_Result {};
			};
			CLanguage* m_pLanguage {};
			std::vector<Element> m_vElements {};
			bool m_Prepared {};

		public:
			bool LoadDefault(std::vector<Element>& vElements);
			bool Prepare();
			void Push(const char* pTextKey, const char* pUnique, int ID);
			void Finish();
		};

		std::string m_Name;
		std::string m_Filename;
		std::string m_ParentFilename;
		bool m_Loaded;
		hashtable< CEntry, 128 > m_Translations;
		CUpdater m_Updater;

	public:
		CLanguage(std::string_view Name, std::string_view Filename, std::string_view ParentFilename);
		~CLanguage();

		const char* GetParentFilename() const { return m_ParentFilename.c_str(); }
		const char* GetFilename() const { return m_Filename.c_str(); }
		const char* GetName() const { return m_Name.c_str(); }
		bool IsLoaded() const { return m_Loaded; }
		void Load();
		const char* Localize(const char* pKey) const;
		CUpdater& Updater() { return m_Updater; }
	};

	~CLocalization();

	bool Init();
	bool Reload();
	const char* Localize(const char* pLanguageCode, const char* pText);
	array<CLanguage*> m_pLanguages{};

private:
	CLanguage* m_pMainLanguage{};
	const char* LocalizeWithDepth(const char* pLanguageFile, const char* pText, int Depth);
};

#endif