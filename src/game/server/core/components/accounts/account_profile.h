/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_ACCOUNT_PROFILE_H
#define GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_ACCOUNT_PROFILE_H

#include <string>

namespace AccountProfile
{
	void SetPin(int ClientID, const char* pPassword, const char* pNewPin);
	void ChangePin(int ClientID, const char* pOldPin, const char* pNewPin);
	void ChangePassword(int ClientID, const char* pOldPassword, const char* pNewPassword, const char* pPinCode);
	void ChangeNickname(int ClientID, const std::string& NewNickname);
	void UseVoucher(int ClientID, const char* pVoucher);
}

#endif
