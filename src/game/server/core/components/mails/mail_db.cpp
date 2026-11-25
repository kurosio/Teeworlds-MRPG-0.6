#include "mail_db.h"

namespace MailDb
{
	namespace
	{
		constexpr Db::EKind KIND = Db::KindOfTable("tw_accounts_mailbox");
	}

	Db::CRequest GetUnreadMailsNum(int AccountID)
	{
		// the counter has to see a letter that was sent a moment ago
		return Db::Query(KIND, "SELECT COUNT(*) AS Letters FROM tw_accounts_mailbox WHERE UserID = ? AND Readed = 0", Db::Bind(AccountID));
	}

	Db::CRequest ListMails(int AccountID, std::string_view Tag)
	{
		return Db::Query(KIND, "SELECT ID, Name, Sender, Readed FROM tw_accounts_mailbox WHERE UserID = ?",
			Db::Bind(AccountID), Db::SQueryOptions().Tag(Tag));
	}

	Db::CRequest ReadMail(int MailID, int AccountID, std::string_view Tag)
	{
		return Db::Query(KIND, "SELECT * FROM tw_accounts_mailbox WHERE ID = ? AND UserID = ?",
			Db::Bind(MailID, AccountID), Db::SQueryOptions().Tag(Tag));
	}

	Db::CRequest ReadAttachedItems(int MailID, int AccountID)
	{
		// Read the remote authority: a letter is claimed once, and the claim has to
		// see the items the letter really holds
		return Db::Query(KIND, "SELECT AttachedItems FROM tw_accounts_mailbox WHERE ID = ? AND UserID = ?",
			Db::Bind(MailID, AccountID));
	}

	Db::CRequest CreateMail(std::string_view Name, std::string_view Description, std::string_view Items, int AccountID, std::string_view Sender)
	{
		return Db::Query(KIND, "INSERT INTO tw_accounts_mailbox (Name, Description, AttachedItems, UserID, Sender) VALUES (?, ?, ?, ?, ?)",
			Db::Bind(Name, Description, Items, AccountID, Sender));
	}

	Db::CRequest MarkRead(int MailID, int AccountID)
	{
		return Db::Query(KIND, "UPDATE tw_accounts_mailbox SET Readed = 1 WHERE ID = ? AND UserID = ?",
			Db::Bind(MailID, AccountID));
	}

	Db::CRequest DeleteMail(int MailID, int AccountID)
	{
		return Db::Query(KIND, "DELETE FROM tw_accounts_mailbox WHERE ID = ? AND UserID = ?",
			Db::Bind(MailID, AccountID));
	}

	Db::CRequest DeleteReadMails(int AccountID)
	{
		return Db::Query(KIND, "DELETE FROM tw_accounts_mailbox WHERE UserID = ? AND Readed = 1", Db::Bind(AccountID));
	}
}
