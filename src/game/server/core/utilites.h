/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_TEMPLATE_UTILS_H
#define GAME_SERVER_CORE_TEMPLATE_UTILS_H

using ByteArray = std::basic_string<std::byte>;

namespace mrpgstd
{
	// сoncept to check if a type has a clear() function
	template<typename T>
	concept is_has_clear_function = requires(T & c) {
		{ c.clear() } -> std::same_as<void>;
	};

	// сoncept to check if a type is a container
	template <typename T>
	concept is_container = requires(T a) {
		typename T::value_type;
		typename T::iterator;
		{ a.begin() } -> std::convertible_to<typename T::iterator>;
		{ a.end() } -> std::convertible_to<typename T::iterator>;
	};

	// сoncept to check if a container holds pointer elements
	template<typename is_container>
	concept is_container_pointers_element = std::is_pointer_v<typename is_container::value_type>;

	// сoncept to check if a type is a map container
	template<typename T>
	concept is_map_container = requires(T a) {
		typename T::key_type;
		typename T::mapped_type;
			requires std::same_as<typename T::value_type, std::pair<const typename T::key_type, typename T::mapped_type>>;
	};

	// сoncept to check if a map container holds pointer values
	template<typename is_map_container>
	concept is_map_container_pointers_element = std::is_pointer_v<typename is_map_container::mapped_type>;

	// function to clear a container
	template<typename T>
	void cleaning_free_container_data(T& container)
	{
		static_assert(is_has_clear_function<T>, "One or more types do not have clear() function");
		if constexpr(is_container_pointers_element<T>)
			std::ranges::for_each(container, [](auto& element) { delete element; });
		else if constexpr(is_container<T> && (is_container<typename T::value_type> || is_map_container<typename T::value_type>))
			std::ranges::for_each(container, [](auto& element) { cleaning_free_container_data(element); });
		else if constexpr(is_map_container_pointers_element<T>)
			std::ranges::for_each(container, [](auto& element) { delete element.second; });
		else if constexpr(is_map_container<T> && (is_container<typename T::mapped_type> || is_map_container<typename T::mapped_type>))
			std::ranges::for_each(container, [](auto& element) { cleaning_free_container_data(element.second); });
		std::cout << typeid(T).name();
		container.clear();
	}

	// clear multiple containers uses fold expression
	template<typename... Containers>
	void cleaning_free_container_data(Containers&... args) { (cleaning_free_container_data(args), ...); }
}

// aesthetic utils (TODO: remove)
namespace Utils::Aesthetic
{
	namespace Impl
	{
		struct AestheticImpl
		{
			AestheticImpl() = default;
			AestheticImpl(const char* pUnique, const char* pRUnique, const char* pSnake, int SnakesIter, bool Post)
			{
				if(pSnake != nullptr)
				{
					m_Detail.m_Post = Post;
					m_Detail.m_SnakesIter = SnakesIter;
					str_copy(m_Detail.aBufSnakeIter, pSnake, sizeof(m_Detail.aBufSnakeIter));
					for(int i = 0; i < SnakesIter; i++) { str_append(m_Detail.aBufSnake, pSnake, sizeof(m_Detail.aBufSnake)); }
					if(Post && m_Detail.aBufSnake[0] != '\0') { str_utf8_reverse(m_Detail.aBufSnake); }
				}
				if(pUnique != nullptr) { str_append(m_Detail.aBufUnique, pUnique, sizeof(m_Detail.aBufUnique)); }
				if(pRUnique != nullptr) { str_append(m_Detail.aBufRUnique, pRUnique, sizeof(m_Detail.aBufRUnique)); }
				if(Post) { str_format(m_aData, sizeof(m_aData), "%s%s%s", m_Detail.aBufRUnique, m_Detail.aBufSnake, m_Detail.aBufUnique); }
				else { str_format(m_aData, sizeof(m_aData), "%s%s%s", m_Detail.aBufUnique, m_Detail.aBufSnake, m_Detail.aBufRUnique); }
			}
			struct Detail
			{
				char aBufUnique[64] {};
				char aBufSnake[128] {};
				char aBufRUnique[64] {};
				char aBufSnakeIter[64] {};
				int m_SnakesIter {};
				bool m_Post {};
			};
			Detail m_Detail {};
			char m_aData[256] {};
		};

		struct CompareAestheticDetail
		{
			bool operator()(const AestheticImpl::Detail& D1, const char* pUnique, const char* pRUnique, const char* pSnake, int SnakesIter, bool Post) const
			{
				bool cUnique = !pUnique || std::string(D1.aBufUnique) == pUnique;
				bool cRUnique = !pRUnique || std::string(D1.aBufRUnique) == pRUnique;
				bool cSnake = !pSnake || std::string(D1.aBufSnakeIter) == pSnake;
				return cUnique && cRUnique && cSnake && D1.m_SnakesIter == SnakesIter && D1.m_Post == Post;
			}
		};
		inline ska::flat_hash_set<AestheticImpl*> m_aResultCollection {};

		inline AestheticImpl* AestheticText(const char* pUnique, const char* pRUnique, const char* pSnake, int SnakesIter, bool Post)
		{
			const auto iter = std::find_if(m_aResultCollection.begin(), m_aResultCollection.end(),
				[&](const AestheticImpl* pAest)
			{
				return CompareAestheticDetail {}(pAest->m_Detail, pUnique, pRUnique, pSnake, SnakesIter, Post);
			});
			if(iter == m_aResultCollection.end())
			{
				auto* pData = new AestheticImpl(pUnique ? pUnique : "\0", pRUnique ? pRUnique : "\0", pSnake ? pSnake : "\0", SnakesIter, Post);
				m_aResultCollection.emplace(pData);
				return pData;
			}
			return *iter;
		}
	};

	/* BORDURES */
	// Example: ┏━━━━━ ━━━━━┓
	inline const char* B_DEFAULT_TOP(int iter, bool post) {
		return Impl::AestheticText(post ? "\u2513" : "\u250F", nullptr, "\u2501", iter, post)->m_aData;
	}
	// Example: ───※ ·· ※───
	inline const char* B_PILLAR(int iter, bool post) {
		return Impl::AestheticText(nullptr, post ? "\u00B7 \u203B" : "\u203B \u00B7", "\u2500", iter, post)->m_aData;
	}
	// Example: ✯¸.•*•✿✿•*•.¸✯
	inline const char* B_FLOWER(bool post) {
		return Impl::AestheticText(post ? "\u273F\u2022*\u2022.\u00B8\u272F" : "\u272F\u00B8.\u2022*\u2022\u273F", nullptr, nullptr, 0, false)->m_aData;
	}
	// Example: ──⇌ • • ⇋──
	inline const char* B_CONFIDENT(int iter, bool post) {
		return Impl::AestheticText(nullptr, post ? "\u2022 \u21CB" : "\u21CC \u2022", "\u2500", iter, post)->m_aData;
	}
	// Example: •·.·''·.·•Text•·.·''·.·
	inline const char* B_IRIDESCENT(int iter, bool post) {
		return Impl::AestheticText(post ? "''\u00B7.\u00B7" : "\u2022\u00B7.\u00B7''", "\u2022", "'\u00B7.\u00B7'", iter, post)->m_aData;
	}

	/* LINES */
	// Example: ━━━━━━
	inline const char* L_DEFAULT(int iter) {
		return Impl::AestheticText(nullptr, nullptr, "\u2501", iter, false)->m_aData;
	}
	// Example: ︵‿︵‿
	inline const char* L_WAVES(int iter, bool post) {
		return Impl::AestheticText(nullptr, nullptr, "\uFE35\u203F", iter, post)->m_aData;
	}

	/* WRAP LINES */
	// Example:  ────⇌ • • ⇋────
	inline std::string SWL_CONFIDENT(int iter) {
		return std::string(B_CONFIDENT(iter, false)) + std::string(B_CONFIDENT(iter, true));
	}
	// Example: ───※ ·· ※───
	inline std::string SWL_PILAR(int iter) {
		return std::string(B_PILLAR(iter, false)) + std::string(B_PILLAR(iter, true));
	}

	/* QUOTES */
	// Example: -ˏˋ Text here ˊˎ
	inline const char* Q_DEFAULT(bool post) {
		return Impl::AestheticText(post ? "\u02CA\u02CE" : "-\u02CF\u02CB", nullptr, nullptr, 0, post)->m_aData;
	}

	/* SYMBOL SMILIES */
	// Example: 『•✎•』
	inline const char* S_EDIT(const char* pBody) {
		return Impl::AestheticText(std::string("\u300E " + std::string(pBody) + " \u300F").c_str(), nullptr, nullptr, 0, false)->m_aData;
	}
	// Example: ᴄᴏᴍᴘʟᴇᴛᴇ!
	inline const char* S_COMPLETE() {
		return Impl::AestheticText("\u1D04\u1D0F\u1D0D\u1D18\u029F\u1D07\u1D1B\u1D07!", nullptr, nullptr, 0, false)->m_aData;
	}
}

// string utils
namespace Utils::String
{
	inline std::string progressBar(int max_value, int current_value, int step, const std::string& UTF_fill_symbol, const std::string& UTF_empty_symbol)
	{
		std::string ProgressBar;
		int numFilled = current_value / step;
		int numEmpty = max_value / step - numFilled;
		ProgressBar.reserve(numFilled + numEmpty);

		for(int i = 0; i < numFilled; i++)
			ProgressBar += UTF_fill_symbol;

		for(int i = 0; i < numEmpty; i++)
			ProgressBar += UTF_empty_symbol;

		return ProgressBar;
	}
}

// json utils
namespace Utils::Json
{
	inline void parseFromString(const std::string& Data, const std::function<void(nlohmann::json& pJson)>& pFuncCallback)
	{
		if(!Data.empty())
		{
			try
			{
				nlohmann::json JsonData = nlohmann::json::parse(Data);
				pFuncCallback(JsonData);
			}
			catch(nlohmann::json::exception& s)
			{
				dbg_assert(false, fmt("[json parse] Invalid json: {}", s.what()).c_str());
			}
		}
	}
}

// files utils
namespace Utils::Files
{
	enum Result : int
	{
		ERROR_FILE,
		SUCCESSFUL,
	};

	inline Result loadFile(const char* pFile, ByteArray* pData)
	{
		IOHANDLE File = io_open(pFile, IOFLAG_READ);
		if(!File)
			return ERROR_FILE;

		pData->resize((unsigned)io_length(File));
		io_read(File, pData->data(), (unsigned)pData->size());
		io_close(File);
		return SUCCESSFUL;
	}

	inline Result deleteFile(const char* pFile)
	{
		int Result = fs_remove(pFile);
		return Result == 0 ? SUCCESSFUL : ERROR_FILE;
	}

	inline Result saveFile(const char* pFile, const void* pData, unsigned size)
	{
		// delete old file
		deleteFile(pFile);

		IOHANDLE File = io_open(pFile, IOFLAG_WRITE);
		if(!File)
			return ERROR_FILE;

		io_write(File, pData, size);
		io_close(File);
		return SUCCESSFUL;
	}
}

// base class multiworld identifiable data
class _BaseMultiworldIdentifiableData
{
	inline static class IServer* m_pServer {};

public:
	IServer* Server() const { return m_pServer; }
	static void Init(IServer* pServer) { m_pServer = pServer; }
};

// class multiworld identifiable data
template < typename T >
class MultiworldIdentifiableData : public _BaseMultiworldIdentifiableData
{
protected:
	static inline T m_pData {};

public:
	static T& Data() { return m_pData; }
};

#endif
