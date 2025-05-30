/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "mailbox_manager.h"

#include <game/server/gamecontext.h>
#include "mail_wrapper.h"

bool CMailboxManager::OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, int Extra1, int Extra2, int ReasonNumber, const char* pReason)
{
	// accept mail by id
	if(PPSTR(pCmd, "MAIL_ACCEPT") == 0)
	{
		if(AcceptMail(pPlayer, Extra1))
			pPlayer->m_VotesData.UpdateVotes(MENU_MAILBOX);
		else
			GS()->Chat(pPlayer->GetCID(), "You can't accept the mail attached item's");
		return true;
	}

	// delete mail by id
	if(PPSTR(pCmd, "MAIL_DELETE") == 0)
	{
		DeleteMail(Extra1);
		pPlayer->m_VotesData.UpdateVotes(MENU_MAILBOX);
		return true;
	}

	return false;
}

bool CMailboxManager::OnSendMenuVotes(CPlayer* pPlayer, int Menulist)
{
	const int ClientID = pPlayer->GetCID();

	// menu mailbox list
	if(Menulist == MENU_MAILBOX)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);

		// show mailbox list
		ShowMailboxList(pPlayer);

		// add backpage
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// menu mailbox selected mail
	if(Menulist == MENU_MAILBOX_SELECT)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAILBOX);

		if(const auto MailID = pPlayer->m_VotesData.GetExtraID())
		{
			ShowMail(MailID.value(), pPlayer);
		}

		// add backpage
		VoteWrapper::AddBackpage(pPlayer->GetCID());
		return true;
	}

	return false;
}

// check whether messages are available
int CMailboxManager::GetMailCount(int AccountID)
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("ID", "tw_accounts_mailbox", "WHERE UserID = '{}'", AccountID);
	const int MailValue = (int)pRes->rowsCount();
	return MailValue;
}

// show a list of mails
void CMailboxManager::ShowMailboxList(CPlayer *pPlayer)
{
	// structure mail
	struct BasicMailInfo
	{
		int m_ID {};
		std::string m_Name {};
		std::string m_Sender {};
	};
	std::vector<BasicMailInfo> vUnreadMails {};
	std::vector<BasicMailInfo> vReadedMails {};

	// collect by found from database
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_accounts_mailbox", "WHERE UserID = '{}'", pPlayer->Account()->GetID());
	while(pRes->next())
	{
		const bool Readed = pRes->getBoolean("Readed");

		// add new element
		BasicMailInfo Mail;
		Mail.m_ID = pRes->getInt("ID");
		Mail.m_Name = pRes->getString("Name").c_str();
		Mail.m_Sender = pRes->getString("Sender").c_str();
		if(Readed && vReadedMails.size() < MAIL_MAX_CAPACITY)
			vReadedMails.push_back(Mail);
		else if(vUnreadMails.size() < MAIL_MAX_CAPACITY)
			vUnreadMails.push_back(Mail);
	}

	// information
	const int ClientID = pPlayer->GetCID();
	VoteWrapper VInfo(ClientID, VWF_SEPARATE | VWF_STYLE_STRICT_BOLD, "Information about mailbox");
	VInfo.Add("You can interact with your mail");
	VInfo.Add("Receive as well as delete mails");
	VoteWrapper::AddEmptyline(ClientID);

	// unreaded mails
	VoteWrapper VUnreadList(ClientID, VWF_SEPARATE | VWF_ALIGN_TITLE | VWF_STYLE_SIMPLE,
		"\u2709 List of unread mails ({} of {})", (int)vUnreadMails.size(), (int)MAIL_MAX_CAPACITY);
	for(auto& p : vUnreadMails)
		VUnreadList.AddMenu(MENU_MAILBOX_SELECT, p.m_ID, "{} (UID:{})", p.m_Name,p.m_ID);
	VoteWrapper::AddEmptyline(ClientID);

	// readed mails
	VoteWrapper VReadList(ClientID, VWF_SEPARATE | VWF_ALIGN_TITLE | VWF_STYLE_SIMPLE,
		"\u2709 List of read mails ({} of {})", (int)vReadedMails.size(), (int)MAIL_MAX_CAPACITY);
	for(auto& p : vReadedMails)
		VReadList.AddMenu(MENU_MAILBOX_SELECT, p.m_ID, "{} (UID:{})", p.m_Name, p.m_ID);
	VoteWrapper::AddEmptyline(ClientID);
}

void CMailboxManager::ShowMail(int MailID, CPlayer* pPlayer) const
{
	int ClientID = pPlayer->GetCID();
	MarkReadedMail(MailID);

	// found from database by mail id
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_accounts_mailbox", "WHERE ID = '{}'", MailID);
	if(pRes->next())
	{
		// get the information to create an object
		std::string Name = pRes->getString("Name").c_str();
		std::string Sender = pRes->getString("Sender").c_str();

		// parse items
		const auto attachedItemsJson = pRes->getJson("AttachedItems");
		CItemsContainer vAttachedItems = attachedItemsJson.value("items", CItemsContainer {});

		// parse description lines
		std::string Descriptions = pRes->getString("Description").c_str();
		std::vector<std::string> vDescriptions {};
		{
			size_t start, end = 0;
			while((start = Descriptions.find_first_not_of("\n", end)) != std::string::npos)
			{
				end = Descriptions.find("\n", start);
				vDescriptions.push_back(Descriptions.substr(start, end - start));
			}
		}

		// show mail information
		VoteWrapper VInfo(ClientID, VWF_SEPARATE | VWF_STYLE_STRICT_BOLD, "{}", Name);
		for(auto& pLine : vDescriptions)
			VInfo.Add(pLine.c_str());
		VInfo.Add("Sender: {}", Sender);
		VoteWrapper::AddEmptyline(ClientID);

		// show attached item's information
		if(!vAttachedItems.empty())
		{
			VoteWrapper VAttached(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_STRICT, "Attached items");
			VAttached.ReinitNumeralDepthStyles({ { DEPTH_LVL1, DEPTH_LIST_STYLE_BOLD } });
			for(auto& pItem : vAttachedItems)
				VAttached.MarkList().Add("{}x{$} ({$})", pItem.Info()->GetName(), pItem.GetValue(), pPlayer->GetItem(pItem)->GetValue());
			VoteWrapper::AddEmptyline(ClientID);
		}

		// add buttons
		VoteWrapper(ClientID).AddOption("MAIL_ACCEPT", MailID, "Accept");
		VoteWrapper(ClientID).AddOption("MAIL_DELETE", MailID, "Delete");
		VoteWrapper::AddEmptyline(ClientID);
	}
}

bool CMailboxManager::AcceptMail(CPlayer* pPlayer, int MailID)
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("AttachedItems", "tw_accounts_mailbox", "WHERE ID = '{}'", MailID);
	if(!pRes->next())
		return false;

	// parse items
	const auto attachedItemsJson = pRes->getJson("AttachedItems");
	CItemsContainer vAttachedItems = attachedItemsJson.value("items", CItemsContainer {});

	// accept attached items
	CItemsContainer vCannotAcceptableItems {};
	for(auto& pItem : vAttachedItems)
	{
		CPlayerItem* pPlayerItem = pPlayer->GetItem(pItem);

		// check enchantable and has item
		if(pPlayerItem->HasItem() && !pItem.Info()->IsStackable())
		{
			vCannotAcceptableItems.push_back(pItem);
		}
		else
		{
			pPlayerItem->Add(pItem.GetValue(), 0, pItem.GetEnchant());
			GS()->Chat(pPlayer->GetCID(), "You received an attached item ['{}'].", pItem.Info()->GetName());
		}
	}

	// not one item can be accepted
	if(vAttachedItems.size() == vCannotAcceptableItems.size())
		return false;

	// send mail only with unaccable items
	if(!vCannotAcceptableItems.empty())
	{
		MailWrapper Mail("System", pPlayer->Account()->GetID(), "Cannot be accepted.");
		Mail.AddDescLine("The reason you already have the item");
		for(const auto& pItem : vCannotAcceptableItems)
			Mail.AttachItem(pItem);
		Mail.Send();
	}

	// is empty letter
	DeleteMail(MailID);
	return true;
}

void CMailboxManager::MarkReadedMail(int MailID) const
{
	// mark readed mail
	Database->Execute<DB::UPDATE>("tw_accounts_mailbox", "Readed = '1' WHERE ID = '{}'", MailID);
}

void CMailboxManager::DeleteMail(int MailID) const
{
	// remove from database
	Database->Execute<DB::REMOVE>("tw_accounts_mailbox", "WHERE ID = '{}'", MailID);
}