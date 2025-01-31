#ifndef BASE_FORMAT_H
#define BASE_FORMAT_H

template <char separator = '.'>
constexpr std::string fmt_big_digit(std::string_view numberStr)
{
	constexpr std::array<std::string_view, 25> vSuffixes =
	{
		"", "k", "m", "b", "t", "qa", "qi", "sx", "sp", "oc",
		"no", "dc", "ud", "dd", "td", "qad", "qid", "sxd", "spd",
		"ocd", "nod", "vg", "uvg", "dvg", "tvg"
	};

	size_t length = numberStr.size();
	size_t suffixIndex = std::clamp((length - 1) / 3, (size_t)0, vSuffixes.size() - 1);
	size_t significantDigitsCount = length % 3 == 0 ? 3 : length % 3;

	std::string result;
	result.reserve(5 + vSuffixes[suffixIndex].size());
	result.append(numberStr.substr(0, significantDigitsCount));

	if(length > 3)
	{
		result.push_back(separator);

		size_t decimalDigitsCount = std::min<size_t>(2, length - significantDigitsCount);
		auto decimals = numberStr.substr(significantDigitsCount, decimalDigitsCount);
		//auto filteredDecimals = decimals | std::views::reverse | std::views::drop_while([](char c) { return c == '0'; }) | std::views::reverse;

		result.append(decimals);
	}

	result.append(vSuffixes[suffixIndex]);
	return result;
}

template<typename T, const char separator = '.'>
constexpr std::string fmt_digit(T Value)
{
	// coverting the value to string
	std::string conversionString;
	constexpr unsigned num = 3;

	if constexpr(std::is_same_v<std::string, T>)
	{
		conversionString = Value;
	}
	else if constexpr(std::is_arithmetic_v<T>)
	{
		conversionString = std::to_string(Value);
	}
	else
	{
		conversionString(Value);
	}

	// prepare digit
	if(conversionString.length() > (num + 1))
	{
		for(int i = (int)conversionString.length() - num; i > 0; i -= num)
		{
			conversionString.insert(i, 1, separator);
		}
	}

	return conversionString;
}

enum
{
	FMTFLAG_HANDLE_ARGS = 1 << 0,
};

// implement format
typedef std::string(HandlerFmtCallbackFunc)(int, const char*, void*);
typedef struct
{
	HandlerFmtCallbackFunc* m_pCallback;
	void* m_pData;
} HandlerFmtCallback;

class CFormatter
{
	enum
	{
		type_unknown,
		type_string,
		type_integers,
		type_big_integers,
		type_floating
	};

	HandlerFmtCallback m_pCallback {};
	int m_Definer {};
	int m_Flags {};

	std::string handle(const std::string& Text) const
	{
		if(m_pCallback.m_pCallback)
		{
			return m_pCallback.m_pCallback(m_Definer, Text.c_str(), m_pCallback.m_pData);
		}
		return Text;
	}

	template<typename T>
	std::pair<int, std::string> to_string(const T& Value)
	{
		if constexpr(std::is_same_v<T, BigInt>)
		{
			return { type_big_integers, Value.to_string() };
		}
		else if constexpr(std::is_integral_v<T>)
		{
			return { type_integers, std::to_string(Value) };
		}
		else if constexpr(std::is_floating_point_v<T>)
		{
			return { type_floating, std::to_string(Value) };
		}
		else if constexpr(std::is_convertible_v<T, std::string>)
		{
			return { type_string, std::string(Value) };
		}
		else if constexpr(std::is_same_v<T, std::string_view>)
		{
			return { type_string, Value.data() };
		}
		else
		{
			static_assert(!std::is_same_v<T, T>, "One of the passed arguments cannot be converted to a string");
			return { type_unknown, "error convertible" };
		}
	}
	void prepare_result(const std::string& Text, std::string* pResult, std::vector<std::pair<int, std::string>>& vPack) const;

public:
	void init(HandlerFmtCallbackFunc* pCallback, void* pData)
	{
		m_pCallback.m_pCallback = pCallback;
		m_pCallback.m_pData = pData;
	}

	void set_definer(int definer)
	{
		m_Definer = definer;
	}

	void use_flags(int flags)
	{
		m_Flags = flags;
	}

	int get_flags() const
	{
		return m_Flags;
	}

	// implementation for default format
	template<typename... Ts>
	std::string operator()(const char* pText, const Ts &...Args)
	{
		// check text pointer valid
		if(!pText)
			return "";

		// prepare text
		std::string Text = handle(pText);

		// collect arguments converted to string
		std::vector<std::pair<int, std::string>> vPack;
		(vPack.emplace_back(to_string(Args)), ...);

		// prepare result
		std::string Result;
		Result.reserve(Text.size() + std::accumulate(vPack.begin(), vPack.end(), std::size_t { 0 }, [](std::size_t acc, const auto& s)
		{
			return acc + s.second.size();
		}));
		prepare_result(Text, &Result, vPack);
		m_Definer = {};
		return Result;
	}

	std::string operator()(const char* pText)
	{
		// check text pointer valid
		if(!pText)
			return "";

		std::string Result = std::move(handle(pText));
		m_Definer = {};
		return std::move(Result);
	}
};

inline CFormatter g_fmt_default {};
inline CFormatter g_fmt_localize {};

template <typename ... Ts>
std::string fmt_default(const char* pText, const Ts& ... args)
{
	return g_fmt_default(pText, args...);
}

template <typename ... Ts>
std::string fmt_localize(int ClientID, const char* pText, const Ts& ... args)
{
	g_fmt_localize.set_definer(ClientID);
	return g_fmt_localize(pText, args...);
}

#endif // BASE_FORMAT_H