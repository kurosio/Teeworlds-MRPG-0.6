/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_TEMPLATE_UTILS_H
#define GAME_SERVER_CORE_TEMPLATE_UTILS_H

using ByteArray = std::basic_string<std::byte>;

namespace mrpgstd
{
	namespace detail
	{
		// specialization pasrsing value
		template<typename T>
		inline std::optional<T> ParseValue(const std::string& str) { return std::nullopt; };

		template<>
		inline std::optional<int> ParseValue<int>(const std::string& str) { return str_toint(str.c_str()); }

		template<>
		inline std::optional<std::string> ParseValue<std::string>(const std::string& str) { return str; }

		// сconcepts
		template<typename T>
		concept is_has_clear_function = requires(T & c) { { c.clear() } -> std::same_as<void>; };

		template<typename T>
		concept is_smart_pointer = requires(T & c) {
			{ c.get() } -> std::convertible_to<typename T::element_type*>;
			{ c.reset() } noexcept -> std::same_as<void>;
		};

		template <typename T>
		concept is_container = requires(T & c) {
			typename T::value_type;
			typename T::iterator;
			{ c.begin() } -> std::convertible_to<typename T::iterator>;
			{ c.end() } -> std::convertible_to<typename T::iterator>;
		};

		template<typename T>
		concept is_map_container = requires(T & c) {
			typename T::key_type;
			typename T::mapped_type;
				requires is_container<T>;
				requires std::same_as<typename T::value_type, std::pair<const typename T::key_type, typename T::mapped_type>>;
		};
	}

	// function to clear a container
	template<detail::is_container T>
	void free_container(T& container)
	{
		static_assert(detail::is_has_clear_function<T>, "One or more types do not have clear() function");

		if constexpr(detail::is_map_container<T>)
		{
			if constexpr(detail::is_container<typename T::mapped_type>)
				std::ranges::for_each(container, [](auto& element) { free_container(element.second); });
			else if constexpr(std::is_pointer_v<typename T::mapped_type> && !detail::is_smart_pointer<typename T::mapped_type>)
				std::ranges::for_each(container, [](auto& element) { delete element.second; });
		}
		else
		{
			if constexpr(detail::is_container<typename T::value_type>)
				std::ranges::for_each(container, [](auto& element) { free_container(element); });
			else if constexpr(std::is_pointer_v<typename T::value_type> && !detail::is_smart_pointer<typename T::value_type>)
				std::ranges::for_each(container, [](auto& element) { delete element; });
		}
		container.clear();
	}
	template<detail::is_container... Containers>
	void free_container(Containers&... args) { (free_container(args), ...); }

	// configurable class
	class CConfigurable
	{
		using ConfigVariant = std::variant<int, float, vec2, std::string, std::vector<vec2>>;
		ska::flat_hash_map<std::string, ConfigVariant> m_umConfig;

	public:
		void SetConfig(const std::string& key, const ConfigVariant& value) { m_umConfig[key] = value; }
		bool HasConfig(const std::string& key) const { return m_umConfig.find(key) != m_umConfig.end(); }

		template<typename T>
		T GetConfig(const std::string& key, const T& defaultValue) const
		{
			if(const auto it = m_umConfig.find(key); it != m_umConfig.end())
			{
				if(std::holds_alternative<T>(it->second))
					return std::get<T>(it->second);

				dbg_assert(false, fmt("Type mismatch for key: {}", key).c_str());
			}
			return defaultValue;
		}

		template<typename T>
		T& GetRefConfig(const std::string& key, const T& defaultValue)
		{
			auto& variant = m_umConfig[key];
			if(!std::holds_alternative<T>(variant))
			{
				dbg_assert(false, fmt("Type mismatch for key: {}\n", key).c_str());
				variant = defaultValue;
			}

			return std::get<T>(variant);
		}

		template<typename T>
		bool TryGetConfig(const std::string& key, T& outValue) const
		{
			if(const auto it = m_umConfig.find(key); it != m_umConfig.end())
			{
				if(std::holds_alternative<T>(it->second))
				{
					outValue = std::get<T>(it->second);
					return true;
				}
			}
			return false;
		}

		void RemoveConfig(const std::string& key)
		{
			m_umConfig.erase(key);
		}
	};

	// function for loading numeral configurations // TODO:ignore severity
	template<typename T1, typename T2 = int>
	inline std::optional<T1> LoadSetting(const std::string& prefix, const std::vector<std::string>& settings, std::optional<T2> UniquePrefix = std::nullopt)
	{
		std::string fullPrefix = prefix + " ";
		if(UniquePrefix.has_value())
		{
			if constexpr(std::is_same_v<T2, std::string>)
				fullPrefix += UniquePrefix.value() + " ";
			else
				fullPrefix += std::to_string(UniquePrefix.value()) + " ";
		}

		auto it = std::ranges::find_if(settings, [&fullPrefix](const std::string& s)
		{
			return s.starts_with(fullPrefix);
		});

		if(it != settings.end())
		{
			std::string valueStr = it->substr(fullPrefix.size());
			if(!valueStr.empty())
				return detail::ParseValue<T1>(valueStr);
		}

		return std::nullopt;
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

// aesthetic utils (TODO: remove)
namespace Utils::Aesthetic
{
	/* BORDURES */
	// Example: ───※ ·· ※───
	inline std::string B_PILLAR(int iter, bool right)
	{
		std::string result {};
		if(right) { result += "\u203B \u00B7"; }
		for(int i = 0; i < iter; i++) { result += "\u2500"; }
		if(!right) { result += "\u00B7 \u203B"; }
		return std::move(result);
	}

	// Example: ┏━━━━━ ━━━━━┓
	inline std::string B_DEFAULT_TOP(int iter, bool right)
	{
		std::string result;
		if(!right) { result += "\u250F"; }
		for(int i = 0; i < iter; i++) { result += "\u2501"; }
		if(right) { result += "\u2513"; }
		return std::move(result);
	}

	// Example: ✯¸.•*•✿✿•*•.¸✯
	inline std::string B_FLOWER(bool right)
	{
		return right ? "\u273F\u2022*\u2022.\u00B8\u272F" : "\u272F\u00B8.\u2022*\u2022\u273F";
	}

	// Example: ──⇌ • • ⇋──
	inline std::string B_CONFIDENT(int iter, bool right)
	{
		std::string result;
		if(right) { result += " \u21CC \u2022"; }
		for(int i = 0; i < iter; i++) { result += "\u2500"; }
		if(!right) { result += "\u2022 \u21CB"; }
		return std::move(result);
	}

	/* LINES */
	// Example: ━━━━━━
	inline std::string L_DEFAULT(int iter)
	{
		std::string result;
		for(int i = 0; i < iter; i++) { result += "\u2501"; }
		return std::move(result);
	}
	// Example: ︵‿︵‿
	inline std::string L_WAVES(int iter, bool post)
	{
		std::string result;
		for(int i = 0; i < iter; i++) { result += "\uFE35\u203F"; }
		return std::move(result);
	}

	/* WRAP LINES */
	// Example:  ────⇌ • • ⇋────
	inline std::string SWL_CONFIDENT(int iter)
	{
		return B_CONFIDENT(iter, false) + B_CONFIDENT(iter, true);
	}
	// Example: ───※ ·· ※───
	inline std::string SWL_PILAR(int iter)
	{
		return B_PILLAR(iter, false) + B_PILLAR(iter, true);
	}

	/* QUOTES */
	// Example: -ˏˋ Text here ˊˎ
	inline std::string Q_DEFAULT(bool right)
	{
		return right ? "\u02CA\u02CE" : "-\u02CF\u02CB";
	}

	/* SYMBOL SMILIES */
	// Example: 『•✎•』
	inline std::string S_EDIT(const char* pBody)
	{
		return std::string("\u300E " + std::string(pBody) + " \u300F");
	}
	// Example: ᴄᴏᴍᴘʟᴇᴛᴇ!
	inline std::string S_COMPLETE()
	{
		return "\u1D04\u1D0F\u1D0D\u1D18\u029F\u1D07\u1D1B\u1D07!";
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
