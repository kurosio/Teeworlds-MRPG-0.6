/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildRankData.h"

#include "GuildData.h"

CGuildRankData::CGuildRankData(GuildRankIdentifier RID, std::string&& Rank, int Access, CGuildData* pGuild) : m_ID(RID), m_Rank(std::move(Rank)), m_Access(Access)
{
	m_pGuild = pGuild;
}

void CGuildRankData::ChangeName(std::string NewRank)
{
	GuildIdentifier GuildID = m_pGuild->GetID();
	if(std::count_if(m_pGuild->GetRanks().begin(), m_pGuild->GetRanks().end(), [&NewRank](const CGuildRankData* pRank) {return pRank->m_Rank == NewRank; }))
	{
		GS()->ChatGuild(GuildID, "A rank with that name is already used");
		return;
	}

	CSqlString<64> cNewRank = CSqlString<64>(NewRank);
	Database->Execute<DB::UPDATE>("tw_guilds_ranks", "Name = '%s' WHERE ID = '%d' AND GuildID = '%d'", cNewRank.cstr(), m_ID, GuildID);
	m_Rank = NewRank;
}

void CGuildRankData::ChangeAccess(int Access)
{
	// change access rank
	m_Access = Access;

	GuildIdentifier GuildID = m_pGuild->GetID();
	Database->Execute<DB::UPDATE>("tw_guilds_ranks", "Access = '%d' WHERE ID = '%d' AND GuildID = '%d'", m_Access, m_ID, GuildID);
	GS()->ChatGuild(GuildID, "Rank [{STR}] changes [{STR}]!", m_Rank, GetAccessName());
}

const char* CGuildRankData::GetAccessName()
{
	switch(m_Access)
	{
		default: return "No Access";
		case ACCESS_INVITE_KICK: return "Access Invite Kick";
		case ACCESS_UPGRADE_HOUSE: return "Access Upgrades & House";
		case ACCESS_FULL: return "Full Access";
	}
}
