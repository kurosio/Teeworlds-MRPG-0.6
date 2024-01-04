/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildMemberData.h"

#include <game/server/gamecontext.h>

#include "game/server/mmocore/Components/Guilds/GuildData.h"

// Get the GuildScore object of the guild that the member belongs to
CGS* CGuildMemberData::GS() const { return m_pGuild->GS(); }

// Constructor for CGuildMemberData
CGuildMemberData::CGuildMemberData(CGuildData* pGuild, int AccountID, CGuildRankData* pRank, int Deposit) : m_pGuild(pGuild)
{
	m_AccountID = AccountID;
	m_Deposit = Deposit;

	// If the given rank is null, set the member's rank to the default rank of the guild
	m_pRank = pRank == nullptr ? pGuild->GetRanks()->GetDefaultRank() : pRank;
}

// Destructor for CGuildMemberData
CGuildMemberData::~CGuildMemberData()
{
	// Reinitialize the guild for the player's account
	if(CPlayer* pPlayer = GS()->GetPlayerByUserID(m_AccountID))
	{
		pPlayer->Account()->ReinitializeGuild(true);
		GS()->UpdateVotes(pPlayer->GetCID(), MENU_MAIN);
	}
}

// Set the rank of the member by rank ID
bool CGuildMemberData::SetRank(GuildRankIdentifier RankID)
{
	// Get the rank object from the guild's ranks using the given rank ID
	CGuildRankData* pRank = m_pGuild->GetRanks()->Get(RankID);
	if(!pRank)
		return false;

	// Set the member's rank to the given rank
	SetRank(pRank);
	return true;
}

// Set the rank of the member
// This function sets the rank of a guild member
bool CGuildMemberData::SetRank(CGuildRankData* pRank)
{
	// Check if the rank is valid
	if(!pRank)
		return false;

	// Set the member's rank
	m_pRank = pRank;

	// Save the member data and add a history entry for the rank change
	m_pGuild->GetMembers()->Save();
	m_pGuild->GetHistory()->Add("%s rank changed to %s", Instance::GetServer()->GetAccountNickname(m_AccountID), m_pRank->GetName());
	return true;
}

// Deposit gold in the guild bank
bool CGuildMemberData::DepositInBank(int Golds)
{
	// Get the player object of the member
	CPlayer* pPlayer = GS()->GetPlayerByUserID(m_AccountID);
	if(!pPlayer)
		return false;

	// Get the bank value from the guild table in the database
	ResultPtr pRes = Database->Execute<DB::SELECT>("Bank", TW_GUILDS_TABLE, "WHERE ID = '%d'", m_pGuild->GetID());
	if(pRes->next())
	{
		// If the player has enough gold to deposit
		if(pPlayer->Account()->SpendCurrency(Golds))
		{
			// Increase the member's deposit and the guild bank value
			m_Deposit += Golds;
			m_pGuild->GetBank()->Set(pRes->getInt("Bank") + Golds);
			Database->Execute<DB::UPDATE>(TW_GUILDS_TABLE, "Bank = '%d' WHERE ID = '%d'", m_pGuild->GetBank()->Get(), m_pGuild->GetID());

			// Send a chat message to the player indicating the successful deposit and the new bank value
			int ClientID = pPlayer->GetCID();
			GS()->Chat(ClientID, "You put {VAL} gold in the safe, now {VAL}!", Golds, m_pGuild->GetBank()->Get());
			m_pGuild->GetMembers()->Save();
			return true;
		}
	}

	return false;
}

// Withdraw gold from the guild bank
bool CGuildMemberData::WithdrawFromBank(int Golds)
{
	// Get the player object of the member
	CPlayer* pPlayer = GS()->GetPlayerByUserID(m_AccountID);
	if(!pPlayer)
		return false;

	// Get the bank value from the guild table in the database
	ResultPtr pRes = Database->Execute<DB::SELECT>("Bank", TW_GUILDS_TABLE, "WHERE ID = '%d'", m_pGuild->GetID());
	if(pRes->next())
	{
		int ClientID = pPlayer->GetCID();
		int Bank = pRes->getInt("Bank");

		// Make sure the requested withdrawal amount is not greater than the available bank value
		Golds = minimum(Golds, Bank);
		if(Golds > 0)
		{
			// Decrease the member's deposit, add the withdrawn gold to the player's account, and decrease the guild bank value
			m_Deposit -= Golds;
			pPlayer->Account()->AddGold(Golds);
			m_pGuild->GetBank()->Set(Bank - Golds);

			// Send a chat message to the player indicating the successful withdrawal and the new bank value
			GS()->Chat(ClientID, "You take {VAL} gold in the safe {VAL}!", Golds, m_pGuild->GetBank()->Get());
			m_pGuild->GetMembers()->Save();
			return true;
		}
	}

	return false;
}

// Check if a member has the required access level
bool CGuildMemberData::CheckAccess(GuildRankAccess RequiredAccess) const
{
	return (m_pGuild->GetLeaderUID() == m_AccountID || m_pRank->GetAccess() == RequiredAccess
		|| (m_pRank->GetAccess() == ACCESS_FULL && RequiredAccess != ACCESS_LEADER));
}
