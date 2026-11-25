/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "mail_wrapper.h"
#include "mail_db.h"

#include <game/server/gamecontext.h>
#include "mailbox_manager.h"

void MailWrapper::Send()
{
	auto* pGS = static_cast<CGS*>(Instance::GameServer());
	const bool LocalMsg = pGS->ChatAccount(m_AccountID, "[Mail] New mail: {}", m_Title);

	std::string Description {};
	for(const auto& Line : m_vDescriptionLines)
		Description += Line + "\n";

	nlohmann::json Items {};
	Items["items"] = m_vAttachedItems;

	const int AccountID = m_AccountID;
	MailDb::CreateMail(m_Title, Description, Items.dump(), AccountID, m_Sender)
		.Then([AccountID, LocalMsg](Db::CResultSet&& Created)
	{
		if(!LocalMsg || !Created.Ok())
			return;

		// The count runs after the insert: it sees the new letter.
		auto* pGS = static_cast<CGS*>(Instance::GameServer());
		if(!pGS)
			return;
		pGS->Core()->MailboxManager()->GetUnreadMailsNum(AccountID).Then([AccountID, pGS](Db::CResultSet&& Result)
		{
			if(!Result.Ok() || !Result.HasRows() || Result.GetInt("Letters") < (int)MAIL_MAX_CAPACITY)
				return;
			pGS->ChatAccount(AccountID, "[Mail] Mailbox is full.");
			pGS->ChatAccount(AccountID, "[Mail] Clear old mails to receive new ones.");
		});
	});
}
