#include "relay_json.h"

#include <cstdint>

namespace relay
{
	bool ParseJson(const std::string &Payload, nlohmann::json &Out)
	{
		if(Payload.empty())
			return false;

		try
		{
			Out = nlohmann::json::parse(Payload);
		}
		catch(const nlohmann::json::exception &)
		{
			return false;
		}
		return Out.is_object();
	}

	std::string BuildJson(const nlohmann::json &Json)
	{
		try
		{
			return Json.dump();
		}
		catch(const nlohmann::json::exception &)
		{
			return std::string();
		}
	}

	int GetInt(const nlohmann::json &Json, const char *pKey, int Default)
	{
		const auto It = Json.find(pKey);
		if(It == Json.end())
			return Default;

		// A peer may send a number that does not fit into an int, or one that does
		// not fit into a signed 64 bit integer at all. nlohmann throws for both
		// cases, and an exception escaping here would take the program down.
		try
		{
			if(It->is_number_unsigned())
			{
				const auto Value = It->get<uint64_t>();
				return Value <= (uint64_t)INT32_MAX ? (int)Value : Default;
			}

			if(!It->is_number_integer())
				return Default;

			const auto Value = It->get<int64_t>();
			return Value >= INT32_MIN && Value <= INT32_MAX ? (int)Value : Default;
		}
		catch(const nlohmann::json::exception &)
		{
			return Default;
		}
	}

	bool GetBool(const nlohmann::json &Json, const char *pKey, bool Default)
	{
		const auto It = Json.find(pKey);
		if(It == Json.end() || !It->is_boolean())
			return Default;
		return It->get<bool>();
	}

	std::string GetString(const nlohmann::json &Json, const char *pKey, const std::string &Default)
	{
		const auto It = Json.find(pKey);
		if(It == Json.end() || !It->is_string())
			return Default;
		return It->get_ref<const std::string &>();
	}

	std::string GetCleanString(const nlohmann::json &Json, const char *pKey, size_t MaxLength)
	{
		std::string Value = GetString(Json, pKey);
		if(Value.size() > MaxLength)
			Value.resize(MaxLength);

		for(char &Character : Value)
		{
			if((unsigned char)Character < 0x20 || Character == 0x7f)
				Character = '?';
		}
		return Value;
	}
}
