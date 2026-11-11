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

	// Type-safe field access with a default for a missing or mistyped field.
	int GetInt(const nlohmann::json &Json, const char *pKey, int Default = 0);
	bool GetBool(const nlohmann::json &Json, const char *pKey, bool Default = false);
	std::string GetString(const nlohmann::json &Json, const char *pKey, const std::string &Default = std::string());

	// GetString() of a peer supplied name, cut to `MaxLength` bytes and stripped
	// of control characters, so it is safe to print. Returns an empty string for
	// a missing field.
	std::string GetCleanString(const nlohmann::json &Json, const char *pKey, size_t MaxLength);
}

#endif // ENGINE_RELAY_RELAY_JSON_H
