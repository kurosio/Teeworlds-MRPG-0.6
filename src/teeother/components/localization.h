#ifndef TEEOTHER_COMPONENTS_LOCALIZATION_H
#define TEEOTHER_COMPONENTS_LOCALIZATION_H

#include <teeother/tl/hashtable.h>

class CLocalization
{
	class IStorageEngine* m_pStorage;
	IStorageEngine* Storage() const { return m_pStorage; }

public:
	class CLanguage
	{
	protected:
		char m_aName[64];
		char m_aFilename[64];
		char m_aParentFilename[64];
		bool m_Loaded;
		int m_Direction;

		class CEntry
		{
		public:
			char* m_apVersions;

			CEntry() : m_apVersions(nullptr) {}

			void Free()
			{
				if (m_apVersions)
				{
					delete[] m_apVersions;
					m_apVersions = nullptr;
				}
			}
		};

		hashtable< CEntry, 128 > m_Translations;

	public:
		CLanguage();
		CLanguage(const char* pName, const char* pFilename, const char* pParentFilename);
		~CLanguage();

		const char* GetParentFilename() const { return m_aParentFilename; }
		const char* GetFilename() const { return m_aFilename; }
		const char* GetName() const { return m_aName; }
		bool IsLoaded() const { return m_Loaded; }
		bool Load(IStorageEngine* pStorage);
		const char* Localize(const char* pKey) const;
	};

	enum
	{
		DIRECTION_LTR=0,
		DIRECTION_RTL,
		NUM_DIRECTIONS,
	};

protected:
	CLanguage* m_pMainLanguage;

public:
	array<CLanguage*> m_pLanguages;
	fixed_string128 m_CfgMainLanguage;

protected:
	const char* LocalizeWithDepth(const char* pLanguageCode, const char* pText, int Depth);

public:
	CLocalization(IStorageEngine* pStorage);
	virtual ~CLocalization();

	virtual bool InitConfig(int argc, const char** argv);
	virtual bool Init();
	const char* Localize(const char* pLanguageCode, const char* pText);
};

#endif