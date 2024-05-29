#ifndef BASE_FORMAT_H
#define BASE_FORMAT_H

/**
 * Formats a big digit value into a string with a specified separator.
 *
 * @tparam T The type of the value to format.
 * @param Value The value to format.
 * @return The formatted string.
 */
template<typename T, const char separator = '.'>
inline std::string fmt_big_digit(T Value)
{
	constexpr unsigned num = 3;

	// coverting the value to string
	std::string conversionString;
	if constexpr(std::is_same_v<std::string, T>)
		conversionString = Value;
	else if constexpr(std::is_arithmetic_v<T>)
		conversionString = std::to_string(Value);
	else
		conversionString(Value);

	const char* pLabel[24] = { "", "million", "billion", "trillion", "quadrillion", "quintillion", "sextillion",
		"septillion", "octillion", "nonillion", "decillion", "undecillion",
		"duodecillion", "tredecillion", "quattuordecillion", "quindecillion",
		"sexdecillion", "septendecillion", "octodecillion", "novemdecillion",
		"vigintillion", "unvigintillion", "duovigintillion", "trevigintillion"
	};

	// prepare big digit
	if(conversionString.length() > (num + 1))
	{
		int Position = -1;
		auto iter = conversionString.end();

		for(auto it = conversionString.rbegin(); (num + 1) <= std::distance(it, conversionString.rend());)
		{
			if(iter != conversionString.end())
			{
				conversionString.erase(iter, conversionString.end());
			}
			std::advance(it, num);
			iter = conversionString.insert(it.base(), separator);
			Position++;
		}

		if(Position > 0 && Position < 24)
		{
			conversionString.append(pLabel[Position]);
		}
	}

	return conversionString;
}

/**
 * Formats a digit value into a string with a specified separator and number of digits.
 *
 * @tparam T The type of the value to format.
 * @param Value The value to format.
 * @return The formatted string.
 */
template<typename T, const char separator = '.', const unsigned num = 3>
inline std::string fmt_digit(T Value)
{
	// coverting the value to string
	std::string conversionString;
	if constexpr(std::is_same_v<std::string, T>)
		conversionString = Value;
	else if constexpr(std::is_arithmetic_v<T>)
		conversionString = std::to_string(Value);
	else
		conversionString(Value);

	// prepare digit
	if(conversionString.length() > (num + 1))
	{
		for(auto it = conversionString.rbegin(); (num + 1) <= std::distance(it, conversionString.rend());)
		{
			std::advance(it, num);
			conversionString.insert(it.base(), separator);
		}
	}

	return conversionString;
}

enum
{
	FMTFLAG_HANDLE_ARGS = 1 << 0,
	FMTFLAG_DIGIT_COMMAS = 1 << 1,
};


typedef std::string (HandlerFmtCallbackFunc)(int, const char*, void*);
typedef struct { HandlerFmtCallbackFunc* m_pCallback; void* m_pData; } HandlerFmtCallback;
struct struct_handler_fmt
{
	static void init(HandlerFmtCallbackFunc* pCallback, void* pData)
	{
		ms_pCallback.m_pCallback = pCallback;
		ms_pCallback.m_pData = pData;
	}
	static std::string handle(const int Definerlang, const std::string& Text)
	{
		return ms_pCallback.m_pCallback ? ms_pCallback.m_pCallback(Definerlang, Text.c_str(), ms_pCallback.m_pData) : Text;
	}
	static void use_flags(int flags) { ms_Flags = flags; }
	static int get_flags() { return ms_Flags; }
private:
	inline static HandlerFmtCallback ms_pCallback {};
	inline static int ms_Flags {};
};

/**
 * Initializes the handler function callback.
 *
 * @param pCallback A pointer to the handler callback function.
 * @param pData A pointer to the data associated with the handler callback function.
 *
 * Usage example:
 * Callback function
 */
void fmt_init_handler_func(HandlerFmtCallbackFunc* pCallback, void* pData);

/**
 * Sets the flags to be used in the formatting process.
 *
 * @param flags The flags to be used.
 */
void fmt_use_flags(int flags);

struct struct_format_implement
{
	// argument conversion
	template<typename T>
	static std::string to_string(int Definerlang, const T& Value)
	{
		if constexpr(std::is_same_v<T, double> || std::is_same_v<T, float>)
		{
			return std::to_string(Value);
		}
		else if constexpr(std::is_arithmetic_v<T>)
		{
			bool digitCommas = struct_handler_fmt::get_flags() & FMTFLAG_DIGIT_COMMAS;
			return digitCommas ? fmt_digit<std::string>(std::to_string(Value)) : std::to_string(Value);
		}
		else if constexpr(std::is_same_v<T, BigInt>)
		{
			bool digitCommas = struct_handler_fmt::get_flags() & FMTFLAG_DIGIT_COMMAS;
			return digitCommas ? fmt_big_digit(Value.to_string()) : Value.to_string();
		}
		else if constexpr(std::is_convertible_v<T, std::string>)
		{
			bool handleArgs = struct_handler_fmt::get_flags() & FMTFLAG_HANDLE_ARGS;
			return handleArgs ? struct_handler_fmt::handle(Definerlang, std::string(Value)) : std::string(Value);
		}
		else
		{
			static_assert(!std::is_same_v<T, T>, "One of the passed arguments cannot be converted to a string");
			return "error convertible";
		}
	}

	// implementation for the last argument
	static std::string impl(int Definer, const std::string& Text, std::list<std::string>& vStrPack);

	// implementation for recursive arguments
	template<typename T, typename... Ts>
	static std::string impl(int Definer, const std::string& Text, std::list<std::string>& vStrPack, const T& Arg, const Ts &...Argpack)
	{
		vStrPack.emplace_back(to_string(Definer, Arg));
		return impl(Definer, Text, vStrPack, Argpack...);
	}
};

/**
* Formats a string with the specified arguments.
*
* @tparam Ts The types of the arguments.
* @param pText The format string.
* @param ArgPack The arguments to format.
* @return The formatted string.
*/
template<typename... Ts>
inline std::string fmt(const char* pText, const Ts &...ArgPack)
{
	std::list<std::string> vStringPack {};
	return struct_format_implement::impl(-1, pText, vStringPack, ArgPack...);
}

/**
* Formats a string without any arguments.
*
* @param pText The format string.
* @return The formatted string.
*/
inline std::string fmt(const char* pText)
{
	return pText;
}

/**
* Formats a localized string with the specified arguments.
*
* @tparam Ts The types of the arguments.
* @param Definer The value, flag or hash.
* @param pText The format string.
* @param Argpack The arguments to format.
* @return The formatted localized string.
*/
template<typename... Ts>
inline std::string fmt_handle(int Definer, const char* pText, const Ts &...Argpack)
{
	std::list<std::string> vStringPack {};
	return struct_format_implement::impl(Definer, struct_handler_fmt::handle(Definer, pText), vStringPack, Argpack...);
}

/**
* Formats a by handle callback string without any arguments.
*
* @param Definer The value, flag or hash.
* @param pText The format string.
* @return The formatted localized string.
*/
inline std::string fmt_handle(int Definer, const char* pText)
{
	return struct_handler_fmt::handle(Definer, pText);
}

#endif // _GAME_SERVER_TOOLS_FORMAT_H