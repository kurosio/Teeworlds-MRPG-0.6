#ifndef GAME_SERVER_CORE_COMPONENTS_MAILS_MAIL_DB_H
#define GAME_SERVER_CORE_COMPONENTS_MAILS_MAIL_DB_H

// The statements of the mailbox component: the letters of one account and the
// single letter that is opened or claimed.

#include <engine/server/db/db_async.h>

namespace MailDb
{
	Db::CRequest GetUnreadMailsNum(int AccountID);
	Db::CRequest ListMails(int AccountID, std::string_view Tag = {});
	Db::CRequest ReadMail(int MailID, int AccountID, std::string_view Tag = {});
	Db::CRequest ReadAttachedItems(int MailID, int AccountID);
	Db::CRequest CreateMail(std::string_view Name, std::string_view Description, std::string_view Items, int AccountID, std::string_view Sender);
	Db::CRequest MarkRead(int MailID, int AccountID, std::string_view Tag = {});
	Db::CRequest DeleteMail(int MailID, int AccountID);
	Db::CRequest DeleteReadMails(int AccountID);
}

#endif // GAME_SERVER_COMPONENT_MAILS_MAIL_DB_H
