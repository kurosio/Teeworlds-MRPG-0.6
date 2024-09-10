/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_TEMPLATE_UTILS_H
#define GAME_SERVER_CORE_TEMPLATE_UTILS_H

using ByteArray = std::basic_string<std::byte>;

/**
 * @namespace mystd
 * @brief A set of utilities for advanced work with containers, concepts and configuration.
 *
 * The `mystd` namespace contains functions and classes for working with containers,
 * smart pointers and configurations, as well as utilities for parsing strings and freeing resources.
 */
namespace mystd
{
	namespace detail
	{
		// specialization parsing value
		template<typename T>
		inline std::optional<T> ParseValue(const std::string& str) { return std::nullopt; };

		template<>
		inline std::optional<int> ParseValue<int>(const std::string& str) { return str_toint(str.c_str()); }

		template<>
		inline std::optional<std::string> ParseValue<std::string>(const std::string& str) { return str; }

		// сoncepts
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
	void freeContainer(T& container)
	{
		static_assert(detail::is_has_clear_function<T>, "One or more types do not have clear() function");

		if constexpr(detail::is_map_container<T>)
		{
			if constexpr(detail::is_container<typename T::mapped_type>)
				std::ranges::for_each(container, [](auto& element) { freeContainer(element.second); });
			else if constexpr(std::is_pointer_v<typename T::mapped_type> && !detail::is_smart_pointer<typename T::mapped_type>)
				std::ranges::for_each(container, [](auto& element) { delete element.second; });
		}
		else
		{
			if constexpr(detail::is_container<typename T::value_type>)
				std::ranges::for_each(container, [](auto& element) { freeContainer(element); });
			else if constexpr(std::is_pointer_v<typename T::value_type> && !detail::is_smart_pointer<typename T::value_type>)
				std::ranges::for_each(container, [](auto& element) { delete element; });
		}
		container.clear();
	}
	template<detail::is_container... Containers>
	void freeContainer(Containers&... args) { (freeContainer(args), ...); }

	// function for loading numeral configurations // TODO:ignore severity
	template<typename T1, typename T2 = int>
	inline std::optional<T1> loadSetting(const std::string& prefix, const std::vector<std::string>& settings, std::optional<T2> UniquePrefix = std::nullopt)
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


	/**
	 * @namespace string
	 * @brief Utilities for working with text strings.
	 *
	 * The `string` namespace contains functions for working with strings, which can be used
	 * for various purposes of formatting, displaying, or manipulating text.
	 */
	namespace string
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


	/**
	 * @namespace json
	 * @brief Utilities for working with JSON data using the nlohmann::json library.
	 *
	 * The `json` namespace provides functions to parse strings in JSON format
	 * and perform actions on the resulting data via callback functions.
	 */
	namespace json
	{
		inline void parse(const std::string& Data, const std::function<void(nlohmann::json& pJson)>& pFuncCallback)
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


	/**
	 * @namespace file
	 * @brief Utilities for working with the file system: reading, writing and deleting files.
	 *
	 * The `file` namespace contains functions for performing file operations,
	 * such as load, save, and delete. These functions return the result of the operation
	 * as a `Result` enumeration.
	 */
	namespace file
	{
		enum result : int
		{
			ERROR_FILE,
			SUCCESSFUL,
		};

		inline result load(const char* pFile, ByteArray* pData)
		{
			IOHANDLE File = io_open(pFile, IOFLAG_READ);
			if(!File)
				return ERROR_FILE;

			pData->resize((unsigned)io_length(File));
			io_read(File, pData->data(), (unsigned)pData->size());
			io_close(File);
			return SUCCESSFUL;
		}

		inline result remove(const char* pFile)
		{
			int Result = fs_remove(pFile);
			return Result == 0 ? SUCCESSFUL : ERROR_FILE;
		}

		inline result save(const char* pFile, const void* pData, unsigned size)
		{
			// delete old file
			remove(pFile);

			IOHANDLE File = io_open(pFile, IOFLAG_WRITE);
			if(!File)
				return ERROR_FILE;

			io_write(File, pData, size);
			io_close(File);
			return SUCCESSFUL;
		}
	}


	/**
	 * @namespace aesthetic
	 * @brief A set of utilities for creating aesthetically pleasing text strings with decorative borders and symbols.
	 *
	 * The `aesthetic` namespace provides functions for creating text elements,
	 * such as borders, symbols, lines, and quotes, which can be used for
	 * output design in applications.
	 */
	namespace aesthetic
	{
		// Example: ───※ ·· ※───
		template<int iter = 5>
		inline std::string boardPillar(const std::string_view& text)
		{
			std::string result;
			result.reserve(iter * 2 + text.size() + 10);

			for(int i = 0; i < iter; ++i)
				result += "\u2500";
			result += "\u203B \u00B7 ";
			result += text;
			result += " \u00B7 \u203B";
			for(int i = 0; i < iter; ++i)
				result += "\u2500";

			return std::move(result);
		}

		// Example: ✯¸.•*•✿✿•*•.¸✯
		inline std::string boardFlower(const std::string_view& text)
		{
			std::string result;
			result.reserve(text.size() + 10);

			result += "\u272F\u00B8.\u2022*\u2022\u273F";
			result += text;
			result += "\u273F\u2022*\u2022.\u00B8\u272F";

			return std::move(result);
		}

		// Example: ──⇌ • • ⇋──
		template<int iter = 5>
		inline std::string boardConfident(const std::string_view& text)
		{
			std::string result;
			result.reserve(iter * 2 + text.size() + 10);

			for(int i = 0; i < iter; ++i)
				result += "\u2500";
			result += " \u21CC \u2022 ";
			result += text;
			result += " \u2022 \u21CB ";
			for(int i = 0; i < iter; ++i)
				result += "\u2500";

			return std::move(result);
		}

		/* LINES */
		// Example: ︵‿︵‿
		template <int iter>
		inline std::string lineWaves()
		{
			std::string result;
			result.reserve(iter * 2);

			for(int i = 0; i < iter; ++i)
				result += "\uFE35\u203F";

			return result;
		}

		/* WRAP LINES */
		// Example:  ────⇌ • • ⇋────
		template <int iter>
		inline std::string wrapLineConfident()
		{
			return boardConfident<iter>("");
		}

		// Example: ───※ ·· ※───
		template <int iter>
		inline std::string wrapLinePillar()
		{
			return boardPillar<iter>("");
		}

		/* QUOTES */
		// Example: -ˏˋ Text here ˊˎ
		inline std::string quoteDefault(const std::string_view& text)
		{
			std::string result;
			result.reserve(text.size() + 10);

			result += "-\u02CF\u02CB";
			result += text;
			result += "\u02CA\u02CE";

			return result;
		}

		/* SYMBOL SMILIES */
		// Example: ᴄᴏᴍᴘʟᴇᴛᴇ!
		inline std::string symbolsComplete()
		{
			return "\u1D04\u1D0F\u1D0D\u1D18\u029F\u1D07\u1D1B\u1D07!";
		}
	}

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
}

#endif
