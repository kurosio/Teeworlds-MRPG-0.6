/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_MMO_UTILS_FIELD_DATA_H
#define GAME_SERVER_MMO_UTILS_FIELD_DATA_H

//#include <type_traits>

// TODO: rework
// temporary replacement
template < typename T >
class CFieldData
{
	char m_aFieldName[128] = {};
	char m_aDescription[128] = {};

public:
	T m_Value;
	explicit operator T() const { return m_Value; }

	/*
	// basic operator
	CFieldData() = default;
	CFieldData(const CFieldData&) = delete;

	// operators
	template< typename U, typename = typename std::enable_if < std::is_arithmetic<T>::value >::type > T operator++(U) { m_Value++; return m_Value; }
	template< typename U, typename = typename std::enable_if < std::is_arithmetic<T>::value >::type > T operator--(U) { m_Value--; return m_Value; }

	template< typename U, typename = typename std::enable_if< std::is_same<U, T>::value >::type > T operator =(const U & r) { m_Value = r; return m_Value; }
	template< typename U, typename = typename std::enable_if< std::is_same<U, T>::value >::type > T operator-=(const U & r) { m_Value -= r; return m_Value; }
	template< typename U, typename = typename std::enable_if< std::is_same<U, T>::value >::type > T operator+=(const U & r) { m_Value += r; return m_Value; }

	// boolean operators
	template< typename U, typename = typename std::enable_if< std::is_same<U, T>::value >::type >
	bool operator==(const U & r) const { return m_Value == r; }
	template< typename U, typename = typename std::enable_if< std::is_same<U, T>::value >::type >
	bool operator!=(const U & r) const { return m_Value != r; }
	template< typename U, typename = typename std::enable_if< std::is_same<U, T>::value >::type >
	bool operator<(const U & r) const { return m_Value < r; }
	template< typename U, typename = typename std::enable_if< std::is_same<U, T>::value >::type >
	bool operator>(const U & r) const { return m_Value > r; }
	template< typename U, typename = typename std::enable_if< std::is_same<U, T>::value >::type >
	bool operator>=(const U & r) const { return m_Value >= r; }
	template< typename U, typename = typename std::enable_if< std::is_same<U, T>::value >::type >
	bool operator<=(const U & r) const { return m_Value <= r; }
	*/
	// functions
	void init(const char* pFieldName, const char* pDescription)
	{
		str_copy(m_aFieldName, pFieldName, sizeof(m_aFieldName));
		str_copy(m_aDescription, pDescription, sizeof(m_aDescription));
		m_Value = T(); // default constructor
	}
	const char* getFieldName() const { return m_aFieldName; };
	const char* getDescription() const { return m_aDescription; }
};

#endif