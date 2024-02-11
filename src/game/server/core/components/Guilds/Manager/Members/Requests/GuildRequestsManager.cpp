/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildRequestsManager.h"

#include <game/server/core/components/Guilds/GuildData.h>
#include <game/server/gamecontext.h>

// Get the game server
CGS* CGuildRequestsManager::GS() const { return m_pGuild->GS(); }

// Constructor
CGuildRequestsManager::CGuildRequestsManager(CGuildData* pGuild) : m_pGuild(pGuild)
{
	CGuildRequestsManager::Init();
}

// Destructor
CGuildRequestsManager::~CGuildRequestsManager()
{
	for(auto p : m_aRequestsJoin)
		delete p;

	m_aRequestsJoin.clear();
}

void CGuildRequestsManager::Init()
{
	// Execute a database query to get the rank data for the guild
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", TW_GUILDS_INVITES_TABLE, "WHERE GuildID = '%d'", m_pGuild->GetID());
	while(pRes->next())
	{
		// Get the rank ID, name, and access level from the database query result
		int AccountID = pRes->getInt("UserID");
		m_aRequestsJoin.push_back(new CGuildRequestData(AccountID));
	}
}

GUILD_MEMBER_RESULT CGuildRequestsManager::Request(int FromUID)
{
	// Check if the invite already exists in the guild's invites container
	if(std::find_if(m_aRequestsJoin.begin(), m_aRequestsJoin.end(),
		[&FromUID](const CGuildRequestData* p){ return p->GetFromUID() == FromUID; }) != m_aRequestsJoin.end())
		return GUILD_MEMBER_RESULT::REQUEST_ALREADY_SEND;

	// Check if the guild's member list has free slots
	if(!m_pGuild->GetMembers()->HasFreeSlots())
	{
		return GUILD_MEMBER_RESULT::NO_AVAILABLE_SLOTS;
	}

	// Add the invite to the guild's history and send a chat message
	const char* pFromNickname = Instance::Server()->GetAccountNickname(FromUID);
	m_pGuild->GetLogger()->Add(LOGFLAG_MEMBERS_CHANGES, "invitation to join from '%s'.", pFromNickname);
	GS()->ChatGuild(m_pGuild->GetID(), "invitation to join from '{STR}'.", pFromNickname);

	// Create a new invite data object and add it to the guild's invites container
	m_aRequestsJoin.push_back(new CGuildRequestData(FromUID));

	// Insert the invite into the database
	Database->Execute<DB::INSERT>(TW_GUILDS_INVITES_TABLE, "(GuildID, UserID) VALUES ('%d', '%d')", m_pGuild->GetID(), FromUID);
	return GUILD_MEMBER_RESULT::SUCCESSFUL;
}

GUILD_MEMBER_RESULT CGuildRequestsManager::Accept(int UserID, const CGuildMemberData* pFromMember)
{
	// Find the request with the given UserID in the m_aRequestsJoin vector
	auto Iter = std::find_if(m_aRequestsJoin.begin(), m_aRequestsJoin.end(), [&UserID](const CGuildRequestData* pRequest)
	{
		return pRequest->GetFromUID() == UserID;
	});

	// If the request is not found, return an undefined error
	if(Iter == m_aRequestsJoin.end())
	{
		return GUILD_MEMBER_RESULT::UNDEFINED_ERROR;
	}

	// Remove the request from the database and delete it from memory
	Database->Execute<DB::REMOVE>(TW_GUILDS_INVITES_TABLE, "WHERE GuildID = '%d' AND UserID = '%d'", m_pGuild->GetID(), (*Iter)->GetFromUID());
	delete (*Iter);
	m_aRequestsJoin.erase(Iter);

	// Try to add the user to the guild as a member if the user is successfully added as a member and pFromMember is not null
	GUILD_MEMBER_RESULT Result = m_pGuild->GetMembers()->Join(UserID);
	if(Result == GUILD_MEMBER_RESULT::SUCCESSFUL && pFromMember)
	{
		// Get the nicknames of the users
		const char* pFromNickname = Instance::Server()->GetAccountNickname(UserID);
		const char* pByNickname = Instance::Server()->GetAccountNickname(pFromMember->GetAccountID());

		// Add a history entry and send a guild chat message
		m_pGuild->GetLogger()->Add(LOGFLAG_MEMBERS_CHANGES, "'%s' accepted invitation from '%s'.", pByNickname, pFromNickname);
		GS()->ChatGuild(m_pGuild->GetID(), "'{STR}' accepted invitation from '{STR}'.", pByNickname, pFromNickname);
	}

	// Return the result of adding the user as a member
	return Result;
}

void CGuildRequestsManager::Deny(int UserID, const CGuildMemberData* pFromMember)
{
	// Find the request in m_aRequestsJoin that matches the UserID
	auto Iter = std::find_if(m_aRequestsJoin.begin(), m_aRequestsJoin.end(), [&UserID](const CGuildRequestData* pRank)
	{
		return pRank->GetFromUID() == UserID;
	});

	// If the request was found
	if(Iter != m_aRequestsJoin.end())
	{
		// Remove the request from the database
		Database->Execute<DB::REMOVE>(TW_GUILDS_INVITES_TABLE, "WHERE GuildID = '%d' AND UserID = '%d'", m_pGuild->GetID(), (*Iter)->GetFromUID());

		// If pFromMember exists
		if(pFromMember)
		{
			// Get the nicknames of the users
			const char* pFromNickname = Instance::Server()->GetAccountNickname(UserID);
			const char* pByNickname = Instance::Server()->GetAccountNickname(pFromMember->GetAccountID());

			// Add a history entry and send a guild chat message
			m_pGuild->GetLogger()->Add(LOGFLAG_MEMBERS_CHANGES, "'%s' denied invitation from '%s'.", pByNickname, pFromNickname);
			GS()->ChatGuild(m_pGuild->GetID(), "'{STR}' denied invitation from '{STR}'.", pByNickname, pFromNickname);
		}

		// Delete the request and remove it from m_aRequestsJoin
		delete (*Iter);
		m_aRequestsJoin.erase(Iter);
	}
}