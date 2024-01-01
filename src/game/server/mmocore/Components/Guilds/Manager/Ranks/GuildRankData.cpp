/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildRankData.h"

#include <game/server/gamecontext.h>

#include <game/server/mmocore/Components/Guilds/GuildData.h>

CGS* CGuildRankData::GS() const
{
	return m_pGuild->GS();
}

CGuildRankData::CGuildRankData(GuildRankIdentifier RID, std::string&& Rank, int Access, CGuildData* pGuild) : m_ID(RID), m_Rank(std::move(Rank)), m_Access(Access)
{
	m_pGuild = pGuild;
}

GUILD_RANK_RESULT CGuildRankData::ChangeName(std::string NewRank)
{
	GuildIdentifier GuildID = m_pGuild->GetID();
	if(std::count_if(m_pGuild->GetRanks()->GetContainer().begin(), m_pGuild->GetRanks()->GetContainer().end(), [&NewRank](const CGuildRankData* pRank) {return pRank->m_Rank == NewRank; }))
	{
		return GUILD_RANK_RESULT::RENAME_ALREADY_NAME_EXISTS;
	}

	CSqlString<64> cNewRank = CSqlString<64>(NewRank.c_str());
	Database->Execute<DB::UPDATE>(TW_GUILDS_RANKS_TABLE, "Name = '%s' WHERE ID = '%d' AND GuildID = '%d'", cNewRank.cstr(), m_ID, GuildID);
	m_Rank = NewRank;
	return GUILD_RANK_RESULT::SUCCESSFUL;
}

void CGuildRankData::ChangeAccess()
{
	m_Access++;
	if(m_Access >= RIGHTS_NUM)
	{
		m_Access = RIGHTS_DEFAULT;
	}

	GuildIdentifier GuildID = m_pGuild->GetID();
	Database->Execute<DB::UPDATE>(TW_GUILDS_RANKS_TABLE, "Access = '%d' WHERE ID = '%d' AND GuildID = '%d'", m_Access, m_ID, GuildID);

	GS()->ChatGuild(GuildID, "Rank '{STR}' new rights '{STR}'!", m_Rank.c_str(), GetAccessName());
}

bool CGuildRankData::CheckAccess(CPlayer* pPlayer, GuildRankAccess Access) const
{
	if(!pPlayer || !pPlayer->IsAuthed())
		return false;

	if(m_pGuild->GetOwnerUID() == pPlayer->Account()->GetID())
		return true;

	return m_Access & Access;
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
