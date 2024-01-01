/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildMemberData.h"

#include <game/server/gamecontext.h>

#include "game/server/mmocore/Components/Guilds/GuildData.h"

CGuildMemberData::CGuildMemberData(CGuildData* pGuild, int AccountID, CGuildRankData* pRank, int Deposit) : m_pGuild(pGuild), m_AccountID(AccountID)
{
	m_pRank = pRank == nullptr ? pGuild->GetRanks()->GetDefaultRank() : pRank;
	m_Deposit = Deposit;
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


