/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef BASE_MATH_H
#define BASE_MATH_H

#include <math.h>
#include <stdlib.h>
#include <type_traits> // cxx inc

template <typename T>
T clamp(T val, T min, T max)
{
	if(val < min)
		return min;
	if(val > max)
		return max;
	return val;
}

inline float sign(float f)
{
	return f<0.0f?-1.0f:1.0f;
}

inline int round_to_int(float f)
{
	if(f > 0)
		return (int)(f+0.5f);
	return (int)(f-0.5f);
}

template<typename T, typename TB>
T mix(const T a, const T b, TB amount)
{
	return a + (b-a)*amount;
}

template<typename T, typename TB>
T bezier(const T p0, const T p1, const T p2, const T p3, TB amount)
{
	// De-Casteljau Algorithm
	const T c10 = mix(p0, p1, amount);
	const T c11 = mix(p1, p2, amount);
	const T c12 = mix(p2, p3, amount);

	const T c20 = mix(c10, c11, amount);
	const T c21 = mix(c11, c12, amount);

	return mix(c20, c21, amount); // c30
}

inline int random_int() { return (((rand() & 0xffff) << 16) | (rand() & 0xffff)) & 0x7FFFFFFF; };
inline int random_num(int min, int max) { return (random_int() % (max - min + 1)) + min; }
inline float frandom() { return rand()/(float)(RAND_MAX); }
inline float frandom_num(float min, float max) { return (frandom() * (max - min)) + min; }
inline float centrelized_frandom(float center, float range) { return (center - range) + (frandom() * (range * 2.0f)); }

// float to fixed
inline int f2fx(float v) { return (int)(v*(float)(1<<10)); }
inline float fx2f(int v) { return v*(1.0f/(1<<10)); }

// int to fixed
inline int i2fx(int v) { return v<<10; }
inline int fx2i(int v) { return v>>10; }

inline int gcd(int a, int b)
{
	while(b != 0)
	{
		int c = a % b;
		a = b;
		b = c;
	}
	return a;
}

class fxp
{
	int value;
public:
	void set(int v) { value = v; }
	int get() const { return value; }
	fxp &operator = (int v) { value = v<<10; return *this; }
	fxp &operator = (float v) { value = (int)(v*(float)(1<<10)); return *this; }
	operator float() const { return value/(float)(1<<10); }
};

const float pi = 3.1415926535897932384626433f;

// ODR (one-definition rule) states that there must be exactly one definition of a variable, function, class, enum or template
template <typename T> T min(T a, T b) { return a<b?a:b; }
template <typename T> T max(T a, T b) { return a>b?a:b; }
template <typename T> T max(T a, T b, T c) { return max(max(a, b), c); }
template <typename T> T absolute(T a) { return a<T(0)?-a:a; }

inline unsigned long long computeExperience(unsigned Level)
{
	if(Level == 1)
		return 18;
	return (unsigned long long)(24 * pow(Level, 2)) - ((unsigned long long)24 * Level);
}

// percents
template < typename T> // char is arithmetic type we must exclude it 'a' / 'd' etc
using PercentArithmetic = typename std::enable_if < std::is_arithmetic  < T >::value && !std::is_same < T, char >::value, T >::type;

template <typename T> // derive from the number of percent e.g. ((100, 10%) = 10)
PercentArithmetic<T> translate_to_percent_rest(T value, float percent) { return (T)(((double)value / 100.0f) * (T)percent); }

template <typename T> // add to the number a percentage e.g. ((100, 10%) = 110)
PercentArithmetic<T> add_percent_to_source(T *pvalue, float percent)
{
	*pvalue = ((T)((double)*pvalue) * (1.0f + ((T)percent / 100.0f)));
	return (T)(*pvalue);
}

template <typename T> // translate from the first to the second in percent e.g. ((10, 5) = 50%)
PercentArithmetic<T> translate_to_percent(T from, T value) { return (T)(((double)value / (double)from) * 100.0f); }

#endif // BASE_MATH_H
