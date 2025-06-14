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

template<typename T>
constexpr T squared(T value)
{
	return value * value;
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
		constexpr long long scale = 10000;
		const auto scaled_percent = static_cast<long long>(percent * scale);
		return (value * scaled_percent) / (100 * scale);
	}
	else
	{
		return static_cast<T>((static_cast<double>(value) * percent) / 100.0);
	}
}

// add to the number a percentage e.g. ((100, 10%) = 110)
template <PercentCompatible T>
void add_percent_to_source(T& value, float percent)
{
	value += translate_to_percent_rest(value, percent);
}

// translate from the first to the second in percent e.g. ((10, 5) = 50%)
template <PercentCompatible T>
float translate_to_percent(T from, T value, float maximum_percent)
{
	if constexpr(std::is_same_v<T, BigInt>)
	{
		if(from == BigInt(0))
			return std::numeric_limits<float>::quiet_NaN();

		constexpr long long scale = 1000000;
		const auto scaledMaxPercent = static_cast<long long>(maximum_percent * scale);
		BigInt tempResult = (value * scaledMaxPercent) / from;
		return static_cast<float>(tempResult.to_long_long()) / static_cast<float>(scale);
	}
	else
	{
		constexpr double minValue = std::numeric_limits<double>::epsilon();
		const double safeFrom = std::max(minValue, static_cast<double>(from));
		const double result = (static_cast<double>(value) * static_cast<double>(maximum_percent)) / safeFrom;
		return static_cast<float>(result);
	}
}

// translate from the first to the second in percent e.g. ((10, 5, 50) = 25%)
template <PercentCompatible T>
float translate_to_percent(T from, T value)
{
	return translate_to_percent(from, value, 100.0f);
}

constexpr inline uint64_t computeExperience(unsigned Level)
{
	if(Level == 1)
		return 18;
	return Level * (static_cast<unsigned long long>(Level) - 1) * 24;
}

constexpr inline uint64_t calculate_exp_gain(int playerLevel, int factorLevel)
{
	double multiplier = (playerLevel < factorLevel) ? 1.0 / (1.0 + ((factorLevel - playerLevel) * 0.05)) : 1.0;
	uint64_t baseExp = factorLevel;
	uint64_t experience = baseExp / multiplier;
	uint64_t minimumExperience = baseExp * 0.05;
	return maximum((uint64_t)1, experience, minimumExperience);
}

constexpr inline uint64_t calculate_loot_gain(int factorLevel, int delimiter)
{
	uint64_t value = (factorLevel / delimiter);
	return maximum((uint64_t)1, value);
}

#endif // BASE_MATH_H
