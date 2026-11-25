/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_COMPONENTS_MAILS_MAILBOX_MANAGER_H
#define GAME_SERVER_CORE_COMPONENTS_MAILS_MAILBOX_MANAGER_H

#include <game/server/core/mmo_component.h>

#include <engine/server/db/db_async.h>

class CMailboxManager : public MmoComponent
{
	bool OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, const std::vector<std::any>& Extras, int ReasonNumber, const char* pReason) override;
	bool OnSendMenuVotes(CPlayer* pPlayer, int Menulist) override;

public:
	Db::CRequest GetUnreadMailsNum(int AccountID);
	void NotifyUnreadMails(CPlayer* pPlayer);

	// Every menu opening reads current data from the database.
	void ShowMailboxList(CPlayer* pPlayer);
	void ShowMail(int MailID, CPlayer* pPlayer);

private:
	void AcceptMail(CPlayer* pPlayer, int MailID);
};

#endif