/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildRankData.h"

#include <game/server/gamecontext.h>

#include <game/server/mmocore/Components/Guilds/GuildData.h>

CGS* CGuildRankData::GS() const { return m_pGuild->GS(); }

// This is the constructor of the CGuildRankData class
CGuildRankData::CGuildRankData(GuildRankIdentifier RID, std::string&& Rank, GuildRankAccess Access, CGuildData* pGuild) : m_ID(RID), m_Rank(std::move(Rank))
{
	m_Access = Access;
	m_pGuild = pGuild;
}

// This function changes the name of the guild rank
GUILD_RANK_RESULT CGuildRankData::Rename(std::string NewRank)
{
	// Create a CSqlString object from the NewRank string
	auto cstrNewRank = CSqlString<64>(NewRank.c_str());

	// Check if the length of the string is less than 2 or greater than 16
	const int LengthRank = str_length(cstrNewRank.cstr());
	if(LengthRank < 2 || LengthRank > MAX_NAME_LENGTH)
	{
		// If the length is not within the valid range, return the result WRONG_NUMBER_OF_CHAR_IN_NAME
		return GUILD_RANK_RESULT::WRONG_NUMBER_OF_CHAR_IN_NAME;
	}

	// Check if the new rank name is already used by another rank in the guild
	if(std::count_if(m_pGuild->GetRanks()->GetContainer().begin(), m_pGuild->GetRanks()->GetContainer().end(),
		[&cstrNewRank](const CGuildRankData* pRank) {return str_comp(pRank->m_Rank.c_str(), cstrNewRank.cstr()) == 0; }))
	{
		return GUILD_RANK_RESULT::RENAME_ALREADY_NAME_EXISTS;
	}

	// Update the name of the guild rank
	m_pGuild->GetLogger()->Add(LOGFLAG_RANKS_CHANGES, "renamed rank '%s' to '%s'", m_Rank.c_str(), cstrNewRank.cstr());
	Database->Execute<DB::UPDATE>(TW_GUILDS_RANKS_TABLE, "Name = '%s' WHERE ID = '%d'", cstrNewRank.cstr(), m_ID);
	m_Rank = cstrNewRank.cstr();
	return GUILD_RANK_RESULT::SUCCESSFUL;
}

// This function changes the access level of the guild rank
void CGuildRankData::ChangeAccess()
{
	if(m_Access >= RIGHTS_FULL)
		SetAccess(RIGHTS_DEFAULT);
	else
		SetAccess((GuildRankAccess)(m_Access + 1));
}

// This function sets the access level of the guild rank
void CGuildRankData::SetAccess(GuildRankAccess Access)
{
	// Set the access level, clamping it between RIGHTS_DEFAULT and RIGHTS_FULL
	m_Access = (GuildRankAccess)clamp((int)Access, (int)RIGHTS_DEFAULT, (int)RIGHTS_FULL);

	// Save the updated access level in the database
	GuildIdentifier GuildID = m_pGuild->GetID();
	Database->Execute<DB::UPDATE>(TW_GUILDS_RANKS_TABLE, "Access = '%d' WHERE ID = '%d'", m_Access, m_ID);

	// Send a chat message to the guild with the updated access level
	GS()->ChatGuild(GuildID, "Rank '{STR}' new rights '{STR}'!", m_Rank.c_str(), GetAccessName());
}

// This function returns the name of the access level of the guild rank
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