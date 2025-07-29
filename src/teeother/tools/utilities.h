/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_TEMPLATE_UTILS_H
#define GAME_SERVER_CORE_TEMPLATE_UTILS_H

using ByteArray = std::basic_string<std::byte>;

/**
 * @brief Convert JSON value to BigInt.
 *
 * This function overload nlohmann json checks if the JSON value is either a string or a number.
 * If the value is a string, it directly converts it to BigInt.
 */

 // convert bigint <-> json
inline void from_json(const nlohmann::json& j, BigInt& value)
{
	if(j.is_string())
	{
		value = BigInt(j.get<std::string>());
	}
	else if(j.is_number())
	{
		value = BigInt(j.dump());
	}
	else
	{
		throw std::invalid_argument("Unsupported JSON type for BigInt");
	}
}
inline void to_json(nlohmann::json& j, const BigInt& value)
{
	j = value.to_string();
}

// convert vec2 <-> json
inline void from_json(const nlohmann::json& j, vec2& value)
{
	if(j.is_object())
	{
		if(j.contains("x") && j.contains("y"))
		{
			value.x = j.at("x").get<float>();
			value.y = j.at("y").get<float>();
		}
		else
		{
			throw std::invalid_argument("JSON object does not contain 'x' and 'y' fields");
		}
	}
	else
	{
		throw std::invalid_argument("Unsupported JSON type for vec2");
	}
}

inline void to_json(nlohmann::json& j, const vec2& value)
{
	j = nlohmann::json
	{
		{"x", value.x},
		{"y", value.y}
	};
}


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
		namespace optparse
		{
			template<typename T> inline std::optional<T> Value(const std::string& str) { return std::nullopt; };
			template<> inline std::optional<bool> Value<bool>(const std::string& str) { return str_toint(str.c_str()) > 0; }
			template<> inline std::optional<float> Value<float>(const std::string& str) { return str_tofloat(str.c_str()); }
			template<> inline std::optional<double> Value<double>(const std::string& str) { return str_tofloat(str.c_str()); }
			template<> inline std::optional<int> Value<int>(const std::string& str) { return str_toint(str.c_str()); }
			template<> inline std::optional<std::string> Value<std::string>(const std::string& str) { return str; }
			template<> inline std::optional<BigInt> Value<BigInt>(const std::string& str) { return BigInt(str); }
			template<> inline std::optional<vec2> Value<vec2>(const std::string& str)
			{
				const auto pos = str.find(' ');
				if(pos == std::string::npos)
				{
					const auto errorStr = "invalid vec2 parse format: " + str;
					dbg_assert(false, errorStr.c_str());
					return std::nullopt;
				}

				const std::string_view xStr = str.substr(0, pos);
				const std::string_view yStr = str.substr(pos + 1);
				const float x = str_tofloat(xStr.data());
				const float y = str_tofloat(yStr.data());
				return vec2(x, y);
			}
		}

		// сoncepts
		template<typename T>
		concept is_has_clear_function = requires(T & c)
		{
			{ c.clear() } -> std::same_as<void>;
		};

		template<typename T>
		concept is_smart_pointer = requires(T & c)
		{
			{ c.get() } -> std::convertible_to<typename T::element_type*>;
			{ c.reset() } noexcept -> std::same_as<void>;
		};

		template <typename T>
		concept is_container = requires(T & c)
		{
			typename T::value_type;
			typename T::iterator;
			{ c.begin() } -> std::convertible_to<typename T::iterator>;
			{ c.end() } -> std::convertible_to<typename T::iterator>;
		};

		template<typename T>
		concept is_map_container = requires(T & c)
		{
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

	template<typename... Args>
	bool loadSettings(const std::string& prefix, const std::vector<std::string>& lines, Args*... values)
	{
		size_t CurrentPos = 0;
		bool Success = true;

		// helper function to load a single value based on prefix
		auto loadSingleValue = [&CurrentPos, &lines](const std::string& fullPrefix) -> std::optional<std::string>
		{
			auto it = std::ranges::find_if(lines, [&CurrentPos, &fullPrefix](const std::string& s)
			{
				return s.starts_with(fullPrefix);
			});

			if(it != lines.end())
			{
				auto valueStr = it->substr(fullPrefix.size() + CurrentPos);

				// erase spaces
				size_t spacesRemoved = 0;
				valueStr.erase(valueStr.begin(), std::find_if_not(valueStr.begin(), valueStr.end(), [&](unsigned char c)
				{
					bool isSpace = std::isspace(c);
					if(isSpace) ++spacesRemoved;
					return isSpace;
				}));

				// update current position by spaces
				CurrentPos += spacesRemoved;

				// is by quotes parser
				if(valueStr.starts_with('"'))
				{
					const auto end = valueStr.find('\"', 1);
					if(end != std::string::npos)
					{
						CurrentPos += end + 1;
						return valueStr.substr(1, end - 1);
					}

					return std::nullopt;
				}

				// default parser
				const auto end = valueStr.find_first_of(" \t\n\r");
				if(end != std::string::npos)
				{
					CurrentPos += end;
					return valueStr.substr(0, end);
				}

				return valueStr;
			}

			return std::nullopt;
		};


		// lambda to load and parse a single value
		auto loadAndParse = [&](auto* value)
		{
			using ValueType = std::remove_pointer_t<decltype(value)>;
			const auto fullPrefix = prefix + " ";

			if(auto valueStrOpt = loadSingleValue(fullPrefix); valueStrOpt.has_value())
			{
				if(auto parsedValue = detail::optparse::Value<ValueType>(valueStrOpt.value()); parsedValue.has_value())
					*value = parsedValue.value();
				else
					Success = false;
			}
			else
				Success = false;
		};

		// use fold expression
		(loadAndParse(values), ...);
		return Success;
	}

	template <typename T> requires std::is_arithmetic_v<T>
	void process_bigint_in_chunks(BigInt total, std::invocable<T> auto processChunk)
	{
		constexpr T maxChunkSize = std::numeric_limits<T>::max();
		while(total > 0)
		{
			T currentChunk = maxChunkSize;
			if(total < maxChunkSize)
			{
				// always true
				currentChunk = total.to_int();
			}
			processChunk(currentChunk);
			total -= currentChunk;
		}
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
		inline std::string progressBar(uint64_t maxValue, uint64_t currentValue, int totalSteps, const std::string& FillSymbols, const std::string& EmptySymbols)
		{
			if(maxValue == 0)
				return "";

			currentValue = std::min(currentValue, maxValue);
			const auto progress = static_cast<double>(currentValue) / maxValue;
			const auto numFilled = static_cast<int>(progress * totalSteps);
			const auto numEmpty = totalSteps - numFilled;

			std::string resultStr;
			resultStr.reserve(numFilled * FillSymbols.length() + numEmpty * EmptySymbols.length());
			for(int i = 0; i < numFilled; ++i)
				resultStr += FillSymbols;

			for(int i = 0; i < numEmpty; ++i)
				resultStr += EmptySymbols;

			return resultStr;
		}

		inline std::string trim(std::string_view sv)
		{
			constexpr std::string_view whitespace = " \t\n\r\f\v";
			const auto start = sv.find_first_not_of(whitespace);
			if(start == std::string::npos)
				return "";
			const auto end = sv.find_last_not_of(whitespace);
			return std::string(sv.substr(start, end - start + 1));
		}


		inline std::pair<std::string, std::string> split_by_delimiter(std::string_view InputString, char Delimiter)
		{
			const auto DelimiterPos = InputString.find(Delimiter);
			if(DelimiterPos != std::string_view::npos)
				return { trim(InputString.substr(0, DelimiterPos)), trim(InputString.substr(DelimiterPos + 1)) };
			else
				return { trim(InputString), {} };
		}

		inline std::vector<std::string> split_lines(const std::string& input)
		{
			std::vector<std::string> lines;
			std::string_view sv(input);
			size_t current_pos = 0;
			size_t newline_pos;

			while((newline_pos = sv.find('\n', current_pos)) != std::string_view::npos)
			{
				lines.emplace_back(sv.substr(current_pos, newline_pos - current_pos));
				current_pos = newline_pos + 1;
			}

			if(current_pos < sv.length())
				lines.emplace_back(sv.substr(current_pos));

			return lines;
		}

		inline std::string escape(const std::string_view src)
		{
			std::string result;
			result.reserve(src.size());

			for(const auto& ch : src)
			{
				switch(ch)
				{
					case '\n': result += "\\n"; break;
					case '\t': result += "\\t"; break;
					case '\\': result += "\\\\"; break;
					case '"':  result += "\\\""; break;
					default:
						result += ch;
						break;
				}
			}

			return result;
		}

		inline std::string unescape(const std::string_view src)
		{
			std::string result;
			result.reserve(src.size());

			for(size_t i = 0; i < src.size(); ++i)
			{
				if(src[i] == '\\' && i + 1 < src.size())
				{
					switch(src[i + 1])
					{
						case 'n':  result += '\n'; ++i; break;
						case 't':  result += '\t'; ++i; break;
						case '\\': result += '\\'; ++i; break;
						case '"':  result += '"';  ++i; break;
						case 'u':
							if(i + 5 < src.size())
							{
								unsigned int unicodeChar = 0;
								auto [ptr, ec] = std::from_chars(&src[i + 2], &src[i + 6], unicodeChar, 16);

								if(ec == std::errc())
								{
									std::array<char, 5> utf8_buffer {};
									int utf8_len = str_utf8_encode(utf8_buffer.data(), unicodeChar);
									if(utf8_len > 0)
										result.append(utf8_buffer.data(), utf8_len);

									i += 5;
									break;
								}
							}
							else
							{
								result += '\\';
							}
							break;
					}
				}
				else
				{
					result += src[i];
				}
			}

			return result;
		}

		/* (c) Valentin Bashkirov (github.com/0xfaulty).*/
		inline void str_transliterate(char* pStr)
		{
			static const std::unordered_map<int, char> translit =
			{
				{0x0401, 'E'}, {0x0451, 'e'},
				{0x0410, 'A'}, {0x0430, 'a'},
				{0x0411, '6'}, {0x0431, '6'},
				{0x0412, 'B'},
				{0x0433, 'r'},
				{0x0414, 'D'}, {0x0434, 'g'},
				{0x0415, 'E'}, {0x0435, 'e'},
				{0x0417, '3'},
				{0x0438, 'u'},
				{0x0439, 'u'},
				{0x041A, 'K'},
				{0x041C, 'M'},
				{0x041D, 'H'},
				{0x041E, 'O'}, {0x043E, 'o'},
				{0x043F, 'n'},
				{0x0420, 'P'}, {0x0440, 'p'},
				{0x0421, 'C'}, {0x0441, 'c'},
				{0x0422, 'T'}, {0x0442, 'm'},
				{0x0443, 'y'},
				{0x0425, 'X'}, {0x0445, 'x'},
				{0x042C, 'b'},
			};

			char* pWrite = pStr;
			while(*pStr)
			{
				const char* pRead = pStr;
				int codePoint = str_utf8_decode(&pRead);
				if(auto it = translit.find(codePoint); it != translit.end())
					*pWrite++ = it->second;
				else
				{
					while(pStr != pRead)
						*pWrite++ = *pStr++;
				}
				pStr = const_cast<char*>(pRead);
			}
			*pWrite = '\0';
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
		inline bool parse(const std::string& Data, const std::function<void(nlohmann::json& pJson)>& pFuncCallback)
		{
			if(Data.empty())
				return true;

			if(!pFuncCallback)
				dbg_assert(pFuncCallback != nullptr, "[json parse] Callback function is null.");

			try
			{
				nlohmann::json JsonData = nlohmann::json::parse(Data);
				pFuncCallback(JsonData);
				return false;
			}
			catch(const nlohmann::json::parse_error& e)
			{
				dbg_msg("[json parse] Invalid json: %s", e.what());
			}
			catch(const nlohmann::json::exception& e)
			{
				dbg_assert(false, fmt_default("[json parse] JSON library exception: {}", e.what()).c_str());
			}
			catch(const std::exception& e)
			{
				dbg_assert(false, fmt_default("[json parse] Standard exception during processing: {}", e.what()).c_str());
			}
			catch(...)
			{
				dbg_assert(false, "[json parse] Unknown non-standard exception during processing.");
			}

			return true;
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
			ERROR_FILE = 0,
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
		inline std::string boardPillar(const std::string_view& text, int iter = 5)
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
		inline std::string boardConfident(const std::string_view& text, int iter = 5)
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
		inline std::string wrapLineConfident(int iter)
		{
			return boardConfident("", iter);
		}

		// Example: ───※ ·· ※───
		inline std::string wrapLinePillar(int iter)
		{
			return boardPillar("", iter);
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

				dbg_assert(false, fmt_default("Type mismatch for key: {}", key).c_str());
			}
			return defaultValue;
		}

		template<typename T>
		T& GetRefConfig(const std::string& key, const T& defaultValue)
		{
			auto& variant = m_umConfig[key];
			if(!std::holds_alternative<T>(variant))
			{
				dbg_assert(false, fmt_default("Type mismatch for key: {}\n", key).c_str());
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

	template <std::integral id_type>
	class string_mapper
	{
	public:
		[[nodiscard]] id_type string_to_id(const std::string& str)
		{
			auto it = m_vStrToIdsMap.find(str);
			if(it == m_vStrToIdsMap.end())
			{
				if(m_NextID >= std::numeric_limits<id_type>::max())
					m_NextID = 0;

				id_type new_id = m_NextID++;
				m_vStrToIdsMap[str] = new_id;
				m_vIdsToStrMap[new_id] = str;
				return new_id;
			}

			return it->second;
		}

		[[nodiscard]] std::optional<std::string> id_to_string(id_type id) const
		{
			auto it = m_vIdsToStrMap.find(id);
			if(it != m_vIdsToStrMap.end())
			{
				return it->second;
			}
			return std::nullopt;
		}

		[[nodiscard]] bool has_string(const std::string& str) const { return m_vStrToIdsMap.count(str) > 0; }
		[[nodiscard]] bool has_id(id_type id) const { return m_vIdsToStrMap.count(id) > 0; }
		[[nodiscard]] size_t count() const { return m_vStrToIdsMap.size(); }

		void clear()
		{
			m_vStrToIdsMap.clear();
			m_vIdsToStrMap.clear();
			m_NextID = 0;
		}

	private:
		std::map<std::string, id_type> m_vStrToIdsMap;
		std::map<id_type, std::string> m_vIdsToStrMap;
		id_type m_NextID = 0;
	};
}

#endif
