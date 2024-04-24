/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "MailBoxManager.h"

#include <game/server/gamecontext.h>

bool CMailboxManager::OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, int VoteID, int VoteID2, int Get, const char* GetText)
{
	if(PPSTR(CMD, "MAIL") == 0)
	{
		AcceptLetter(pPlayer, VoteID);
		pPlayer->m_VotesData.UpdateVotesIf(MENU_INBOX);
		return true;
	}

	if(PPSTR(CMD, "DELETE_MAIL") == 0)
	{
		DeleteLetter(VoteID);
		pPlayer->m_VotesData.UpdateVotesIf(MENU_INBOX);
		return true;
	}

	return false;
}

bool CMailboxManager::OnHandleMenulist(CPlayer* pPlayer, int Menulist)
{
	const int ClientID = pPlayer->GetCID();

	if(Menulist == MenuList::MENU_INBOX)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);

		CVoteWrapper::AddBackpage(ClientID);
		CVoteWrapper::AddLine(ClientID);
		ShowMailboxMenu(pPlayer);
		return true;
	}

	return false;
}

// check whether messages are available
int CMailboxManager::GetLettersCount(int AccountID)
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("ID", "tw_accounts_mailbox", "WHERE UserID = '%d'", AccountID);
	const int MailValue = pRes->rowsCount();
	return MailValue;
}

// show a list of mails
void CMailboxManager::ShowMailboxMenu(CPlayer *pPlayer)
{
	int LetterPos = 0;
	bool EmptyMailBox = true;
	const int ClientID = pPlayer->GetCID();

	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_accounts_mailbox", "WHERE UserID = '%d' LIMIT %d", pPlayer->Account()->GetID(), MAILLETTER_MAX_CAPACITY);
	while(pRes->next())
	{
		// get the information to create an object
		const int MailLetterID = pRes->getInt("ID");
		const int ItemID = pRes->getInt("ItemID");
		const int Value = pRes->getInt("ItemValue");
		const int Enchant = pRes->getInt("Enchant");
		const char* pDescription = pRes->getString("Description").c_str();
		EmptyMailBox = false;
		LetterPos++;

		// add vote menu
		CItem AttachedItem(ItemID, Value);
		CVoteWrapper VLetter(ClientID, VWF_UNIQUE|VWF_STYLE_SIMPLE, "{}. {}", LetterPos, pRes->getString("Name").c_str());
		VLetter.Add(pDescription);

		if(!AttachedItem.IsValid())
			VLetter.AddOption("MAIL", MailLetterID, "Accept");
		else if(AttachedItem.Info()->IsEnchantable())
			VLetter.AddOption("MAIL", MailLetterID, "Receive {} {}", AttachedItem.Info()->GetName(), AttachedItem.Info()->StringEnchantLevel(Enchant).c_str());
		else
			VLetter.AddOption("MAIL", MailLetterID, "Receive {}x{c}", AttachedItem.Info()->GetName(), Value);

		VLetter.AddOption("DELETE_MAIL", MailLetterID, "Delete");
	}

	if(EmptyMailBox)
	{
		CVoteWrapper(ClientID).Add("Your mailbox is empty");
	}
}

// sending a mail to a player
void CMailboxManager::SendInbox(const char* pFrom, int AccountID, const char* pName, const char* pDesc, int ItemID, int Value, int Enchant)
{
	// clear str and connection
	const CSqlString<64> cName = CSqlString<64>(pName);
	const CSqlString<64> cDesc = CSqlString<64>(pDesc);
	const CSqlString<64> cFrom = CSqlString<64>(pFrom);

	// send information about new message
	const bool LocalMsg = GS()->ChatAccount(AccountID, "[Mailbox] New letter ({})!", cName.cstr());
	if(LocalMsg)
	{
		const int LetterCount = GetLettersCount(AccountID);
		if(LetterCount >= (int)MAILLETTER_MAX_CAPACITY)
		{
			GS()->ChatAccount(AccountID, "[Mailbox] Your mailbox is full you can't get.");
			GS()->ChatAccount(AccountID, "[Mailbox] It will come after you clear your mailbox.");
		}
	}

	// attach item
	CItem AttachedItem(ItemID, Value, Enchant);
	if(AttachedItem.IsValid())
	{
		Database->Execute<DB::INSERT>("tw_accounts_mailbox", "(Name, Description, ItemID, ItemValue, Enchant, UserID, FromSend) VALUES ('%s', '%s', '%d', '%d', '%d', '%d', '%s');",
			cName.cstr(), cDesc.cstr(), AttachedItem.GetID(), AttachedItem.GetValue(), AttachedItem.GetEnchant(), AccountID, cFrom.cstr());
	}
	else
	{
		Database->Execute<DB::INSERT>("tw_accounts_mailbox", "(Name, Description, UserID, FromSend) VALUES ('%s', '%s', '%d', '%s');", cName.cstr(), cDesc.cstr(), AccountID, cFrom.cstr());
	}
}

bool CMailboxManager::SendInbox(const char* pFrom, const char* pNickname, const char* pName, const char* pDesc, int ItemID, int Value, int Enchant)
{
	const CSqlString<64> cName = CSqlString<64>(pNickname);
	ResultPtr pRes = Database->Execute<DB::SELECT>("ID, Nick", "tw_accounts_data", "WHERE Nick = '%s'", cName.cstr());
	if(pRes->next())
	{
		const int AccountID = pRes->getInt("ID");
		SendInbox(pFrom, AccountID, pName, pDesc, ItemID, Value, Enchant);
		return true;
	}
	return false;
}

void CMailboxManager::AcceptLetter(CPlayer* pPlayer, int LetterID)
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("ItemID, ItemValue, Enchant", "tw_accounts_mailbox", "WHERE ID = '%d'", LetterID);
	if(!pRes->next())
		return;

	// get informed about the mail
	ItemIdentifier ItemID = pRes->getInt("ItemID");
	int Value = pRes->getInt("ItemValue");
	CItem AttachedItem(ItemID, Value);

	// attached valid item
	if(AttachedItem.IsValid())
	{
		// check enchanted item
		CPlayerItem* pItem = pPlayer->GetItem(AttachedItem);
		if(pItem->Info()->IsEnchantable() && pItem->HasItem())
		{
			GS()->Chat(pPlayer->GetCID(), "Enchant item maximal count x1 in a backpack!");
			return;
		}

		// recieve
		const int Enchant = pRes->getInt("Enchant");
		pItem->Add(Value, 0, Enchant);
		GS()->Chat(pPlayer->GetCID(), "You received an attached item [{}].", GS()->GetItemInfo(ItemID)->GetName());
		DeleteLetter(LetterID);
		return;
	}

	// is empty letter
	DeleteLetter(LetterID);
}

void CMailboxManager::DeleteLetter(int LetterID)
{
	Database->Execute<DB::REMOVE>("tw_accounts_mailbox", "WHERE ID = '%d'", LetterID);
}