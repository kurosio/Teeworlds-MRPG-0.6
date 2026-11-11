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
		if(It == Json.end() || !It->is_number_integer())
			return Default;

		// The value is an int64_t or a uint64_t depending on its sign, so both
		// ends are checked before the cast; a peer may send any number.
		if(It->is_number_unsigned())
		{
			const uint64_t Value = It->get<uint64_t>();
			return Value <= (uint64_t)INT32_MAX ? (int)Value : Default;
		}

		const int64_t Value = It->get<int64_t>();
		return Value >= INT32_MIN && Value <= INT32_MAX ? (int)Value : Default;
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
