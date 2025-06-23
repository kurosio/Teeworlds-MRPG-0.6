/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef BASE_VMATH_H
#define BASE_VMATH_H

#include <cmath>
#include <cstdint>

#include "math.h"

// ------------------------------------

template<typename T>
class vector2_base
{
public:
	union
	{
		T x, u;
	};
	union
	{
		T y, v;
	};

	constexpr vector2_base() = default;
	constexpr vector2_base(T nx, T ny) :
		x(nx), y(ny)
	{
	}

	vector2_base operator-() const { return vector2_base(-x, -y); }
	vector2_base operator-(const vector2_base& vec) const { return vector2_base(x - vec.x, y - vec.y); }
	vector2_base operator+(const vector2_base& vec) const { return vector2_base(x + vec.x, y + vec.y); }
	vector2_base operator*(const T rhs) const { return vector2_base(x * rhs, y * rhs); }
	vector2_base operator*(const vector2_base& vec) const { return vector2_base(x * vec.x, y * vec.y); }
	vector2_base operator/(const T rhs) const { return vector2_base(x / rhs, y / rhs); }
	vector2_base operator/(const vector2_base& vec) const { return vector2_base(x / vec.x, y / vec.y); }

	const vector2_base& operator+=(const vector2_base& vec)
	{
		x += vec.x;
		y += vec.y;
		return *this;
	}
	const vector2_base& operator-=(const vector2_base& vec)
	{
		x -= vec.x;
		y -= vec.y;
		return *this;
	}
	const vector2_base& operator*=(const T rhs)
	{
		x *= rhs;
		y *= rhs;
		return *this;
	}
	const vector2_base& operator*=(const vector2_base& vec)
	{
		x *= vec.x;
		y *= vec.y;
		return *this;
	}
	const vector2_base& operator/=(const T rhs)
	{
		x /= rhs;
		y /= rhs;
		return *this;
	}
	const vector2_base& operator/=(const vector2_base& vec)
	{
		x /= vec.x;
		y /= vec.y;
		return *this;
	}

	bool operator==(const vector2_base& vec) const { return x == vec.x && y == vec.y; } //TODO: do this with an eps instead
	bool operator!=(const vector2_base& vec) const { return x != vec.x || y != vec.y; }

	T& operator[](const int index) { return index ? y : x; }
};


template<typename T>
constexpr inline vector2_base<T> rotate(const vector2_base<T>& v, float angle)
{
	float s = sinf(angle);
	float c = cosf(angle);
	return vector2_base<T>((T)(c * v.x - s * v.y), (T)(s * v.x + c * v.y));
}

template<typename T>
constexpr inline vector2_base<T> rotate(const vector2_base<T>& pos, const vector2_base<T>& curPos, float angle)
{
	float s = sinf(angle);
	float c = cosf(angle);
	return vector2_base<T>((T)(curPos.x + pos.x * c - pos.y * s), (T)(curPos.y + pos.x * s + pos.y * c));
}

template<typename T>
constexpr inline vector2_base<T> random_range_pos(const vector2_base<T>& curPos, float radius)
{
	float angle = random_float(0.0f, 2.0f * pi);
	float distance = sqrt(random_float()) * radius;
	return vector2_base<T>((T)curPos.x + distance * cos(angle), (T)curPos.y + distance * sin(angle));
}

template<typename T>
constexpr inline bool is_negative_vec(const vector2_base<T>& a)
{
	return a.x <= T{} || a.y <= T{};
}

template<typename T>
constexpr inline T dot(const vector2_base<T>& a, const vector2_base<T>& b)
{
	return a.x * b.x + a.y * b.y;
}

template<typename T>
constexpr inline T length_squared(const vector2_base<T>& a)
{
	return dot(a, a);
}

template<typename T>
constexpr inline T length(const vector2_base<T>& a)
{
	return std::sqrt(dot(a, a));
}

template<typename T>
constexpr inline T distance(const vector2_base<T>& a, const vector2_base<T>& b)
{
	return length(a - b);
}

template<typename T>
constexpr inline T distance_squared(const vector2_base<T>& a, const vector2_base<T>& b)
{
	const T dx = a.x - b.x;
	const T dy = a.y - b.y;
	return dx * dx + dy * dy;
}

template<std::floating_point T>
constexpr inline bool closest_point_on_line(const vector2_base<T>& line_pointA, const vector2_base<T>& line_pointB,
	const vector2_base<T>& target_point, vector2_base<T>& out_pos)
{
	constexpr T zero { 0 };
	const vector2_base<T> AB = line_pointB - line_pointA;
	const T SquaredMagnitudeAB = dot(AB, AB);

	if(SquaredMagnitudeAB <= zero)
	{
		out_pos = line_pointA;
		return false;
	}

	const vector2_base<T> AP = target_point - line_pointA;
	const T APdotAB = dot(AP, AB);

	if(APdotAB <= zero) [[likely]]
	{
		out_pos = line_pointA;
		return false;
	}
	else if(APdotAB >= SquaredMagnitudeAB) [[likely]]
	{
		out_pos = line_pointB;
		return false;
	}
	else
	{
		const T t = APdotAB / SquaredMagnitudeAB;
		out_pos = line_pointA + AB * t;
		return true;
	}
}

template<std::floating_point T>
constexpr inline bool is_within_distance_to_segment_sq(T squared_dist, const vector2_base<T>& line_pointA, const vector2_base<T>& line_pointB,
	const vector2_base<T>& target_point)
{
	constexpr T zero { 0 };

	const T dx_ab = line_pointB.x - line_pointA.x;
	const T dy_ab = line_pointB.y - line_pointA.y;
	const T dx_ap = target_point.x - line_pointA.x;
	const T dy_ap = target_point.y - line_pointA.y;

	const T APdotAB = dx_ap * dx_ab + dy_ap * dy_ab;
	const T SquaredDistAP = dx_ap * dx_ap + dy_ap * dy_ap;
	const T SquaredMagnitudeAB = dx_ab * dx_ab + dy_ab * dy_ab;

	if(APdotAB <= zero) [[likely]]
	{
		return SquaredDistAP < squared_dist;
	}
	else if(APdotAB >= SquaredMagnitudeAB) [[likely]]
	{
		const T SquaredDistBP = SquaredDistAP - static_cast<T>(2) * APdotAB + SquaredMagnitudeAB;
		return SquaredDistBP < squared_dist;
	}

	return SquaredDistAP * SquaredMagnitudeAB - APdotAB * APdotAB < squared_dist * SquaredMagnitudeAB;
}

constexpr inline float angle(const vector2_base<float>& a)
{
	if(a.x == 0 && a.y == 0)
		return 0.0f;
	else if(a.x == 0)
		return a.y < 0 ? -pi / 2 : pi / 2;
	float result = std::atan(a.y / a.x);
	if(a.x < 0)
		result = result + pi;
	return result;
}

inline float angle(const vector2_base<float>& a, const vector2_base<float>& b)
{
	float result = std::atan2(b.y - a.y, b.x - a.x) * 180 / pi;
	return result < 0 ? result + 360 : result;
}

template<typename T>
constexpr inline vector2_base<T> normalize_pre_length(const vector2_base<T>& v, T len)
{
	if(len == 0)
		return vector2_base<T>();
	return vector2_base<T>(v.x / len, v.y / len);
}

inline vector2_base<float> normalize(const vector2_base<float>& v)
{
	float divisor = length(v);
	if(divisor == 0.0f)
		return vector2_base<float>(0.0f, 0.0f);
	float l = (float)(1.0f / divisor);
	return vector2_base<float>(v.x * l, v.y * l);
}

inline vector2_base<float> direction(float angle)
{
	return vector2_base<float>(std::cos(angle), std::sin(angle));
}

inline vector2_base<float> random_direction()
{
	return direction(random_angle());
}

inline vector2_base<float> lerp(const vector2_base<float>& start, const vector2_base<float>& end, float t)
{
	return start + (end - start) * t;
}

typedef vector2_base<float> vec2;
typedef vector2_base<bool> bvec2;
typedef vector2_base<int> ivec2;

// ------------------------------------
template<typename T>
class vector3_base
{
public:
	union
	{
		T x, r, h, u;
	};
	union
	{
		T y, g, s, v;
	};
	union
	{
		T z, b, l, w;
	};

	constexpr vector3_base() = default;
	constexpr vector3_base(T nx, T ny, T nz) :
		x(nx), y(ny), z(nz)
	{
	}

	vector3_base operator-(const vector3_base& vec) const { return vector3_base(x - vec.x, y - vec.y, z - vec.z); }
	vector3_base operator-() const { return vector3_base(-x, -y, -z); }
	vector3_base operator+(const vector3_base& vec) const { return vector3_base(x + vec.x, y + vec.y, z + vec.z); }
	vector3_base operator*(const T rhs) const { return vector3_base(x * rhs, y * rhs, z * rhs); }
	vector3_base operator*(const vector3_base& vec) const { return vector3_base(x * vec.x, y * vec.y, z * vec.z); }
	vector3_base operator/(const T rhs) const { return vector3_base(x / rhs, y / rhs, z / rhs); }
	vector3_base operator/(const vector3_base& vec) const { return vector3_base(x / vec.x, y / vec.y, z / vec.z); }

	const vector3_base& operator+=(const vector3_base& vec)
	{
		x += vec.x;
		y += vec.y;
		z += vec.z;
		return *this;
	}
	const vector3_base& operator-=(const vector3_base& vec)
	{
		x -= vec.x;
		y -= vec.y;
		z -= vec.z;
		return *this;
	}
	const vector3_base& operator*=(const T rhs)
	{
		x *= rhs;
		y *= rhs;
		z *= rhs;
		return *this;
	}
	const vector3_base& operator*=(const vector3_base& vec)
	{
		x *= vec.x;
		y *= vec.y;
		z *= vec.z;
		return *this;
	}
	const vector3_base& operator/=(const T rhs)
	{
		x /= rhs;
		y /= rhs;
		z /= rhs;
		return *this;
	}
	const vector3_base& operator/=(const vector3_base& vec)
	{
		x /= vec.x;
		y /= vec.y;
		z /= vec.z;
		return *this;
	}

	bool operator==(const vector3_base& vec) const { return x == vec.x && y == vec.y && z == vec.z; } //TODO: do this with an eps instead
	bool operator!=(const vector3_base& vec) const { return x != vec.x || y != vec.y || z != vec.z; }
};

template<typename T>
inline T distance(const vector3_base<T>& a, const vector3_base<T>& b)
{
	return length(a - b);
}

template<typename T>
constexpr inline T dot(const vector3_base<T>& a, const vector3_base<T>& b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

template<typename T>
constexpr inline vector3_base<T> cross(const vector3_base<T>& a, const vector3_base<T>& b)
{
	return vector3_base<T>(
		a.y * b.z - a.z * b.y,
		a.z * b.x - a.x * b.z,
		a.x * b.y - a.y * b.x);
}

//
inline float length(const vector3_base<float>& a)
{
	return std::sqrt(dot(a, a));
}

inline vector3_base<float> normalize(const vector3_base<float>& v)
{
	float divisor = length(v);
	if(divisor == 0.0f)
		return vector3_base<float>(0.0f, 0.0f, 0.0f);
	float l = (float)(1.0f / divisor);
	return vector3_base<float>(v.x * l, v.y * l, v.z * l);
}

typedef vector3_base<float> vec3;
typedef vector3_base<bool> bvec3;
typedef vector3_base<int> ivec3;

// ------------------------------------

template<typename T>
class vector4_base
{
public:
	union
	{
		T x, r, h;
	};
	union
	{
		T y, g, s;
	};
	union
	{
		T z, b, l;
	};
	union
	{
		T w, a;
	};

	constexpr vector4_base() = default;
	constexpr vector4_base(T nx, T ny, T nz, T nw) :
		x(nx), y(ny), z(nz), w(nw)
	{
	}

	vector4_base operator+(const vector4_base& vec) const { return vector4_base(x + vec.x, y + vec.y, z + vec.z, w + vec.w); }
	vector4_base operator-(const vector4_base& vec) const { return vector4_base(x - vec.x, y - vec.y, z - vec.z, w - vec.w); }
	vector4_base operator-() const { return vector4_base(-x, -y, -z, -w); }
	vector4_base operator*(const vector4_base& vec) const { return vector4_base(x * vec.x, y * vec.y, z * vec.z, w * vec.w); }
	vector4_base operator*(const T rhs) const { return vector4_base(x * rhs, y * rhs, z * rhs, w * rhs); }
	vector4_base operator/(const vector4_base& vec) const { return vector4_base(x / vec.x, y / vec.y, z / vec.z, w / vec.w); }
	vector4_base operator/(const T vec) const { return vector4_base(x / vec, y / vec, z / vec, w / vec); }

	const vector4_base& operator+=(const vector4_base& vec)
	{
		x += vec.x;
		y += vec.y;
		z += vec.z;
		w += vec.w;
		return *this;
	}
	const vector4_base& operator-=(const vector4_base& vec)
	{
		x -= vec.x;
		y -= vec.y;
		z -= vec.z;
		w -= vec.w;
		return *this;
	}
	const vector4_base& operator*=(const T rhs)
	{
		x *= rhs;
		y *= rhs;
		z *= rhs;
		w *= rhs;
		return *this;
	}
	const vector4_base& operator*=(const vector4_base& vec)
	{
		x *= vec.x;
		y *= vec.y;
		z *= vec.z;
		w *= vec.w;
		return *this;
	}
	const vector4_base& operator/=(const T rhs)
	{
		x /= rhs;
		y /= rhs;
		z /= rhs;
		w /= rhs;
		return *this;
	}
	const vector4_base& operator/=(const vector4_base& vec)
	{
		x /= vec.x;
		y /= vec.y;
		z /= vec.z;
		w /= vec.w;
		return *this;
	}

	bool operator==(const vector4_base& vec) const { return x == vec.x && y == vec.y && z == vec.z && w == vec.w; } //TODO: do this with an eps instead
	bool operator!=(const vector4_base& vec) const { return x != vec.x || y != vec.y || z != vec.z || w != vec.w; }
};

typedef vector4_base<float> vec4;
typedef vector4_base<bool> bvec4;
typedef vector4_base<int> ivec4;
typedef vector4_base<uint8_t> ubvec4;

#endif
