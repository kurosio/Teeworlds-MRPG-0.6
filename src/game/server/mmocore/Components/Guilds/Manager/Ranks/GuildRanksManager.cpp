/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildRanksManager.h"

#include <game/server/gamecontext.h>
#include "GuildRankData.h"

#include <game/server/mmocore/Components/Guilds/GuildData.h>

CGS* CGuildRanksManager::GS() const { return m_pGuild->GS(); }

// Constructor for CGuildRanksManager
CGuildRanksManager::CGuildRanksManager(CGuildData* pGuild, GuildRankIdentifier DefaultID)
	: m_pGuild(pGuild)
{
	// Initialize the guild ranks controller
	CGuildRanksManager::Init(DefaultID);
}

// Destructor for CGuildRanksManager
CGuildRanksManager::~CGuildRanksManager()
{
	// Delete all the rank data objects
	for(auto p : m_aRanks)
		delete p;

	// Clear the default rank pointer and the ranks vector
	m_pDefaultRank = nullptr;
	m_aRanks.clear();
}

// Initialize the guild ranks controller
void CGuildRanksManager::Init(GuildRankIdentifier DefaultID)
{
	// Execute a database query to get the rank data for the guild
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_guilds_ranks", "WHERE GuildID = '%d'", m_pGuild->GetID());
	while(pRes->next())
	{
		// Get the rank ID, name, and access level from the database query result
		GuildRankIdentifier RID = pRes->getInt("ID");
		std::string Rank = pRes->getString("Name").c_str();
		GuildRankAccess Access = (GuildRankAccess)pRes->getInt("Access");

		// Create a new CGuildRankData object and add it to the ranks vector
		if(DefaultID == RID)
		{
			m_pDefaultRank = m_aRanks.emplace_back(new CGuildRankData(RID, std::forward<std::string>(Rank), RIGHTS_DEFAULT, m_pGuild));
		}
		else
		{
			m_aRanks.emplace_back(new CGuildRankData(RID, std::forward<std::string>(Rank), Access, m_pGuild));
		}
	}
}

// Update the default rank for the guild
void CGuildRanksManager::UpdateDefaultRank()
{
	// If the default rank already exists, return
	if(m_pDefaultRank)
		return;

	// If there are no ranks, create a default rank called "Member"
	if(!m_aRanks.empty())
	{
		m_pDefaultRank = m_aRanks.back();
		m_pDefaultRank->SetAccess(RIGHTS_DEFAULT);
	}
	else
	{
		GUILD_RANK_RESULT Result = Add("Newbie");
		dbg_assert(Result == GUILD_RANK_RESULT::SUCCESSFUL, "guild cannot initialize a default rank");
		m_pDefaultRank = Get("Newbie");
	}

	// Set the default rank for all guild members who do not have a rank
	for(auto& pIterMember : m_pGuild->GetMembers()->GetContainer())
	{
		CGuildMemberData* pMember = pIterMember.second;
		if(!pMember->GetRank())
		{
			bool Status = pMember->SetRank(m_pDefaultRank);
			dbg_assert(Status, "guild cannot set a default rank for member");
		}
	}

	// Save the guild members
	m_pGuild->GetMembers()->Save();
}

// Add a new rank to the guild
GUILD_RANK_RESULT CGuildRanksManager::Add(const std::string& Rank)
{
	// Create a CSqlString object for the rank name
	auto cstrRank = CSqlString<64>(Rank.c_str());

	// Check if the length of the string is less than 2 or greater than 16
	const int LengthRank = str_length(cstrRank.cstr());
	if(LengthRank < 2 || LengthRank > MAX_NAME_LENGTH)
	{
		// If the length is not within the valid range, return the result WRONG_NUMBER_OF_CHAR_IN_NAME
		return GUILD_RANK_RESULT::WRONG_NUMBER_OF_CHAR_IN_NAME;
	}

	// Check if the rank already exists
	if(std::count_if(m_aRanks.begin(), m_aRanks.end(), [&cstrRank](const CGuildRankData* pRank) { return std::string(pRank->GetName()) == cstrRank.cstr(); }))
	{
		return GUILD_RANK_RESULT::ADD_ALREADY_EXISTS;
	}

	// Check if the rank count has reached the limit
	if((int)m_aRanks.size() >= MAX_GUILD_RANK_NUM)
	{
		return GUILD_RANK_RESULT::ADD_LIMIT_HAS_REACHED;
	}

	// Get the ID for the new rank
	ResultPtr pResID = Database->Execute<DB::SELECT>("ID", "tw_guilds_ranks", "ORDER BY ID DESC LIMIT 1");
	const int InitID = pResID->next() ? pResID->getInt("ID") + 1 : 1;

	// Insert the new rank into the database
	GuildIdentifier GuildID = m_pGuild->GetID();
	Database->Execute<DB::INSERT>("tw_guilds_ranks", "(ID, Access, GuildID, Name) VALUES ('%d', '%d', '%d', '%s')", InitID, (int)RIGHTS_DEFAULT, GuildID, cstrRank.cstr());
	m_aRanks.emplace_back(new CGuildRankData(InitID, cstrRank.cstr(), RIGHTS_DEFAULT, m_pGuild));

	// Send information to the game server and update the guild history
	GS()->ChatGuild(GuildID, "New rank is created [{STR}]!", cstrRank.cstr());
	m_pGuild->GetLogger()->Add("added rank '%s'", cstrRank.cstr());
	return GUILD_RANK_RESULT::SUCCESSFUL;
}

// Remove a rank from the guild
GUILD_RANK_RESULT CGuildRanksManager::Remove(const std::string& Rank)
{
	// Create a CSqlString object for the rank name
	auto cstrRank = CSqlString<64>(Rank.c_str());

	// Find the rank in the ranks vector
	auto Iter = std::find_if(m_aRanks.begin(), m_aRanks.end(), [&cstrRank](const CGuildRankData* pRank)
	{
		return str_comp(pRank->GetName(), cstrRank.cstr()) == 0;
	});

	// If the rank is the default rank, return
	if(*Iter == m_pDefaultRank)
	{
		return GUILD_RANK_RESULT::REMOVE_RANK_IS_DEFAULT;
	}

	// If the rank does not exist, return
	if(Iter == m_aRanks.end())
	{
		return GUILD_RANK_RESULT::REMOVE_RANK_DOES_NOT_EXIST;
	}

	// Set the default rank for all guild members who have the rank being removed
	for(auto& pMember : m_pGuild->GetMembers()->GetContainer())
	{
		if((*Iter)->GetID() == pMember.second->GetRank()->GetID())
		{
			bool Status = pMember.second->SetRank(m_pDefaultRank->GetID());
			dbg_assert(Status, "guild cannot set a default rank for member");
		}
	}

	// Remove the rank from the database and delete the rank data object
	Database->Execute<DB::REMOVE>("tw_guilds_ranks", "WHERE ID = '%d'", (*Iter)->GetID());
	delete (*Iter);
	m_aRanks.erase(Iter);

	// Send information to the game server and update the guild history
	GS()->ChatGuild(m_pGuild->GetID(), "Rank [{STR}] succesful delete", cstrRank.cstr());
	m_pGuild->GetLogger()->Add("removed rank '%s'", cstrRank.cstr());
	return GUILD_RANK_RESULT::SUCCESSFUL;
}

// Get a rank by its name
CGuildRankData* CGuildRanksManager::Get(const std::string& Rank) const
{
	auto Iter = std::find_if(m_aRanks.begin(), m_aRanks.end(), [&Rank](const CGuildRankData* pRank){ return pRank->GetName() == Rank; });
	return Iter != m_aRanks.end() ? *Iter : nullptr;
}

// Get a rank by its ID
CGuildRankData* CGuildRanksManager::Get(GuildRankIdentifier ID) const
{
	auto Iter = std::find_if(m_aRanks.begin(), m_aRanks.end(), [ID](const CGuildRankData* pRank){ return pRank->GetID() == ID; });
	return Iter != m_aRanks.end() ? *Iter : nullptr;
}
