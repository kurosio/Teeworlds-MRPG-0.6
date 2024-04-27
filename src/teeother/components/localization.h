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

private:
	// reformat types to string
	template <typename T>
	std::string ToString(const char* pLanguageCode, T Value)
	{
		if constexpr(std::is_same_v<T, double> || std::is_same_v<T, float>)
		{
			return std::to_string(Value);
		}
		else if constexpr(std::is_arithmetic_v<T>)
		{
			return get_commas(std::to_string(Value));
		}
		else if constexpr(std::is_same_v<T, BigInt>)
		{
			return get_label(Value.to_string());
		}
		else if constexpr(std::is_convertible_v<T, std::string>)
		{
			std::string localizeFmt(Localize(pLanguageCode, std::string(Value).c_str()));
			return localizeFmt;
		}
		else
		{
			static_assert(false, "One of the passed arguments cannot be converted to a string");
		}

		return "error convertable";
	}

	// end unpacking args function
	std::string FormatImpl(const char*, const char* pText, std::deque<std::string>& vStrPack)
	{
		std::string Result{};
		bool ArgStarted = false;
		int TextLength = str_length(pText);

		// found args
		for(int i = 0; i <= TextLength; ++i)
		{
			bool IsLast = (i == TextLength);
			char IterChar = pText[i];

			// start arg iterate
			if(IterChar == '{')
			{
				ArgStarted = true;
				continue;
			}

			// arg started
			if(ArgStarted)
			{
				if(IterChar == '}')
				{
					ArgStarted = false;
					if(!vStrPack.empty())
					{
						Result += vStrPack.front();
						vStrPack.pop_front();
					}
				}
				continue;
			}

			// add next char
			if(!IsLast)
			{
				Result += IterChar;
			}
		}

		// result string
		return Result;
	}

	// unpacking ellipsis args pack
	template<typename Arg, typename ... Args>
	std::string FormatImpl(const char* pLanguageCode, const char* pText, std::deque<std::string>& vStrPack, Arg&& arg, Args&& ... argsfmt)
	{
		vStrPack.push_back(ToString(pLanguageCode, std::forward<Arg>(arg)));
		return FormatImpl(pLanguageCode, pText, vStrPack, std::forward<Args>(argsfmt) ...);
	}

public:
	// format without args pack
	std::string Format(const char* pLanguageCode, const char* pText)
	{
		return std::string(Localize(pLanguageCode, pText));
	}

	// format args pack
	template<typename ... Args>
	std::string Format(const char* pLanguageCode, const char* pText, Args&& ... argsfmt)
	{
		std::deque<std::string> vStrPack;
		return FormatImpl(pLanguageCode, Localize(pLanguageCode, pText), vStrPack, std::forward<Args>(argsfmt) ...);
	}

	// format with dynamic_string
	template<typename ... Args>
	void Format(dynamic_string& Buffer, const char* pLanguageCode, const char* pText, Args&& ... argsfmt)
	{
		std::string fmtStr = Format(pLanguageCode, pText, std::forward<Args>(argsfmt) ...);
		Buffer.append(fmtStr.c_str());
	}

	// Localize
	const char* Localize(const char* pLanguageCode, const char* pText);
};

#endif