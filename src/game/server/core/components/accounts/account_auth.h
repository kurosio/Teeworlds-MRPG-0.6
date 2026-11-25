/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_ACCOUNT_AUTH_H
#define GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_ACCOUNT_AUTH_H

#include <string>
#include <string_view>

namespace AccountAuth
{
	// Credential rules, shared with the profile commands.
	constexpr int LENGTH_LOGPASS_MIN = 4;
	constexpr int LENGTH_LOGPASS_MAX = 12;
	constexpr int LENGTH_PIN_MIN = 4;
	constexpr int LENGTH_PIN_MAX = 6;

	// Chat commands /register and /login.
	void Register(int ClientID, const char* pLogin, const char* pPassword);
	void Login(int ClientID, const char* pLogin, const char* pPassword);

	// Guest accounts (the "Play" button).
	void RegisterGuest(int ClientID, const char* pNickname);
	void LoginGuest(int ClientID);

	// Credential helpers.
	std::string HashPassword(const std::string& Password, const std::string& Salt);
	std::string BuildGuestCredential(const char* pNickname);
	bool IsGuestCredentialForNickname(std::string_view Login, const char* pNickname);

	// PIN format checks, used by the profile commands with their own messages.
	bool IsValidPinLength(std::string_view Pin);
	bool IsDigitsOnly(std::string_view Text);
}

#endif
