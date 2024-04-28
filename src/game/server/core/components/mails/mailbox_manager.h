/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_COMPONENTS_MAILS_MAILBOX_MANAGER_H
#define GAME_SERVER_CORE_COMPONENTS_MAILS_MAILBOX_MANAGER_H

#include <game/server/core/mmo_component.h>

class CMailboxManager : public MmoComponent
{
	bool OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, int VoteID, int VoteID2, int Get, const char* GetText) override;
	bool OnHandleMenulist(CPlayer* pPlayer, int Menulist) override;

public:
	// get mail count
	int GetMailCount(int AccountID);

	// vote list's menus
	void ShowMailboxList(CPlayer *pPlayer);
	void ShowMail(int MailID, CPlayer *pPlayer) const;

private:
	// accept mail by id
	bool AcceptMail(CPlayer* pPlayer, int MailID);

	// mark mail readed by id
	void MarkReadedMail(int MailID) const;

	// delete mail by id
	void DeleteMail(int MailID) const;
};

#endif