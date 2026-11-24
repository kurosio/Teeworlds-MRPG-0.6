/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_COMPONENTS_MAILS_MAILBOX_MANAGER_H
#define GAME_SERVER_CORE_COMPONENTS_MAILS_MAILBOX_MANAGER_H

#include <game/server/core/mmo_component.h>

#include <engine/server/db/db_async.h>

#include <game/server/core/components/inventory/item_data.h>

class CVoteLoad;

class CMailboxManager : public MmoComponent
{
	bool OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, const std::vector<std::any> &Extras, int ReasonNumber, const char* pReason) override;
	bool OnSendMenuVotes(CPlayer* pPlayer, int Menulist) override;

	// A menu is rendered from memory, so the mailbox is read in the background the
	// first time it is shown and the menu is rendered again when the answer is in.
	struct SBasicMail
	{
		int m_ID {};
		std::string m_Name {};
		std::string m_Sender {};
	};

	struct SMailView
	{
		bool m_Loaded {};
		int m_ID {};
		std::string m_Name {};
		std::string m_Sender {};
		std::string m_Description {};
		CItemsContainer m_Items {};
	};

	struct SMailbox
	{
		bool m_ListRead {};
		std::vector<SBasicMail> m_vUnread {};
		std::vector<SBasicMail> m_vRead {};
		SMailView m_View {};
	};

	// The identities of the two reads; a menu waits for them through VoteWrapper.
	static CVoteLoad ListLoad(int AccountID);
	static CVoteLoad ViewLoad(int AccountID, int MailID);

	std::unordered_map<int, SMailbox> m_vMailboxes {};
	void InvalidateMailbox(int ClientID) { m_vMailboxes.erase(ClientID); }

public:
	Db::CRequest CountMails(int AccountID);
	void NotifyUnreadMails(CPlayer* pPlayer);

	// vote list's menus
	void ShowMailboxList(CPlayer* pPlayer);
	void ShowMail(int MailID, CPlayer* pPlayer);

private:

	void AcceptMail(CPlayer* pPlayer, int MailID);
	void DeleteReadMails(int AccountID) const;
	void MarkReadedMail(int MailID) const;
	void DeleteMail(int MailID) const;
};

#endif