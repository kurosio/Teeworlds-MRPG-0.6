/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef BASE_MATH_H
#define BASE_MATH_H

#include <algorithm>
#include <cmath>
#include <cstdlib>

constexpr float pi = 3.1415926535897932384626433f;

template <typename T>
constexpr inline T clamp(T val, T min, T max)
{
	if(val < min)
		return min;
	if(val > max)
		return max;
	return val;
}

constexpr inline int round_to_int(float f)
{
	return f > 0 ? (int)(f + 0.5f) : (int)(f - 0.5f);
}

constexpr inline int round_truncate(float f)
{
	return (int)f;
}

template<typename T, typename TB>
constexpr inline T mix(const T a, const T b, TB amount)
{
	return a + (b - a) * amount;
}

template<typename T, typename TB>
inline T bezier(const T p0, const T p1, const T p2, const T p3, TB amount)
{
	// De-Casteljau Algorithm
	const T c10 = mix(p0, p1, amount);
	const T c11 = mix(p1, p2, amount);
	const T c12 = mix(p2, p3, amount);

	const T c20 = mix(c10, c11, amount);
	const T c21 = mix(c11, c12, amount);

	return mix(c20, c21, amount); // c30
}

inline float random_float()
{
	return rand() / (float)(RAND_MAX);
}

inline float random_float(float min, float max)
{
	return min + random_float() * (max - min);
}

inline float random_float(float max)
{
	return random_float(0.0f, max);
}

inline float centrelized_frandom(float center, float range) { return (center - range) + (random_float(range * 2.0f)); }

inline float random_angle()
{
	return 2.0f * pi * (rand() / std::nextafter((float)RAND_MAX, std::numeric_limits<float>::max()));
}

constexpr int fxpscale = 1 << 10;

// float to fixed
constexpr inline int f2fx(float v)
{
	return round_to_int(v * fxpscale);
}
constexpr inline float fx2f(int v)
{
	return v / (float)fxpscale;
}

// int to fixed
constexpr inline int i2fx(int v)
{
	return v * fxpscale;
}
constexpr inline int fx2i(int v)
{
	return v / fxpscale;
}

class fxp
{
	int value;

public:
	void set(int v)
	{
		value = v;
	}
	int get() const
	{
		return value;
	}
	fxp& operator=(int v)
	{
		value = i2fx(v);
		return *this;
	}
	fxp& operator=(float v)
	{
		value = f2fx(v);
		return *this;
	}
	operator int() const
	{
		return fx2i(value);
	}
	operator float() const
	{
		return fx2f(value);
	}
};

template<typename T>
constexpr inline T minimum(T a, T b)
{
	return std::min(a, b);
}
template<typename T, typename ... Ts>
constexpr inline T minimum(T a, Ts ... args)
{
	return minimum(a, minimum((args)...));
}
template<typename T>
constexpr inline T maximum(T a, T b)
{
	return std::max(a, b);
}
template<typename T, typename ... Ts>
constexpr inline T maximum(T a, Ts ... args)
{
	return maximum(a, maximum((args)...));
}
template<typename T>
constexpr inline T absolute(T a)
{
	return a < T(0) ? -a : a;
}

template<typename T>
constexpr inline T in_range(T a, T lower, T upper)
{
	return lower <= a && a <= upper;
}
template<typename T>
constexpr inline T in_range(T a, T upper)
{
	return in_range(a, 0, upper);
}

class Chance
{
	bool m_State;
	float m_Chance;

public:
	Chance(float chance) : m_Chance(chance) { Update(); }
	bool Update() { return m_State = random_float(100.0f) <= m_Chance; }
	bool operator()() const { return m_State; }
};


// Concept arithmetic type
template <typename T>
concept PercentCompatible = std::is_arithmetic_v<T> || std::is_same_v<T, BigInt>;

// derive from the number of percent e.g. ((100, 10%) = 10)
template <PercentCompatible T>
T translate_to_percent_rest(T value, float percent)
{
	if constexpr(std::is_same_v<T, BigInt>)
	{
		try
		{
			// try to convert BigInt to double and perform calculation
			const double value_double = std::stod(value.to_string());
			const double result = (value_double * percent) / 100.0;

			// convert result to string and find the decimal point
			std::string resultStr = std::to_string(result);
			if(const size_t pos = resultStr.find('.'); pos != std::string::npos)
			{
				return BigInt(resultStr.substr(0, pos));
			}

			dbg_msg("math", "no decimal point found in result string. BigInt(0).");
			return BigInt(0);
		}
		catch(...)
		{
			dbg_msg("math", "Error during BigInt to double conversion.");
			return BigInt(0);
		}
	}
	else
	{
		return static_cast<T>((static_cast<double>(value) * percent) / 100.0);
	}
}

// add to the number a percentage e.g. ((100, 10%) = 110)
template <PercentCompatible T>
T add_percent_to_source(T* pvalue, float percent)
{
	if(pvalue)
	{
		if constexpr(std::is_same_v<T, BigInt>)
		{
			BigInt result = translate_to_percent_rest(*pvalue, percent);
			*pvalue += result;
		}
		else
		{
			T result = translate_to_percent_rest(*pvalue, percent);
			*pvalue += result;
		}
	}
	return *pvalue;
}

// translate from the first to the second in percent e.g. ((10, 5) = 50%)
template <PercentCompatible T>
float translate_to_percent(T from, T value)
{
	constexpr double min_value = std::numeric_limits<double>::epsilon();

	if constexpr(std::is_same_v<T, BigInt>)
	{
		try
		{
			const double safe_from = maximum(min_value, std::stod(from.to_string()));
			const double safe_value = maximum(min_value, std::stod(value.to_string()));
			return (float)((safe_value * 100.0) / safe_from);
		}
		catch(...)
		{
			return std::numeric_limits<float>::quiet_NaN();
		}
	}
	else
	{
		const double safe_from = std::max(min_value, static_cast<double>(from));
		return (float)((value * 100.0) / safe_from);
	}
}

// translate from the first to the second in percent e.g. ((10, 5, 50) = 25%)
template <PercentCompatible T>
float translate_to_percent(T from, T value, float maximum_percent)
{
	constexpr double min_value = std::numeric_limits<double>::epsilon();

	if constexpr(std::is_same_v<T, BigInt>)
	{
		try
		{
			const double safe_from = maximum(min_value, std::stod(from.to_string()));
			const double safe_value = maximum(min_value, std::stod(value.to_string()));
			return (float)((safe_value * maximum_percent) / safe_from);
		}
		catch(...)
		{
			return std::numeric_limits<float>::quiet_NaN();
		}
	}
	else
	{
		double safe_from = std::max(min_value, static_cast<double>(from));
		return (float)((value * maximum_percent) / safe_from);
	}
}

constexpr inline uint64_t computeExperience(unsigned Level)
{
	if(Level == 1)
		return 18;
	return Level * (static_cast<unsigned long long>(Level) - 1) * 24;
}

constexpr inline uint64_t calculate_exp_gain(int factorCount, int baseLevel, int factorLevel)
{
	int levelDifference = baseLevel - factorLevel;
	double multiplier = (levelDifference >= 0)
		? 1.0 + (levelDifference * 0.05)
		: 1.0 / (1.0 + (std::abs(levelDifference) * 0.1));
	uint64_t baseExp = computeExperience(factorLevel) / factorCount;
	uint64_t experience = baseExp / multiplier;
	uint64_t minimumExperience = baseExp * 0.05;
	return maximum((uint64_t)1, experience, minimumExperience);
}


constexpr inline uint64_t calculate_gold_gain(int factorCount, int baseLevel, int factorLevel, bool randomBonus = false)
{
	int levelDifference = baseLevel - factorLevel;
	double multiplier = (levelDifference >= 0)
		? 1.0 + (levelDifference * 0.05)
		: 1.0 / (1.0 + (std::abs(levelDifference) * 0.1));
	unsigned long long baseGold = computeExperience(factorLevel) / factorCount;
	unsigned long long gold = baseGold / multiplier;
	unsigned long long minimumGold = baseGold * 0.05;

	if(randomBonus)
	{
		gold += rand() % (gold / 5 + 1);
		minimumGold += rand() % (minimumGold / 5 + 1);
	}

	return maximum((unsigned long long)1, gold, minimumGold);
}

#endif // BASE_MATH_H
