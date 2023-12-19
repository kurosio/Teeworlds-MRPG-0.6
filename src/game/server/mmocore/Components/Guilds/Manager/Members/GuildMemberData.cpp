/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildMemberData.h"

#include <game/server/gamecontext.h>

CGuildMemberData::CGuildMemberData(CGuildData* pGuild, int AccountID, GuildRankIdentifier RankID, int Deposit) : m_pGuild(pGuild), m_AccountID(AccountID)
{
	m_RankID = RankID;
	m_Deposit = Deposit;
}
