/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "mailbox_manager.h"
#include "mail_db.h"

#include <game/server/core/tools/db_async_context.h>
#include <game/server/gamecontext.h>
#include "mail_wrapper.h"

namespace
{
	struct SMailRequestData
	{
		int m_AccountID {};
		uint64_t m_RequestID {};
	};

	using CMailRequestContext = DbAsync::CContext<SMailRequestData>;
	std::array<uint64_t, MAX_CLIENTS> s_aMailboxRequestID {};

	CPlayer* GetCurrentMailRequestPlayer(const std::shared_ptr<CMailRequestContext>& pContext, int MenuID, int MailID = NOPE)
	{
		CPlayer* pPlayer = pContext ? pContext->GetPlayer() : nullptr;
		if(!pPlayer)
			return nullptr;

		const auto& Request = pContext->Data();
		if(pPlayer->Account()->GetID() != Request.m_AccountID ||
			s_aMailboxRequestID[pContext->GetClientID()] != Request.m_RequestID ||
			pPlayer->m_VotesData.GetCurrentMenuID() != MenuID)
			return nullptr;

		if(MailID != NOPE)
		{
			const auto SelectedMail = pPlayer->m_VotesData.GetExtraID();
			if(!SelectedMail || *SelectedMail != MailID)
				return nullptr;
		}

		return pPlayer;
	}

	void BuildMailboxError(int ClientID, const char* pMessage, bool CanRetry)
	{
		mystd::freeContainer(VoteWrapper::Data()[ClientID]);
		VoteWrapper VError(ClientID, VWF_SEPARATE | VWF_STYLE_STRICT_BOLD, "Mailbox unavailable");
		VError.Add("{~}", pMessage);
		if(CanRetry)
			VError.AddOption("MAIL_RETRY_LIST", "Retry loading");
	}

	void BuildMailError(int ClientID)
	{
		mystd::freeContainer(VoteWrapper::Data()[ClientID]);
		VoteWrapper VError(ClientID, VWF_SEPARATE | VWF_STYLE_STRICT_BOLD, "Mail unavailable");
		VError.Add("Unable to load this mail. Return to mailbox and try again.");
	}
}

bool CMailboxManager::OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, const std::vector<std::any>& Extras, int ReasonNumber, const char* pReason)
{
	const int ClientID = pPlayer->GetCID();
	const int AccountID = pPlayer->Account()->GetID();

	if(PPSTR(pCmd, "MAIL_RETRY_LIST") == 0)
	{
		pPlayer->m_VotesData.UpdateVotes(MENU_MAILBOX);
		return true;
	}

	if(PPSTR(pCmd, "MAIL_ACCEPT") == 0)
	{
		AcceptMail(pPlayer, GetIfExists<int>(Extras, 0, NOPE));
		return true;
	}

	if(PPSTR(pCmd, "MAIL_DELETE") == 0)
	{
		const int MailID = GetIfExists<int>(Extras, 0, NOPE);
		auto pContext = DbAsync::MakeContext(ClientID);
		auto Request = MailDb::DeleteMail(MailID, AccountID);
		if(!Request)
		{
			GS()->Chat(ClientID, "Unable to start mail deletion.");
			return true;
		}

		Request.Then([pContext](Db::CResultSet&& Result)
		{
			auto* pPlayer = pContext->GetPlayer();
			if(!pPlayer)
				return;

			if(!Result.Ok())
			{
				pContext->GS()->Chat(pContext->GetClientID(), "Unable to delete this mail.");
				return;
			}
			pPlayer->m_VotesData.UpdateVotes(MENU_MAILBOX);
		});
		return true;
	}

	if(PPSTR(pCmd, "MAIL_DELETE_READ") == 0)
	{
		auto pContext = DbAsync::MakeContext(ClientID);
		auto Request = MailDb::DeleteReadMails(AccountID);
		if(!Request)
		{
			GS()->Chat(ClientID, "Unable to start deleting read mails.");
			return true;
		}

		Request.Then([pContext](Db::CResultSet&& Result)
		{
			auto* pPlayer = pContext->GetPlayer();
			if(!pPlayer)
				return;

			if(!Result.Ok())
			{
				pContext->GS()->Chat(pContext->GetClientID(), "Unable to delete read mails.");
				return;
			}
			pContext->GS()->Chat(pContext->GetClientID(), "All read mails deleted.");
			pPlayer->m_VotesData.UpdateVotes(MENU_MAILBOX);
		});
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

Db::CRequest CMailboxManager::GetUnreadMailsNum(int AccountID)
{
	return MailDb::GetUnreadMailsNum(AccountID);
}

// tell the player how many letters are waiting, without holding up the login
void CMailboxManager::NotifyUnreadMails(CPlayer* pPlayer)
{
	if(!pPlayer)
		return;

	auto pContext = DbAsync::MakeContext(pPlayer->GetCID());
	GetUnreadMailsNum(pPlayer->Account()->GetID()).Then([pContext](Db::CResultSet&& Result)
	{
		auto* pPlayer = pContext->GetPlayer();
		if(!pPlayer || !Result.Ok() || !Result.HasRows())
			return;

		const int Letters = Result.GetInt("Letters");
		if(Letters > 0)
			pContext->GS()->Chat(pContext->GetClientID(), "You have '{} unread letters'.", Letters);
	});
}

// The list is always read from the database. The callback owns its rows only
// until the current menu is rebuilt; no mailbox data is retained between opens.
void CMailboxManager::ShowMailboxList(CPlayer* pPlayer)
{
	const int ClientID = pPlayer->GetCID();
	const int AccountID = pPlayer->Account()->GetID();
	const uint64_t RequestID = ++s_aMailboxRequestID[ClientID];
	const std::string Tag = fmt_default("mailbox_list_{}_{}", ClientID, RequestID);
	auto pContext = DbAsync::MakeContext<SMailRequestData>(ClientID, SMailRequestData{ AccountID, RequestID });

	VoteWrapper VLoading(ClientID, VWF_SEPARATE | VWF_STYLE_STRICT_BOLD, "Mailbox");
	VLoading.Add("Loading mailbox ...");

	auto Request = MailDb::ListMails(AccountID, Tag);
	if(!Request)
	{
		BuildMailboxError(ClientID, "Unable to start the mailbox query.", true);
		return;
	}

	Request.Then([pContext](Db::CResultSet&& Mails)
	{
		CPlayer* pPlayer = GetCurrentMailRequestPlayer(pContext, MENU_MAILBOX);
		if(!pPlayer)
			return;

		const int ClientID = pContext->GetClientID();
		mystd::freeContainer(VoteWrapper::Data()[ClientID]);
		if(!Mails.Ok())
		{
			const auto& Error = Mails.Error();
			dbg_msg("Mailbox", "list query failed for account %d, code=%d sqlstate=%s",
				pContext->Data().m_AccountID, Error.m_Code,
				Error.m_SqlState.empty() ? "-" : Error.m_SqlState.c_str());
			BuildMailboxError(ClientID, "Unable to load your mail. Please retry.", true);
			VoteWrapper::AddBackpage(ClientID);
			VoteWrapper::RebuildVotes(ClientID);
			return;
		}

		struct SBasicMail
		{
			int m_ID {};
			std::string m_Name {};
		};
		std::vector<SBasicMail> vUnread;
		std::vector<SBasicMail> vRead;
		while(Mails.Next())
		{
			SBasicMail Mail;
			Mail.m_ID = Mails.GetInt("ID");
			Mail.m_Name = Mails.GetString("Name");
			auto& vTarget = Mails.GetBoolean("Readed") ? vRead : vUnread;
			if(vTarget.size() < MAIL_MAX_CAPACITY)
				vTarget.push_back(std::move(Mail));
		}

		VoteWrapper VInfo(ClientID, VWF_SEPARATE | VWF_STYLE_STRICT_BOLD, "Mailbox help");
		VInfo.Add("Open mail to read and claim items.");
		VInfo.Add("Use Claim to get items, Delete to remove.");
		VoteWrapper::AddEmptyline(ClientID);

		VoteWrapper VUnreadList(ClientID, VWF_SEPARATE | VWF_ALIGN_TITLE | VWF_STYLE_SIMPLE,
			"\u2709 Unread: {} / {}", (int)vUnread.size(), (int)MAIL_MAX_CAPACITY);
		for(const auto& Mail : vUnread)
			VUnreadList.AddMenu(MENU_MAILBOX_SELECT, Mail.m_ID, "{} [#{}]", Mail.m_Name, Mail.m_ID);
		VoteWrapper::AddEmptyline(ClientID);

		VoteWrapper VReadList(ClientID, VWF_SEPARATE | VWF_ALIGN_TITLE | VWF_STYLE_SIMPLE,
			"\u2709 Read: {} / {}", (int)vRead.size(), (int)MAIL_MAX_CAPACITY);
		for(const auto& Mail : vRead)
			VReadList.AddMenu(MENU_MAILBOX_SELECT, Mail.m_ID, "{} [#{}]", Mail.m_Name, Mail.m_ID);

		if(!vRead.empty())
			VoteWrapper(ClientID).AddOption("MAIL_DELETE_READ", "Delete all read");
		VoteWrapper::AddEmptyline(ClientID);
		VoteWrapper::AddBackpage(ClientID);
		VoteWrapper::RebuildVotes(ClientID);
	});
}

// Every entry performs a fresh read. The row data exists only in this callback.
void CMailboxManager::ShowMail(int MailID, CPlayer* pPlayer)
{
	const int ClientID = pPlayer->GetCID();
	const int AccountID = pPlayer->Account()->GetID();
	const uint64_t RequestID = ++s_aMailboxRequestID[ClientID];
	auto pContext = DbAsync::MakeContext<SMailRequestData>(ClientID, SMailRequestData{ AccountID, RequestID });

	VoteWrapper VLoading(ClientID, VWF_SEPARATE | VWF_STYLE_STRICT_BOLD, "Mail");
	VLoading.Add("Loading mail ...");

	const std::string Tag = fmt_default("mail_view_{}_{}_{}", ClientID, MailID, RequestID);
	auto Request = MailDb::ReadMail(MailID, AccountID, Tag);
	if(!Request)
	{
		BuildMailError(ClientID);
		return;
	}

	Request.Then([pContext, MailID](Db::CResultSet&& Rows)
	{
		CPlayer* pPlayer = GetCurrentMailRequestPlayer(pContext, MENU_MAILBOX_SELECT, MailID);
		if(!pPlayer)
			return;

		const int ClientID = pContext->GetClientID();
		if(!Rows.Ok() || !Rows.HasRows())
		{
			if(!Rows.Ok())
			{
				const auto& Error = Rows.Error();
				dbg_msg("Mailbox", "mail %d read failed for account %d, code=%d sqlstate=%s",
					MailID, pContext->Data().m_AccountID, Error.m_Code,
					Error.m_SqlState.empty() ? "-" : Error.m_SqlState.c_str());
			}
			BuildMailError(ClientID);
			VoteWrapper::AddBackpage(ClientID);
			VoteWrapper::RebuildVotes(ClientID);
			return;
		}

		const std::string Name = Rows.GetString("Name");
		const std::string Sender = Rows.GetString("Sender");
		const std::string Description = Rows.GetString("Description");
		const CItemsContainer Items = Rows.GetJson("AttachedItems").value("items", CItemsContainer {});

		// Finish the read-mark write before displaying the message. This guarantees
		// that returning to the list sees the updated Readed value.
		auto MarkRequest = MailDb::MarkRead(MailID, pContext->Data().m_AccountID);
		auto RenderMail = [pContext, pPlayer, ClientID, MailID, Name, Sender, Description, Items]()
		{
			if(GetCurrentMailRequestPlayer(pContext, MENU_MAILBOX_SELECT, MailID) != pPlayer)
				return;

			mystd::freeContainer(VoteWrapper::Data()[ClientID]);
			VoteWrapper VInfo(ClientID, VWF_SEPARATE | VWF_STYLE_STRICT_BOLD, "{}", Name);
			size_t Start, End = 0;
			while((Start = Description.find_first_not_of("\n", End)) != std::string::npos)
			{
				End = Description.find("\n", Start);
				VInfo.Add("{~}", Description.substr(Start, End - Start).c_str());
			}
			VInfo.Add("From: {~}", Sender);
			VoteWrapper::AddEmptyline(ClientID);

			if(!Items.empty())
			{
				VoteWrapper VAttached(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_STRICT, "Items inside");
				VAttached.ReinitNumeralDepthStyles({ { DEPTH_LVL1, DEPTH_LIST_STYLE_BOLD } });
				for(const auto& Item : Items)
				{
					const CPlayerItem* pItem = pPlayer->GetItem(Item);
					const int CurrentValue = pItem ? pItem->GetValue() : 0;
					VAttached.MarkList().Add("{}x{$} ({$})", Item.Info()->GetName(), Item.GetValue(), CurrentValue);
				}
				VoteWrapper::AddEmptyline(ClientID);
			}

			VoteWrapper(ClientID).AddOption("MAIL_ACCEPT", MailID, "Claim");
			VoteWrapper(ClientID).AddOption("MAIL_DELETE", MailID, "Delete");
			VoteWrapper::AddEmptyline(ClientID);
			VoteWrapper::AddBackpage(ClientID);
			VoteWrapper::RebuildVotes(ClientID);
		};

		if(MarkRequest)
			MarkRequest.Then([RenderMail = std::move(RenderMail)](Db::CResultSet&&) mutable { RenderMail(); });
		else
			RenderMail();
	});
}

void CMailboxManager::AcceptMail(CPlayer* pPlayer, int MailID)
{
	const int ClientID = pPlayer->GetCID();
	const int AccountID = pPlayer->Account()->GetID();
	auto pContext = DbAsync::MakeContext(ClientID);
	auto Request = MailDb::ReadAttachedItems(MailID, AccountID);
	if(!Request)
	{
		GS()->Chat(ClientID, "Unable to load attached items.");
		return;
	}

	Request.Then([pContext, MailID, AccountID](Db::CResultSet&& Rows)
	{
		auto* pPlayer = pContext->GetPlayer();
		if(!pPlayer)
			return;

		if(!Rows.Ok() || !Rows.HasRows())
		{
			pContext->GS()->Chat(pContext->GetClientID(), "Unable to claim this mail.");
			return;
		}

		const auto AttachedItemsJson = Rows.GetJson("AttachedItems");
		CItemsContainer vAttachedItems = AttachedItemsJson.value("items", CItemsContainer {});
		CItemsContainer vCannotAccept {};
		for(const auto& Item : vAttachedItems)
		{
			CPlayerItem* pPlayerItem = pPlayer->GetItem(Item);
			if(!pPlayerItem)
			{
				vCannotAccept.push_back(Item);
				continue;
			}

			if(pPlayerItem->HasItem() && !Item.Info()->IsStackable())
			{
				vCannotAccept.push_back(Item);
				continue;
			}

			pPlayerItem->Add(Item.GetValue(), 0, Item.GetEnchant(), Item.GetExpiresAt(), true, Item.GetStringData());
			pContext->GS()->Chat(pContext->GetClientID(), "Item received: {}.", Item.Info()->GetName());
		}

		if(!vAttachedItems.empty() && vAttachedItems.size() == vCannotAccept.size())
		{
			pContext->GS()->Chat(pContext->GetClientID(), "Can't claim: no free slot for attached items.");
			return;
		}

		if(!vCannotAccept.empty())
		{
			MailWrapper Mail("System", AccountID, "Could not claim all items");
			Mail.AddDescLine("Some items were already owned.");
			for(const auto& Item : vCannotAccept)
				Mail.AttachItem(Item);
			Mail.Send();
		}

		auto DeleteRequest = MailDb::DeleteMail(MailID, AccountID);
		if(!DeleteRequest)
		{
			pContext->GS()->Chat(pContext->GetClientID(), "Items were received, but the mail could not be removed.");
			return;
		}

		DeleteRequest.Then([pContext](Db::CResultSet&& Result)
		{
			auto* pPlayer = pContext->GetPlayer();
			if(!pPlayer)
				return;

			if(!Result.Ok())
			{
				pContext->GS()->Chat(pContext->GetClientID(), "Items were received, but the mail could not be removed.");
				return;
			}
			pPlayer->m_VotesData.UpdateVotes(MENU_MAILBOX);
		});
	});
}
