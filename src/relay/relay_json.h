#ifndef ENGINE_RELAY_RELAY_JSON_H
#define ENGINE_RELAY_RELAY_JSON_H

#include <teeother/tools/nlohmann_json.h>

#include <string>

/*
 * Small helpers around the JSON payloads of the synchronization protocol.
 *
 * Parsing happens on data that arrives over the network, so every helper
 * either returns a complete answer or fails without throwing: a peer must never
 * be able to break the relay or a game server with a malformed message. The
 * fields are read defensively as well, a wrong type falls back to the default
 * instead of raising an exception.
 */
namespace relay
{
	// Parses `Payload` and keeps it only if it is a JSON object.
	bool ParseJson(const std::string &Payload, nlohmann::json &Out);

	// Serializes `Json`. Returns an empty string instead of throwing: an empty
	// payload is never a valid message, so callers can treat it as "failed".
	std::string BuildJson(const nlohmann::json &Json);

	// Field access with a default for a missing or mistyped field. An integer is
	// the only number type accepted, a float or a string falls back to
	// `Default`; a number that does not fit into an int does the same.
	int GetInt(const nlohmann::json &Json, const char *pKey, int Default = 0);
	std::string GetString(const nlohmann::json &Json, const char *pKey, const std::string &Default = std::string());

	// GetString() of a peer supplied name, cut to `MaxLength` bytes and stripped
	// of control characters, so it is safe to print. Returns an empty string for
	// a missing field.
	std::string GetCleanString(const nlohmann::json &Json, const char *pKey, size_t MaxLength);
}

#endif // ENGINE_RELAY_RELAY_JSON_H
