/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildRankData.h"

#include <game/server/gamecontext.h>

#include <game/server/mmocore/Components/Guilds/GuildData.h>

CGS* CGuildRankData::GS() const
{
	return m_pGuild->GS();
}

CGuildRankData::CGuildRankData(GuildRankIdentifier RID, std::string&& Rank, GuildRankAccess Access, CGuildData* pGuild) : m_ID(RID), m_Rank(std::move(Rank))
{
	m_Access = Access;
	m_pGuild = pGuild;
}

GUILD_RANK_RESULT CGuildRankData::ChangeName(std::string NewRank)
{
	auto cstrNewRank = CSqlString<64>(NewRank.c_str());

	// check already used name
	if(std::count_if(m_pGuild->GetRanks()->GetContainer().begin(), m_pGuild->GetRanks()->GetContainer().end(),
		[&cstrNewRank](const CGuildRankData* pRank) {return str_comp(pRank->m_Rank.c_str(), cstrNewRank.cstr()) == 0; }))
	{
		return GUILD_RANK_RESULT::RENAME_ALREADY_NAME_EXISTS;
	}

	// update name
	m_pGuild->GetHistory()->Add("renamed rank '%s' to '%s'", m_Rank.c_str(), cstrNewRank.cstr());
	Database->Execute<DB::UPDATE>(TW_GUILDS_RANKS_TABLE, "Name = '%s' WHERE ID = '%d'", cstrNewRank.cstr(), m_ID);
	m_Rank = cstrNewRank.cstr();
	return GUILD_RANK_RESULT::SUCCESSFUL;
}

void CGuildRankData::ChangeAccess()
{
	if(m_Access >= RIGHTS_FULL)
		SetAccess(RIGHTS_DEFAULT);
	else
		SetAccess((GuildRankAccess)(m_Access + 1));
}

void CGuildRankData::SetAccess(GuildRankAccess Access)
{
	// set access
	m_Access = (GuildRankAccess)clamp((int)Access, (int)RIGHTS_DEFAULT, (int)RIGHTS_FULL);

	// save
	GuildIdentifier GuildID = m_pGuild->GetID();
	Database->Execute<DB::UPDATE>(TW_GUILDS_RANKS_TABLE, "Access = '%d' WHERE ID = '%d'", m_Access, m_ID);
	GS()->ChatGuild(GuildID, "Rank '{STR}' new rights '{STR}'!", m_Rank.c_str(), GetAccessName());
}

const char* CGuildRankData::GetAccessName() const
{
	if(m_Access == RIGHTS_INVITE_KICK)
		return "Invite & kick";
	if(m_Access == RIGHTS_UPGRADES_HOUSE)
		return "Upgrade & house door's";
	if(m_Access == RIGHTS_FULL)
		return "Full";
	return "Default";
}