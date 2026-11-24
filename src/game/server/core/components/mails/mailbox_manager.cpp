/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "mailbox_manager.h"
#include "mail_db.h"

#include <game/server/core/tools/db_async_context.h>
#include <game/server/gamecontext.h>
#include "mail_wrapper.h"

bool CMailboxManager::OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, const std::vector<std::any> &Extras, int ReasonNumber, const char* pReason)
{
	// accept mail by id
	if(PPSTR(pCmd, "MAIL_ACCEPT") == 0)
	{
		const int MailID = GetIfExists<int>(Extras, 0, NOPE);
		AcceptMail(pPlayer, MailID);
		return true;
	}

	// delete mail by id
	if(PPSTR(pCmd, "MAIL_DELETE") == 0)
	{
		const int MailID = GetIfExists<int>(Extras, 0, NOPE);
		DeleteMail(MailID);
		InvalidateMailbox(pPlayer->GetCID());
		pPlayer->m_VotesData.UpdateVotes(MENU_MAILBOX);
		return true;
	}

	// delete all read mails
	if(PPSTR(pCmd, "MAIL_DELETE_READ") == 0)
	{
		DeleteReadMails(pPlayer->Account()->GetID());
		InvalidateMailbox(pPlayer->GetCID());
		GS()->Chat(pPlayer->GetCID(), "All read mails deleted.");
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

Db::CRequest CMailboxManager::CountMails(int AccountID)
{
	return MailDb::CountMails(AccountID);
}

CVoteLoad CMailboxManager::ListLoad(int AccountID)
{
	return CVoteLoad(fmt_default("mailbox_{}", AccountID));
}

CVoteLoad CMailboxManager::ViewLoad(int AccountID, int MailID)
{
	return CVoteLoad(fmt_default("mail_{}_{}", AccountID, MailID));
}

// tell the player how many letters are waiting, without holding up the login
void CMailboxManager::NotifyUnreadMails(CPlayer* pPlayer)
{
	if(!pPlayer)
		return;

	auto pContext = DbAsync::MakeContext(pPlayer->GetCID());
	CountMails(pPlayer->Account()->GetID()).Then([pContext](Db::CResultSet&& Result)
	{
		auto* pPlayer = pContext->GetPlayer();
		if(!pPlayer || !Result.HasRows())
			return;

		const int Letters = Result.GetInt("Letters");
		if(Letters > 0)
			pContext->GS()->Chat(pContext->GetClientID(), "You have '{} unread letters'.", Letters);
	});
}

// show a list of mails
void CMailboxManager::ShowMailboxList(CPlayer* pPlayer)
{
	const int ClientID = pPlayer->GetCID();
	auto& Mailbox = m_vMailboxes[ClientID];

	// The mailbox is read in the background the first time it is shown; the menu
	// is rendered from what was read and rendered again when the answer is in.
	if(!Mailbox.m_ListRead)
	{
		const CVoteLoad Load = ListLoad(pPlayer->Account()->GetID());
		VoteWrapper(ClientID).SetLoad(Load);
		if(!Load.Begin())
			return;
		auto pContext = DbAsync::MakeContext(ClientID);
		MailDb::ListMails(pPlayer->Account()->GetID())
			.Then([pContext, Load](Db::CResultSet&& Mails)
		{
			auto* pPlayer = pContext->GetPlayer();
			if(!pPlayer)
			{
				// the player is gone: the load must not stay marked as running
				Load.Finish();
				return;
			}

			auto* pManager = pContext->GS()->Core()->MailboxManager();
			auto& Cache = pManager->m_vMailboxes[pContext->GetClientID()];
			while(Mails.Next())
			{
				SBasicMail Mail;
				Mail.m_ID = Mails.GetInt("ID");
				Mail.m_Name = Mails.GetString("Name");
				Mail.m_Sender = Mails.GetString("Sender");

				auto& vTarget = Mails.GetBoolean("Readed") ? Cache.m_vRead : Cache.m_vUnread;
				if(vTarget.size() < MAIL_MAX_CAPACITY)
					vTarget.push_back(std::move(Mail));
			}
			Cache.m_ListRead = true;
			Load.Finish();
		});
		return;
	}

	// information
	VoteWrapper VInfo(ClientID, VWF_SEPARATE | VWF_STYLE_STRICT_BOLD, "Mailbox help");
	VInfo.Add("Open mail to read and claim items.");
	VInfo.Add("Use Claim to get items, Delete to remove.");
	VoteWrapper::AddEmptyline(ClientID);

	// unreaded mails
	VoteWrapper VUnreadList(ClientID, VWF_SEPARATE | VWF_ALIGN_TITLE | VWF_STYLE_SIMPLE,
		"\u2709 Unread: {} / {}", (int)Mailbox.m_vUnread.size(), (int)MAIL_MAX_CAPACITY);
	for(auto& p : Mailbox.m_vUnread)
		VUnreadList.AddMenu(MENU_MAILBOX_SELECT, p.m_ID, "{} [#{}]", p.m_Name, p.m_ID);
	VoteWrapper::AddEmptyline(ClientID);

	// readed mails
	VoteWrapper VReadList(ClientID, VWF_SEPARATE | VWF_ALIGN_TITLE | VWF_STYLE_SIMPLE,
		"\u2709 Read: {} / {}", (int)Mailbox.m_vRead.size(), (int)MAIL_MAX_CAPACITY);
	for(auto& p : Mailbox.m_vRead)
		VReadList.AddMenu(MENU_MAILBOX_SELECT, p.m_ID, "{} [#{}]", p.m_Name, p.m_ID);

	if(!Mailbox.m_vRead.empty())
		VoteWrapper(ClientID).AddOption("MAIL_DELETE_READ", "Delete all read");
	VoteWrapper::AddEmptyline(ClientID);
}

void CMailboxManager::ShowMail(int MailID, CPlayer* pPlayer)
{
	const int ClientID = pPlayer->GetCID();
	const int AccountID = pPlayer->Account()->GetID();
	auto& Mailbox = m_vMailboxes[ClientID];

	MarkReadedMail(MailID);

	// one mail, read the same way as the list: in the background, rendered after
	if(!Mailbox.m_View.m_Loaded || Mailbox.m_View.m_ID != MailID)
	{
		const CVoteLoad Load = ViewLoad(AccountID, MailID);
		VoteWrapper(ClientID).SetLoad(Load);
		if(!Load.Begin())
			return;

		Mailbox.m_View.m_ID = MailID;
		auto pContext = DbAsync::MakeContext(ClientID);
		MailDb::ReadMail(MailID, AccountID)
			.Then([pContext, MailID, Load](Db::CResultSet&& Rows)
		{
			auto* pPlayer = pContext->GetPlayer();
			if(!pPlayer || !Rows.HasRows())
			{
				Load.Finish();
				return;
			}

			auto* pManager = pContext->GS()->Core()->MailboxManager();
			auto& Cache = pManager->m_vMailboxes[pContext->GetClientID()];
			Cache.m_View.m_Loaded = true;
			Cache.m_View.m_ID = MailID;
			Cache.m_View.m_Name = Rows.GetString("Name");
			Cache.m_View.m_Sender = Rows.GetString("Sender");
			Cache.m_View.m_Description = Rows.GetString("Description");
			Cache.m_View.m_Items = Rows.GetJson("AttachedItems").value("items", CItemsContainer {});
			Load.Finish();
		});
		return;
	}

	const auto& View = Mailbox.m_View;
	VoteWrapper VInfo(ClientID, VWF_SEPARATE | VWF_STYLE_STRICT_BOLD, "{}", View.m_Name);
	{
		size_t start, end = 0;
		while((start = View.m_Description.find_first_not_of("\n", end)) != std::string::npos)
		{
			end = View.m_Description.find("\n", start);
			VInfo.Add("{~}", View.m_Description.substr(start, end - start).c_str());
		}
	}
	VInfo.Add("From: {~}", View.m_Sender);
	VoteWrapper::AddEmptyline(ClientID);

	// show attached item's information
	if(!View.m_Items.empty())
	{
		VoteWrapper VAttached(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_STRICT, "Items inside");
		VAttached.ReinitNumeralDepthStyles({ { DEPTH_LVL1, DEPTH_LIST_STYLE_BOLD } });
		for(auto& pItem : View.m_Items)
			VAttached.MarkList().Add("{}x{$} ({$})", pItem.Info()->GetName(), pItem.GetValue(), pPlayer->GetItem(pItem)->GetValue());
		VoteWrapper::AddEmptyline(ClientID);
	}

	// add buttons
	VoteWrapper(ClientID).AddOption("MAIL_ACCEPT", MailID, "Claim");
	VoteWrapper(ClientID).AddOption("MAIL_DELETE", MailID, "Delete");
	VoteWrapper::AddEmptyline(ClientID);
}

void CMailboxManager::AcceptMail(CPlayer* pPlayer, int MailID)
{
	// The answer decides what the player gets, so it is applied in the completion:
	// no item is handed out before the content of the letter is known.
	auto pContext = DbAsync::MakeContext(pPlayer->GetCID());
	MailDb::ReadAttachedItems(MailID, pPlayer->Account()->GetID())
		.Then([pContext, MailID](Db::CResultSet&& Rows)
	{
		auto* pPlayer = pContext->GetPlayer();
		if(!pPlayer)
			return;

		auto* pManager = pContext->GS()->Core()->MailboxManager();
		if(!Rows.HasRows())
		{
			pContext->GS()->Chat(pContext->GetClientID(), "Can't claim: no free slot for attached items.");
			return;
		}

		// parse items
		const auto attachedItemsJson = Rows.GetJson("AttachedItems");
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
				pPlayerItem->Add(pItem.GetValue(), 0, pItem.GetEnchant(), pItem.GetExpiresAt(), true, pItem.GetStringData());
				pContext->GS()->Chat(pContext->GetClientID(), "Item received: {}.", pItem.Info()->GetName());
			}
		}

		// not one item can be accepted
		if(vAttachedItems.size() == vCannotAcceptableItems.size())
		{
			pContext->GS()->Chat(pContext->GetClientID(), "Can't claim: no free slot for attached items.");
			return;
		}

		// send mail only with the items that could not be taken
		if(!vCannotAcceptableItems.empty())
		{
			MailWrapper Mail("System", pPlayer->Account()->GetID(), "Could not claim all items");
			Mail.AddDescLine("Some items were already owned.");
			for(const auto& pItem : vCannotAcceptableItems)
				Mail.AttachItem(pItem);
			Mail.Send();
		}

		// the letter is empty now
		pManager->DeleteMail(MailID);
		pManager->InvalidateMailbox(pContext->GetClientID());
		pPlayer->m_VotesData.UpdateVotesIf(MENU_MAILBOX);
	});
}

void CMailboxManager::DeleteReadMails(int AccountID) const
{
	MailDb::DeleteReadMails(AccountID);
}

void CMailboxManager::MarkReadedMail(int MailID) const
{
	// mark readed mail
	MailDb::MarkRead(MailID);
}

void CMailboxManager::DeleteMail(int MailID) const
{
	// remove from database
	MailDb::DeleteMail(MailID);
}
