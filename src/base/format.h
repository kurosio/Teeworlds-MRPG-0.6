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

typedef std::string(HandlerFmtCallbackFunc)(int, const char*, void*);
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

// implement format
struct struct_format_implement
{
	struct description
	{
		int m_definer;
		bool m_handlefmt;
	};

	// argument conversion
	template<typename T>
	static std::string to_string(const description& desc, const T& value)
	{
		if constexpr(std::is_same_v<T, double> || std::is_same_v<T, float>)
		{
			return std::to_string(value);
		}
		else if constexpr(std::is_arithmetic_v<T>)
		{
			bool digitCommas = struct_handler_fmt::get_flags() & FMTFLAG_DIGIT_COMMAS;
			return digitCommas ? fmt_digit<std::string>(std::to_string(value)) : std::to_string(value);
		}
		else if constexpr(std::is_convertible_v<T, std::string>)
		{
			bool handleArgs = desc.m_handlefmt && struct_handler_fmt::get_flags() & FMTFLAG_HANDLE_ARGS;
			return handleArgs ? struct_handler_fmt::handle(desc.m_definer, std::string(value)) : std::string(value);
		}
		else
		{
			static_assert(!std::is_same_v<T, T>, "One of the passed arguments cannot be converted to a string");
			return "error convertible";
		}
	}

	// implementation for the last argument
	static std::string impl(const description& desc, const std::string& text, std::list<std::string>& vStrPack);

	// implementation for recursive arguments
	template<typename T, typename... Ts>
	static std::string impl(const description& desc, const std::string& text, std::list<std::string>& vStrPack, const T& arg, const Ts &...args)
	{
		vStrPack.emplace_back(to_string(desc, arg));
		return impl(desc, text, vStrPack, args...);
	}

	// implementation for default format
	template<typename... Ts>
	static std::string impl_fmt(const description& desc, const char* ptext, const Ts &...args)
	{
		std::list<std::string> vStringPack {};
		auto text = desc.m_handlefmt ? struct_handler_fmt::handle(desc.m_definer, ptext) : ptext;
		return struct_format_implement::impl(desc, text, vStringPack, args...);
	}

	static std::string impl_fmt(const description& desc, const char* ptext)
	{
		return desc.m_handlefmt ? struct_handler_fmt::handle(desc.m_definer, ptext) : ptext;
	}
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
void fmt_set_flags(int flags);


/**
 * Formats a string with the specified arguments.
 *
 * @tparam Ts The types of the arguments.
 * @param ptext The format string.
 * @param args The arguments to be formatted.
 * @return The formatted string.
 */
template <typename... Ts>
inline std::string fmt(const char* ptext, const Ts &...args)
{
	struct_format_implement::description desc { 0, false };
	return struct_format_implement::impl_fmt({}, ptext, args...);
}

/**
 * Formats a string with the specified arguments using a handler function.
 *
 * @tparam Ts The types of the arguments.
 * @param ptext The format string.
 * @param args The arguments to be formatted.
 * @return The formatted string.
 */
template <typename... Ts>
inline std::string fmt_handle(const char* ptext, const Ts &...args)
{
	struct_format_implement::description desc { 0, true };
	return struct_format_implement::impl_fmt(desc, ptext, args...);
}

/**
 * Formats a string with the specified arguments using a handler function.
 *
 * @tparam Ts The types of the arguments.
 * @param definer The definer value.
 * @param ptext The format string.
 * @param args The arguments to be formatted.
 * @return The formatted string.
 */
template<typename... Ts>
inline std::string fmt_handle_def(int definer, const char* ptext, const Ts &...args)
{
	struct_format_implement::description desc { definer, true };
	return struct_format_implement::impl_fmt(desc, ptext, args...);
}

#endif // _GAME_SERVER_TOOLS_FORMAT_H