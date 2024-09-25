/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "mail_wrapper.h"

#include <game/server/gamecontext.h>
#include "mailbox_manager.h"

void MailWrapper::Send()
{
	CGS* pGS = (CGS*)Instance::GameServer();

	// send information about new message
	const bool LocalMsg = pGS->ChatAccount(m_AccountID, "[Mailbox] New letter ({})!", m_Title);
	if(LocalMsg)
	{
		const int LetterCount = pGS->Core()->MailboxManager()->GetMailCount(m_AccountID);
		if(LetterCount >= (int)MAIL_MAX_CAPACITY)
		{
			pGS->ChatAccount(m_AccountID, "[Mailbox] Your mailbox is full you can't get.");
			pGS->ChatAccount(m_AccountID, "[Mailbox] It will come after you clear your mailbox.");
		}
	}

	// parse description's
	std::string EndDescription {};
	for(auto& Line : m_vDescriptionLines)
		EndDescription += Line + "\n";

	// prepare sql string
	const CSqlString<64> cTitle = CSqlString<64>(m_Title.c_str());
	const CSqlString<64> cSender = CSqlString<64>(m_Sender.c_str());
	const CSqlString<256> cDesc = CSqlString<256>(EndDescription.c_str());

	// get prepared json attached items
	nlohmann::json preparedJsItems {};
	CItem::ToArrayJSON(m_vAttachedItems, preparedJsItems, "items");

	// send to database
	Database->Execute<DB::INSERT>("tw_accounts_mailbox", "(Name, Description, AttachedItems, UserID, Sender) VALUES ('{}', '{}', '{}', '{}', '{}');",
		cTitle.cstr(), cDesc.cstr(), preparedJsItems.dump().c_str(), m_AccountID, cSender.cstr());
}
