/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "mailbox_manager.h"
#include "mail_db.h"

#include <game/server/core/tools/db_async_context.h>
#include <game/server/gamecontext.h>
#include "mail_wrapper.h"

#include <exception>
#include <optional>

namespace
{
	struct SMailEntry
	{
		int m_ID {};
		std::string m_Name {};
	};

	struct SMailList
	{
		std::vector<SMailEntry> m_vUnread {};
		std::vector<SMailEntry> m_vRead {};
	};

	struct SMailView
	{
		int m_ID { NOPE };
		std::string m_Name {};
		std::string m_Sender {};
		std::string m_Description {};
		CItemsContainer m_Items {};
	};

	// Rows are kept only between the tagged query and the redraw it triggers.
	// A menu always asks for a fresh answer; nothing is reused on the next open.
	template<typename T>
	struct TRequestResult
	{
		std::string m_Tag {};
		int m_AccountID { NOPE };
		bool m_Pending {};
		bool m_Ready {};
		bool m_Failed {};
		T m_Value {};
	};

	using CMailListResult = TRequestResult<SMailList>;
	using CMailViewResult = TRequestResult<SMailView>;
	std::array<CMailListResult, MAX_CLIENTS> s_aLists {};
	std::array<CMailViewResult, MAX_CLIENTS> s_aViews {};
	std::array<bool, MAX_CLIENTS> s_aClaiming {};

	std::string ListTag(int AccountID)
	{
		return fmt_default("mailbox_list_{}", AccountID);
	}

	std::string ViewTag(int AccountID, int MailID)
	{
		return fmt_default("mail_view_{}_{}", AccountID, MailID);
	}

	template<typename T>
	void Reset(TRequestResult<T>& Result)
	{
		Result = TRequestResult<T> {};
	}

	CPlayer* GetRequestPlayer(const std::shared_ptr<DbAsync::CContextBase>& pContext, int AccountID)
	{
		CPlayer* pPlayer = pContext ? pContext->GetPlayer() : nullptr;
		return pPlayer && pPlayer->Account()->GetID() == AccountID ? pPlayer : nullptr;
	}

	CItemsContainer GetAttachedItems(const Db::CResultSet& Rows)
	{
		const auto Json = Rows.GetJson("AttachedItems");
		if(!Json.is_object())
			return {};

		try
		{
			return Json.value("items", CItemsContainer {});
		}
		catch(const std::exception&)
		{
			return {};
		}
	}

	// A watcher of a superseded request must not clear the state of a newer one.
	// When the answer is no longer wanted, drop it instead of redrawing a menu
	// that has already moved on.
	void WatchListRefresh(const std::shared_ptr<DbAsync::CContextBase>& pContext, int ClientID, int AccountID, const std::string& Tag)
	{
		Db::WatchTag(Tag, [pContext, ClientID, AccountID, Tag]
		{
			auto& Result = s_aLists[ClientID];
			if(Result.m_Tag != Tag)
				return;

			CPlayer* pPlayer = GetRequestPlayer(pContext, AccountID);
			if(!pPlayer || !Result.m_Ready || pPlayer->m_VotesData.GetCurrentMenuID() != MENU_MAILBOX)
				Reset(Result);
		});
	}

	void WatchViewRefresh(const std::shared_ptr<DbAsync::CContextBase>& pContext, int ClientID, int AccountID, int MailID, const std::string& Tag)
	{
		Db::WatchTag(Tag, [pContext, ClientID, AccountID, MailID, Tag]
		{
			auto& Result = s_aViews[ClientID];
			if(Result.m_Tag != Tag)
				return;

			CPlayer* pPlayer = GetRequestPlayer(pContext, AccountID);
			const auto SelectedMail = pPlayer ? pPlayer->m_VotesData.GetExtraID() : std::optional<int> {};
			if(!pPlayer || !Result.m_Ready || Result.m_Value.m_ID != MailID ||
				pPlayer->m_VotesData.GetCurrentMenuID() != MENU_MAILBOX_SELECT ||
				!SelectedMail || *SelectedMail != MailID)
				Reset(Result);
		});
	}

	void ResetMailMenus(int ClientID)
	{
		Reset(s_aLists[ClientID]);
		Reset(s_aViews[ClientID]);
	}

	void RefreshMailbox(CPlayer* pPlayer)
	{
		const int ClientID = pPlayer->GetCID();
		ResetMailMenus(ClientID);
		pPlayer->m_VotesData.UpdateVotes(MENU_MAILBOX);
	}

	void BuildMailboxError(int ClientID, const char* pMessage)
	{
		VoteWrapper VError(ClientID, VWF_SEPARATE | VWF_STYLE_STRICT_BOLD, "Mailbox unavailable");
		VError.Add("{~}", pMessage);
		VError.AddOption("MAIL_RETRY_LIST", "Retry loading");
	}

	void BuildMailError(int ClientID)
	{
		VoteWrapper VError(ClientID, VWF_SEPARATE | VWF_STYLE_STRICT_BOLD, "Mail unavailable");
		VError.Add("Unable to load this mail. Return to mailbox and try again.");
	}

	void RenderMailboxList(int ClientID, const SMailList& Mails)
	{
		VoteWrapper VInfo(ClientID, VWF_SEPARATE | VWF_STYLE_STRICT_BOLD, "Mailbox help");
		VInfo.Add("Open mail to read and claim items.");
		VInfo.Add("Use Claim to get items, Delete to remove.");
		VoteWrapper::AddEmptyline(ClientID);

		VoteWrapper VUnreadList(ClientID, VWF_SEPARATE | VWF_ALIGN_TITLE | VWF_STYLE_SIMPLE,
			"\u2709 Unread: {} / {}", (int)Mails.m_vUnread.size(), (int)MAIL_MAX_CAPACITY);
		for(const auto& Mail : Mails.m_vUnread)
			VUnreadList.AddMenu(MENU_MAILBOX_SELECT, Mail.m_ID, "{} [#{}]", Mail.m_Name, Mail.m_ID);
		VoteWrapper::AddEmptyline(ClientID);

		VoteWrapper VReadList(ClientID, VWF_SEPARATE | VWF_ALIGN_TITLE | VWF_STYLE_SIMPLE,
			"\u2709 Read: {} / {}", (int)Mails.m_vRead.size(), (int)MAIL_MAX_CAPACITY);
		for(const auto& Mail : Mails.m_vRead)
			VReadList.AddMenu(MENU_MAILBOX_SELECT, Mail.m_ID, "{} [#{}]", Mail.m_Name, Mail.m_ID);

		if(!Mails.m_vRead.empty())
			VoteWrapper(ClientID).AddOption("MAIL_DELETE_READ", "Delete all read");
		VoteWrapper::AddEmptyline(ClientID);
	}

	void RenderMailView(int ClientID, CPlayer* pPlayer, const SMailView& View)
	{
		VoteWrapper VInfo(ClientID, VWF_SEPARATE | VWF_STYLE_STRICT_BOLD, "{}", View.m_Name);
		std::string_view Lines(View.m_Description);
		for(size_t Start = 0; Start < Lines.size();)
		{
			const size_t End = Lines.find('\n', Start);
			const auto Line = Lines.substr(Start, End == std::string_view::npos ? End : End - Start);
			if(!Line.empty() && Line.front() != '\r')
				VInfo.Add("{~}", std::string(Line).c_str());
			if(End == std::string_view::npos)
				break;
			Start = End + 1;
		}
		VInfo.Add("From: {~}", View.m_Sender);
		VoteWrapper::AddEmptyline(ClientID);

		if(!View.m_Items.empty())
		{
			VoteWrapper VAttached(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_STRICT, "Items inside");
			VAttached.ReinitNumeralDepthStyles({ { DEPTH_LVL1, DEPTH_LIST_STYLE_BOLD } });
			for(const auto& Item : View.m_Items)
			{
				const CPlayerItem* pItem = pPlayer->GetItem(Item);
				VAttached.MarkList().Add("{}x{$} ({$})", Item.Info()->GetName(), Item.GetValue(), pItem ? pItem->GetValue() : 0);
			}
			VoteWrapper::AddEmptyline(ClientID);
		}

		VoteWrapper(ClientID).AddOption("MAIL_ACCEPT", View.m_ID, "Claim");
		VoteWrapper(ClientID).AddOption("MAIL_DELETE", View.m_ID, "Delete");
		VoteWrapper::AddEmptyline(ClientID);
	}
}

bool CMailboxManager::OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, const std::vector<std::any>& Extras, int ReasonNumber, const char* pReason)
{
	const int ClientID = pPlayer->GetCID();
	const int AccountID = pPlayer->Account()->GetID();
	const int MailID = GetIfExists<int>(Extras, 0, NOPE);
	const bool IsMailView = pPlayer->m_VotesData.GetCurrentMenuID() == MENU_MAILBOX_SELECT;

	if(PPSTR(pCmd, "MAIL_RETRY_LIST") == 0)
	{
		RefreshMailbox(pPlayer);
		return true;
	}

	if(PPSTR(pCmd, "MAIL_ACCEPT") == 0)
	{
		if(MailID > 0 && IsMailView)
			AcceptMail(pPlayer, MailID);
		return true;
	}

	if(PPSTR(pCmd, "MAIL_DELETE") == 0)
	{
		if(MailID <= 0 || !IsMailView)
			return true;

		ResetMailMenus(ClientID);
		auto pContext = DbAsync::MakeContext(ClientID);
		MailDb::DeleteMail(MailID, AccountID).Then([pContext, ClientID, AccountID](Db::CResultSet&& Result)
		{
			CPlayer* pPlayer = GetRequestPlayer(pContext, AccountID);
			if(!pPlayer)
				return;
			if(!Result.Ok())
				pContext->GS()->Chat(ClientID, "Unable to delete this mail.");
			RefreshMailbox(pPlayer);
		});
		return true;
	}

	if(PPSTR(pCmd, "MAIL_DELETE_READ") == 0)
	{
		ResetMailMenus(ClientID);
		auto pContext = DbAsync::MakeContext(ClientID);
		MailDb::DeleteReadMails(AccountID).Then([pContext, ClientID, AccountID](Db::CResultSet&& Result)
		{
			CPlayer* pPlayer = GetRequestPlayer(pContext, AccountID);
			if(!pPlayer)
				return;
			if(!Result.Ok())
				pContext->GS()->Chat(ClientID, "Unable to delete read mails.");
			else
				pContext->GS()->Chat(ClientID, "All read mails deleted.");
			RefreshMailbox(pPlayer);
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
		ShowMailboxList(pPlayer);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// menu mailbox selected mail
	if(Menulist == MENU_MAILBOX_SELECT)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAILBOX);
		if(const auto MailID = pPlayer->m_VotesData.GetExtraID())
			ShowMail(MailID.value(), pPlayer);
		VoteWrapper::AddBackpage(ClientID);
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

	const int ClientID = pPlayer->GetCID();
	const int AccountID = pPlayer->Account()->GetID();
	auto pContext = DbAsync::MakeContext(ClientID);
	GetUnreadMailsNum(AccountID).Then([pContext, ClientID, AccountID](Db::CResultSet&& Result)
	{
		if(!GetRequestPlayer(pContext, AccountID) || !Result.Ok() || !Result.HasRows())
			return;

		const int Letters = Result.GetInt("Letters");
		if(Letters > 0)
			pContext->GS()->Chat(ClientID, "You have '{} unread letters'.", Letters);
	});
}

void CMailboxManager::ShowMailboxList(CPlayer* pPlayer)
{
	const int ClientID = pPlayer->GetCID();
	const int AccountID = pPlayer->Account()->GetID();
	auto& Result = s_aLists[ClientID];
	const std::string Tag = ListTag(AccountID);

	// The list is the entry point: a letter opened from it must be read again.
	Reset(s_aViews[ClientID]);
	if(Result.m_AccountID != AccountID)
		Reset(Result);

	// One request per answer. The tagged refresh redraws this menu once the rows
	// arrive, so the callback never has to build or send votes itself.
	if(!Result.m_Pending && !Result.m_Ready)
	{
		Result.m_Tag = Tag;
		Result.m_AccountID = AccountID;
		Result.m_Pending = true;
		auto pContext = DbAsync::MakeContext(ClientID);
		auto Request = MailDb::ListMails(AccountID, Tag);
		WatchListRefresh(pContext, ClientID, AccountID, Tag);
		Request.Then([pContext, ClientID, AccountID, Tag](Db::CResultSet&& Mails)
		{
			auto& Result = s_aLists[ClientID];
			if(!GetRequestPlayer(pContext, AccountID) || !Result.m_Pending || Result.m_Tag != Tag)
				return;

			Result.m_Pending = false;
			Result.m_Ready = true;
			if(!Mails.Ok())
			{
				const auto& Error = Mails.Error();
				dbg_msg("Mailbox", "list query failed for account %d, code=%d sqlstate=%s", AccountID,
					Error.m_Code, Error.m_SqlState.empty() ? "-" : Error.m_SqlState.c_str());
				Result.m_Failed = true;
				return;
			}

			while(Mails.Next())
			{
				SMailEntry Mail { Mails.GetInt("ID"), Mails.GetString("Name") };
				auto& vTarget = Mails.GetBoolean("Readed") ? Result.m_Value.m_vRead : Result.m_Value.m_vUnread;
				if(vTarget.size() < MAIL_MAX_CAPACITY)
					vTarget.push_back(std::move(Mail));
			}
		});
	}

	if(!Result.m_Ready)
	{
		VoteWrapper VLoading(ClientID, VWF_SEPARATE | VWF_STYLE_STRICT_BOLD, "Mailbox");
		(void)VLoading.WaitLoad(Tag);
		VLoading.Add("Loading mailbox ...");
		return;
	}

	SMailList Mails = std::move(Result.m_Value);
	const bool Failed = Result.m_Failed;
	Reset(Result);
	if(Failed)
		BuildMailboxError(ClientID, "Unable to load your mail. Please retry.");
	else
		RenderMailboxList(ClientID, Mails);
}

void CMailboxManager::ShowMail(int MailID, CPlayer* pPlayer)
{
	const int ClientID = pPlayer->GetCID();
	const int AccountID = pPlayer->Account()->GetID();
	if(MailID <= 0)
	{
		BuildMailError(ClientID);
		return;
	}

	auto& Result = s_aViews[ClientID];
	const std::string Tag = ViewTag(AccountID, MailID);
	if(Result.m_AccountID != AccountID || Result.m_Value.m_ID != MailID)
		Reset(Result);

	// Reading a letter changes the list, so returning to it needs a fresh read.
	if(!Result.m_Pending && !Result.m_Ready)
	{
		Reset(s_aLists[ClientID]);
		Result.m_Tag = Tag;
		Result.m_AccountID = AccountID;
		Result.m_Pending = true;
		auto pContext = DbAsync::MakeContext(ClientID);
		auto Request = MailDb::ReadMail(MailID, AccountID, Tag);
		WatchViewRefresh(pContext, ClientID, AccountID, MailID, Tag);
		Request.Then([pContext, ClientID, AccountID, MailID, Tag](Db::CResultSet&& Rows)
		{
			auto& Result = s_aViews[ClientID];
			if(!GetRequestPlayer(pContext, AccountID) || !Result.m_Pending || Result.m_Tag != Tag)
				return;

			Result.m_Pending = false;
			Result.m_Ready = true;
			Result.m_Value.m_ID = MailID;
			if(!Rows.Ok() || !Rows.HasRows())
			{
				if(!Rows.Ok())
				{
					const auto& Error = Rows.Error();
					dbg_msg("Mailbox", "mail %d read failed for account %d, code=%d sqlstate=%s", MailID,
						AccountID, Error.m_Code, Error.m_SqlState.empty() ? "-" : Error.m_SqlState.c_str());
				}
				Result.m_Failed = true;
				return;
			}

			Result.m_Value.m_Name = Rows.GetString("Name");
			Result.m_Value.m_Sender = Rows.GetString("Sender");
			Result.m_Value.m_Description = Rows.GetString("Description");
			Result.m_Value.m_Items = GetAttachedItems(Rows);

			// Both statements share the tag: the menu redraws after the read flag
			// has landed, and going back sees the updated mailbox list.
			MailDb::MarkRead(MailID, AccountID, Tag).Then([MailID, AccountID](Db::CResultSet&& Written)
			{
				if(!Written.Ok())
					dbg_msg("Mailbox", "mail %d mark read failed for account %d, code=%d", MailID, AccountID, Written.Error().m_Code);
			});
		});
	}

	if(!Result.m_Ready)
	{
		VoteWrapper VLoading(ClientID, VWF_SEPARATE | VWF_STYLE_STRICT_BOLD, "Mail");
		(void)VLoading.WaitLoad(Tag);
		VLoading.Add("Loading mail ...");
		return;
	}

	SMailView View = std::move(Result.m_Value);
	const bool Failed = Result.m_Failed;
	Reset(Result);
	if(Failed)
		BuildMailError(ClientID);
	else
		RenderMailView(ClientID, pPlayer, View);
}

void CMailboxManager::AcceptMail(CPlayer* pPlayer, int MailID)
{
	const int ClientID = pPlayer->GetCID();
	const int AccountID = pPlayer->Account()->GetID();
	if(s_aClaiming[ClientID])
		return;
	s_aClaiming[ClientID] = true;
	ResetMailMenus(ClientID);

	auto pContext = DbAsync::MakeContext(ClientID);
	MailDb::ReadAttachedItems(MailID, AccountID).Then([pContext, ClientID, AccountID, MailID](Db::CResultSet&& Rows)
	{
		auto* pPlayer = GetRequestPlayer(pContext, AccountID);
		if(!pPlayer)
		{
			s_aClaiming[ClientID] = false;
			return;
		}

		if(!Rows.Ok() || !Rows.HasRows())
		{
			pContext->GS()->Chat(ClientID, "Unable to claim this mail.");
			s_aClaiming[ClientID] = false;
			return;
		}

		CItemsContainer vItems = GetAttachedItems(Rows);
		CItemsContainer vAccepted {}, vRejected {};
		for(const auto& Item : vItems)
		{
			CPlayerItem* pPlayerItem = pPlayer->GetItem(Item);
			if(!pPlayerItem || (pPlayerItem->HasItem() && !Item.Info()->IsStackable()))
				vRejected.push_back(Item);
			else
				vAccepted.push_back(Item);
		}

		if(!vItems.empty() && vAccepted.empty())
		{
			pContext->GS()->Chat(ClientID, "Can't claim: no free slot for attached items.");
			s_aClaiming[ClientID] = false;
			return;
		}

		// The delete is the claim. Only the request that actually removes the row
		// grants items, so a second click or another client cannot claim twice.
		MailDb::DeleteMail(MailID, AccountID).Then([pContext, ClientID, AccountID, vAccepted, vRejected](Db::CResultSet&& Deleted) mutable
		{
			s_aClaiming[ClientID] = false;
			CPlayer* pPlayer = GetRequestPlayer(pContext, AccountID);
			if(!pPlayer)
				return;

			if(!Deleted.Ok() || !Deleted.Changed())
			{
				pContext->GS()->Chat(ClientID, "Unable to claim this mail.");
				RefreshMailbox(pPlayer);
				return;
			}

			for(const auto& Item : vAccepted)
			{
				CPlayerItem* pPlayerItem = pPlayer->GetItem(Item);
				if(!pPlayerItem)
					continue;
				pPlayerItem->Add(Item.GetValue(), 0, Item.GetEnchant(), Item.GetExpiresAt(), true, Item.GetStringData());
				pContext->GS()->Chat(ClientID, "Item received: {}.", Item.Info()->GetName());
			}

			if(!vRejected.empty())
			{
				MailWrapper Mail("System", AccountID, "Could not claim all items");
				Mail.AddDescLine("Some items were already owned.");
				for(const auto& Item : vRejected)
					Mail.AttachItem(Item);
				Mail.Send();
			}

			RefreshMailbox(pPlayer);
		});
	});
}
