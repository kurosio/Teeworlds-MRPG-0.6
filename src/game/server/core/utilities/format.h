#ifndef GAME_SERVER_CORE_UTILITIES_FORMAT_H
#define GAME_SERVER_CORE_UTILITIES_FORMAT_H

#include <engine/server.h>

// namespace Tools
namespace Tools
{
	namespace String
	{
	    /**
	     * Formats a big digit value into a string with a specified separator.
	     *
	     * @tparam T The type of the value to format.
	     * @param Value The value to format.
	     * @return The formatted string.
	     */
	    template<typename T, const char separator = '.'>
	    std::string FormatBigDigit(T Value)
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
		std::string FormatDigit(T Value)
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

		// namespace FormatImpl
		namespace FormatImpl
		{
			// argument conversion
			template<typename T>
			static std::string toString(int ClientID, const T& Value)
			{
				if constexpr(std::is_same_v<T, double> || std::is_same_v<T, float>)
				{
					return std::to_string(Value);
				}
				else if constexpr(std::is_arithmetic_v<T>)
				{
					return FormatDigit<std::string>(std::to_string(Value));
				}
				else if constexpr(std::is_same_v<T, BigInt>)
				{
					return FormatBigDigit(Value.to_string());
				}
				else if constexpr(std::is_convertible_v<T, std::string>)
				{
					return Instance::Localize(ClientID, std::string(Value).c_str());
				}
				else
				{
					static_assert(!std::is_same_v<T, T>, "One of the passed arguments cannot be converted to a string");
					return "error convertible";
				}
			}

			// implementation for the last argument
			std::string Impl(int ClientID, const char* pText, std::list<std::string>& vStrPack);

			// implementation for recursive arguments
			template<typename T, typename... Ts>
			std::string Impl(int ClientID, const char* pText, std::list<std::string>& vStrPack, const T& Arg, const Ts &...Argpack)
			{
				vStrPack.emplace_back(toString(ClientID, Arg));
				return Impl(ClientID, pText, vStrPack, Argpack...);
			}
		}

		/**
		* Formats a string with the specified arguments.
		*
		* @tparam Ts The types of the arguments.
		* @param pText The format string.
		* @param ArgPack The arguments to format.
		* @return The formatted string.
		*/
		template<typename... Ts>
		std::string Format(const char* pText, const Ts &...ArgPack)
		{
			std::list<std::string> vStringPack {};
			return FormatImpl::Impl(-1, pText, vStringPack, ArgPack...);
		}

		/**
		* Formats a string without any arguments.
		*
		* @param pText The format string.
		* @return The formatted string.
		*/
		inline std::string Format(const char* pText)
		{
			return pText;
		}

		/**
		* Formats a localized string with the specified arguments.
		*
		* @tparam Ts The types of the arguments.
		* @param ClientID The client ID.
		* @param pText The format string.
		* @param Argpack The arguments to format.
		* @return The formatted localized string.
		*/
		template<typename... Ts>
		std::string FormatLocalize(int ClientID, const char* pText, const Ts &...Argpack)
		{
			std::list<std::string> vStringPack {};
			return FormatImpl::Impl(ClientID, Instance::Localize(ClientID, pText), vStringPack, Argpack...);
		}

		/**
		* Formats a localized string without any arguments.
		*
		* @param ClientID The client ID.
		* @param pText The format string.
		* @return The formatted localized string.
		*/
		inline std::string FormatLocalize(int ClientID, const char* pText)
		{
			return Instance::Localize(ClientID, pText);
		}
	}
}

#endif // _GAME_SERVER_TOOLS_FORMAT_H