#ifndef BASE_BIG_INT_H
#define BASE_BIG_INT_H

template <typename T>
concept BigIntConcept = std::integral<T> || std::floating_point<T>;

class intbig
{
	double m_Value;

public:
	// constructors
	intbig() : m_Value(0.0) {}

	template<BigIntConcept T>
	intbig(T v) : m_Value(static_cast<double>(v)) {}

	intbig(const std::string& str)
	{
		std::string filteredStr;
		for(const char ch : str)
		{
			if(std::isdigit(ch) || ch == '.' || ch == '-')
				filteredStr += ch;
		}

		// is has only
		if(filteredStr.empty() || filteredStr == "-" || filteredStr == ".")
		{
			m_Value = 0.0;
			return;
		}

		// try conversion
		try
		{
			m_Value = std::stod(filteredStr);
		}
		catch(...)
		{
			m_Value = 0.0;
		}
	}

	// conversion operators
	std::string to_string() const 
	{
		std::string resultStr = std::to_string(m_Value);

		if(const size_t pos = resultStr.find('.'); pos != std::string::npos)
		{
			const size_t lastNonZero = resultStr.find_last_not_of('0');
			if(lastNonZero == pos)
			{
				// erase comma
				resultStr.erase(pos);
			}
			else
			{
				// erase null
				resultStr.erase(lastNonZero + 1);
			}
		}

		return resultStr;
	}

	// implicit
	operator int() const { return static_cast<int>(m_Value); }
	operator float() const { return static_cast<float>(m_Value); }
	operator double() const { return m_Value; }
	operator long() const { return static_cast<long>(m_Value); }
	operator unsigned int() const { return static_cast<unsigned int>(m_Value); }
	operator long long() const { return static_cast<long long>(m_Value); }
	operator unsigned long long() const { return static_cast<unsigned long long>(m_Value); }

	// compare operators
	template<BigIntConcept T>
	std::partial_ordering operator<=>(T other) const
	{
		return this->m_Value <=> static_cast<double>(other);
	}

	std::partial_ordering operator<=>(const intbig& other) const
	{
		return this->m_Value <=> other.m_Value;
	}

	template<BigIntConcept T>
	bool operator==(T other) const
	{
		return std::fabs(this->m_Value - static_cast<double>(other)) < std::numeric_limits<double>::epsilon();
	}

	bool operator==(const intbig& other) const
	{
		return std::fabs(this->m_Value - other.m_Value) < std::numeric_limits<double>::epsilon();
	}


	// operators integral
	template<BigIntConcept T>
	intbig operator+(T other) const
	{
		return intbig(this->m_Value + static_cast<double>(other));
	}

	template<BigIntConcept T>
	intbig& operator+=(T other)
	{
		this->m_Value += static_cast<double>(other);
		return *this;
	}

	template<BigIntConcept T>
	intbig operator-(T other) const
	{
		return intbig(this->m_Value - static_cast<double>(other));
	}

	template<BigIntConcept T>
	intbig& operator-=(T other)
	{
		this->m_Value -= static_cast<double>(other);
		return *this;
	}

	template<BigIntConcept T>
	intbig operator*(T other) const
	{
		return intbig(this->m_Value * static_cast<double>(other));
	}

	template<BigIntConcept T>
	intbig& operator*=(T other)
	{
		this->m_Value *= static_cast<double>(other);
		return *this;
	}

	template<BigIntConcept T>
	intbig operator/(T other) const
	{
		if(other == 0)
		{
			throw std::overflow_error("Division by zero!");
		}
		return intbig(this->m_Value / static_cast<double>(other));
	}

	template<BigIntConcept T>
	intbig& operator/=(T other)
	{
		if(other == 0)
		{
			throw std::overflow_error("Division by zero!");
		}
		this->m_Value /= static_cast<double>(other);
		return *this;
	}

	// operators bigint
	intbig operator+(const intbig& other) const
	{
		return { this->m_Value + other.m_Value };
	}

	intbig& operator+=(const intbig& other)
	{
		this->m_Value += other.m_Value;
		return *this;
	}

	intbig operator-(const intbig& other) const
	{
		return { this->m_Value - other.m_Value };
	}

	intbig& operator-=(const intbig& other)
	{
		this->m_Value -= other.m_Value;
		return *this;
	}

	intbig operator*(const intbig& other) const
	{
		return { this->m_Value * other.m_Value };
	}

	intbig& operator*=(const intbig& other)
	{
		this->m_Value *= other.m_Value;
		return *this;
	}

	intbig operator/(const intbig& other) const
	{
		if(other.m_Value == 0.0)
		{
			throw std::overflow_error("Division by zero!");
		}
		return { this->m_Value / other.m_Value };
	}

	intbig& operator/=(const intbig& other)
	{
		if(other.m_Value == 0.0)
		{
			throw std::overflow_error("Division by zero!");
		}
		this->m_Value /= other.m_Value;
		return *this;
	}
};

#endif  // BASE_BIG_INT_H