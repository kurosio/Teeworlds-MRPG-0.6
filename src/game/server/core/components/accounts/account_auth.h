/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_ACCOUNT_AUTH_H
#define GAME_SERVER_COMPONENT_ACCOUNT_AUTH_H

#include <memory>
#include <string>

class CAccountManager;
struct CAccountSharedData;

// Login by credentials, and the "still in progress" notices of a login or a
// registration that waits for the primary.
namespace AccountAuth
{
	void Start(CAccountManager* pManager, int ClientID, std::string Login, std::string Password, std::string Nickname);

	void StartProgress(CAccountSharedData& Shared, const std::shared_ptr<void>& pAttempt, bool Registration);
	void TickProgress();
}

#endif
