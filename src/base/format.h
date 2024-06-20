#ifndef BASE_FORMAT_H
#define BASE_FORMAT_H

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
};

// implement format
typedef std::string(HandlerFmtCallbackFunc)(int, const char*, void*);
typedef struct { HandlerFmtCallbackFunc* m_pCallback; void* m_pData; } HandlerFmtCallback;
class CFormatter
{
	enum { type_unknown, type_string, type_integers, type_big_integers, type_floating };
	struct Config { int m_Definer; };
	HandlerFmtCallback m_pCallback {};
	Config m_Config {};
	int m_Flags {};

	std::string handle(const std::string& Text) const
	{
		if(m_pCallback.m_pCallback)
			return m_pCallback.m_pCallback(m_Config.m_Definer, Text.c_str(), m_pCallback.m_pData);
		return Text;
	}

	template<typename T>
	std::pair<int, std::string> to_string(const T& Value)
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
	void prepare_result(const std::string& Text, std::string* pResult, std::vector<std::pair<int, std::string>>& vPack) const;

public:
	void init(HandlerFmtCallbackFunc* pCallback, void* pData)
	{
		m_pCallback.m_pCallback = pCallback;
		m_pCallback.m_pData = pData;
	}

	void set_definer(int definer) { m_Config.m_Definer = definer; }
	void use_flags(int flags) { m_Flags = flags; }
	int get_flags() const { return m_Flags; }

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
		Result.reserve(Text.size() + std::accumulate(vPack.begin(), vPack.end(), std::size_t { 0 }, [](std::size_t acc, const auto& s) { return acc + s.second.size(); }));
		prepare_result(Text, &Result, vPack);
		m_Config.m_Definer = {};
		return Result;
	}

	std::string operator()(const char* pText)
	{
		// check text pointer valid
		if(!pText)
			return "";

		std::string Result = std::move(handle(pText));
		m_Config.m_Definer = {};
		return std::move(Result);
	}
};

inline CFormatter g_fmt_default {};
inline CFormatter g_fmt_localize {};

template <typename ... Ts>
std::string fmt(const char* pText, const Ts& ... args)
{
	return g_fmt_default(pText, args...);
}

template <typename ... Ts>
std::string fmt_self(int flags, const char* pText, const Ts& ... args)
{
	CFormatter fmt;
	fmt.use_flags(flags);
	return fmt(pText, args...);
}

template <typename ... Ts>
std::string fmt_localize(int ClientID, const char* pText, const Ts& ... args)
{
	g_fmt_localize.set_definer(ClientID);
	return g_fmt_localize(pText, args...);
}

#endif // BASE_FORMAT_H