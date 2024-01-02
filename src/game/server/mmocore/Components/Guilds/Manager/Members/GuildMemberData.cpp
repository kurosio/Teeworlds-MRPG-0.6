/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildMemberData.h"

#include <game/server/gamecontext.h>

#include "game/server/mmocore/Components/Guilds/GuildData.h"

CGS* CGuildMemberData::GS() const
{
	return m_pGuild->GS();
}

CGuildMemberData::CGuildMemberData(CGuildData* pGuild, int AccountID, CGuildRankData* pRank, int Deposit) : m_pGuild(pGuild), m_AccountID(AccountID)
{
	m_pRank = pRank == nullptr ? pGuild->GetRanks()->GetDefaultRank() : pRank;
	m_Deposit = Deposit;
}

CGuildMemberData::~CGuildMemberData()
{
	if(CPlayer* pPlayer = GS()->GetPlayerByUserID(m_AccountID))
	{
		pPlayer->Account()->ReinitializeGuild(true);
		GS()->UpdateVotes(pPlayer->GetCID(), MENU_MAIN);
	}
}

CGuildRankData* CGuildMemberData::GetRank() const
{
	return m_pRank;
}

bool CGuildMemberData::SetRank(GuildRankIdentifier RankID)
{
	CGuildRankData* pRank = m_pGuild->GetRanks()->Get(RankID);
	if(!pRank)
		return false;

	SetRank(pRank);
	return true;
}

bool CGuildMemberData::SetRank(CGuildRankData* pRank)
{
	if(!pRank)
		return false;

	m_pRank = pRank;
	m_pGuild->GetMembers()->Save();
	return true;
}

bool CGuildMemberData::DepositInBank(int Golds)
{
	CPlayer* pPlayer = GS()->GetPlayerByUserID(m_AccountID);
	if(!pPlayer)
		return false;

	ResultPtr pRes = Database->Execute<DB::SELECT>("Bank", TW_GUILD_TABLE, "WHERE ID = '%d'", m_pGuild->GetID());
	if(pRes->next())
	{
		if(pPlayer->Account()->SpendCurrency(Golds))
		{
			m_Deposit += Golds;
			m_pGuild->GetBank()->Set(pRes->getInt("Bank") + Golds);
			Database->Execute<DB::UPDATE>(TW_GUILD_TABLE, "Bank = '%d' WHERE ID = '%d'", m_pGuild->GetBank()->Get(), m_pGuild->GetID());

			int ClientID = pPlayer->GetCID();
			GS()->Chat(ClientID, "You put {VAL} gold in the safe, now {VAL}!", Golds, m_pGuild->GetBank()->Get());
			m_pGuild->GetMembers()->Save();
			return true;
		}
	}

	return false;
}

bool CGuildMemberData::WithdrawFromBank(int Golds)
{
	CPlayer* pPlayer = GS()->GetPlayerByUserID(m_AccountID);
	if(!pPlayer)
		return false;

	ResultPtr pRes = Database->Execute<DB::SELECT>("Bank", TW_GUILD_TABLE, "WHERE ID = '%d'", m_pGuild->GetID());
	if(pRes->next())
	{
		int ClientID = pPlayer->GetCID();
		int Bank = pRes->getInt("Bank");

		Golds = minimum(Golds, Bank);
		if(Golds > 0)
		{
			m_Deposit -= Golds;
			pPlayer->Account()->AddGold(Golds);
			m_pGuild->GetBank()->Set(Bank - Golds);

			GS()->Chat(ClientID, "You take {VAL} gold in the safe {VAL}!", Golds, m_pGuild->GetBank()->Get());
			m_pGuild->GetMembers()->Save();
			return true;
		}
	}

	return false;
}


