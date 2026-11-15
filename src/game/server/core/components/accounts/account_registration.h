/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_ACCOUNT_REGISTRATION_H
#define GAME_SERVER_COMPONENT_ACCOUNT_REGISTRATION_H

#include <string>

class CAccountManager;

// Creates an account for the current nickname, or hands a guest nickname over to
// a real login. The nickname is checked and written on the primary: a lagging
// replica would hide the accounts registered moments ago on another server.
namespace AccountRegistration
{
	void Start(CAccountManager* pManager, int ClientID, std::string Login, std::string Password, std::string Nickname, std::string RegisterIP);
}

#endif
