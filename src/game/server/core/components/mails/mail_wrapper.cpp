/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "mail_wrapper.h"
#include "mail_db.h"

#include <game/server/gamecontext.h>
#include "mailbox_manager.h"

void MailWrapper::Send()
{
	CGS* pGS = (CGS*)Instance::GameServer();

	// send information about new message
	const bool LocalMsg = pGS->ChatAccount(m_AccountID, "[Mail] New mail: {}", m_Title);
	if(LocalMsg)
	{
		// the count is a statement of its own: the mail is already written, this
		// only warns the receiver, so it must not hold up the sender
		const int AccountID = m_AccountID;
		pGS->Core()->MailboxManager()->CountMails(AccountID).Then([AccountID](Db::CResultSet&& Result)
		{
			if(!Result.HasRows() || Result.GetInt("Letters") < (int)MAIL_MAX_CAPACITY)
				return;

			auto* pGS = (CGS*)Instance::GameServer();
			pGS->ChatAccount(AccountID, "[Mail] Mailbox is full.");
			pGS->ChatAccount(AccountID, "[Mail] Clear old mails to receive new ones.");
		});
	}

	// parse description's
	std::string EndDescription {};
	for(auto& Line : m_vDescriptionLines)
		EndDescription += Line + "\n";

	// get prepared json attached items
	nlohmann::json preparedItemsJson {};
	preparedItemsJson["items"] = m_vAttachedItems;

	// send to database
	MailDb::CreateMail(m_Title, EndDescription, preparedItemsJson.dump(), m_AccountID, m_Sender);
}
