#ifndef BASE_FORMAT_H
#define BASE_FORMAT_H

/**
 * Formats a big digit value into a string with a specified separator and number of digits.
 *
 * @param numberStr The string value to format.
 * @return The formatted string.
 */
template <char separator = '.'>
std::string fmt_big_digit( std::string numberStr)
{
	// intiailize variables
	double number = strtod(numberStr.c_str(), nullptr);
	const std::vector<std::string> vSuffixes =
	{
		"", "k", "mil", "bil", "tri", "quad", "quint", "sext", "sept", "oct",
		"non", "dec", "undec", "duodec", "tredec", "quattuordec", "quindec",
		"sexdec", "septendec", "octodec", "novemdec", "vigint", "unvigint", "duovigint", "trevigint"
	};

	// check is less default number
	if(floor(number) == number && number < 1000)
		return std::to_string(static_cast<int>(number));

	// get suffix index
	size_t index = 0;
	while(number >= 1000 && index < vSuffixes.size() - 1)
	{
		number /= 1000;
		index++;
	}

	// format the number with two decimal places
	std::string numberResult = std::to_string(round(number * 100) / 100);
	if(auto pos = numberResult.find(separator); pos != std::string::npos)
	{
		numberResult.erase(numberResult.find_last_not_of('0') + 1, std::string::npos);
		if(numberResult.back() == separator)
			numberResult.pop_back();
	}

	// result
	return numberResult + vSuffixes[index];
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

// implement format
typedef std::string(HandlerFmtCallbackFunc)(int, const char*, void*);
typedef struct { HandlerFmtCallbackFunc* m_pCallback; void* m_pData; } HandlerFmtCallback;
struct struct_format_implement
{
	struct description
	{
		int m_definer;
		bool m_handlefmt;
	};
	enum { type_unknown, type_string, type_integers, type_big_integers, type_floating };

	/**
	 * Handles the formatting of a string using a callback function.
	 */
	struct handler_fmt
	{
		static void init(HandlerFmtCallbackFunc* pCallback, void* pData)
		{
			ms_pCallback.m_pCallback = pCallback;
			ms_pCallback.m_pData = pData;
		}
		static std::string handle(const description& Desc, const std::string& Text)
		{
			if(Desc.m_handlefmt && ms_pCallback.m_pCallback)
				return ms_pCallback.m_pCallback(Desc.m_definer, Text.c_str(), ms_pCallback.m_pData);
			return Text;
		}
		static void use_flags(int flags)
		{
			ms_Flags = flags;
		}
		static int get_flags()
		{
			return ms_Flags;
		}
	private:
		inline static HandlerFmtCallback ms_pCallback {};
		inline static int ms_Flags {};
	};

	// argument conversion
	template<typename T>
	static std::pair<int, std::string> to_string(const description& Desc, const T& Value)
	{
		if constexpr(std::is_same_v<T, double> || std::is_same_v<T, float>)
			return { type_floating, std::to_string(Value) };
		else if constexpr(std::is_same_v<T, BigInt>)
			return { type_big_integers, Value.to_string() };
		else if constexpr(std::is_arithmetic_v<T>)
			return { type_integers, std::to_string(Value) };
		else if constexpr(std::is_convertible_v<T, std::string>)
			return { type_string, std::string(Value) };
		else
		{
			static_assert(!std::is_same_v<T, T>, "One of the passed arguments cannot be converted to a string");
			return { type_unknown, "error convertible" };
		}
	}

	// implementation for the last argument
	static void prepare_result(const description& Desc, const std::string& Text, std::string* pResult, std::vector<std::pair<int, std::string>>& vPack);

	// implementation for default format
	template<typename... Ts>
	static std::string impl_format(const description& Desc, const char* pText, const Ts &...Args)
	{
		// check text pointer valid
		if(!pText)
			return "";

		// prepare text
		std::string Text = handler_fmt::handle(Desc, pText);

		// collect arguments converted to string
		std::vector<std::pair<int, std::string>> vPack;
		(vPack.emplace_back(to_string(Desc, Args)), ...);

		// prepare result
		std::string Result;
		Result.reserve(Text.size() + std::accumulate(vPack.begin(), vPack.end(), std::size_t { 0 }, [](std::size_t acc, const auto& s) { return acc + s.second.size(); }));
		prepare_result(Desc, Text, &Result, vPack);
		return Result;
	}

	static std::string impl_format(const description& Desc, const char* pText)
	{
		// check text pointer valid
		if(!pText)
			return "";

		return handler_fmt::handle(Desc, pText);
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
	return struct_format_implement::impl_format(desc, ptext, args...);
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
	return struct_format_implement::impl_format(desc, ptext, args...);
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
	return struct_format_implement::impl_format(desc, ptext, args...);
}

#endif // BASE_FORMAT_H