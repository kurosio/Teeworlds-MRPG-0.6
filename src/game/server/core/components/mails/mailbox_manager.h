/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_COMPONENTS_MAILS_MAILBOX_MANAGER_H
#define GAME_SERVER_CORE_COMPONENTS_MAILS_MAILBOX_MANAGER_H

#include <game/server/core/mmo_component.h>

class CMailboxManager : public MmoComponent
{
	bool OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, const std::vector<std::any> &Extras, int ReasonNumber, const char* pReason) override;
	bool OnSendMenuVotes(CPlayer* pPlayer, int Menulist) override;

public:
	// get mail count
	int GetMailCount(int AccountID);

	// vote list's menus
	void ShowMailboxList(CPlayer *pPlayer);
	void ShowMail(int MailID, CPlayer *pPlayer) const;

private:

	bool AcceptMail(CPlayer* pPlayer, int MailID);
	void DeleteReadMails(int AccountID) const;
	void MarkReadedMail(int MailID) const;
	void DeleteMail(int MailID) const;
};

#endif