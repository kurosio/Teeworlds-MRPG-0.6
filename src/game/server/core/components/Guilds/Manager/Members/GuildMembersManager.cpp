/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildMembersManager.h"

#include <game/server/core/components/Guilds/GuildData.h>
#include <game/server/gamecontext.h>

// Get the game server
CGS* CGuildMembersManager::GS() const { return m_pGuild->GS(); }

// Constructor
CGuildMembersManager::CGuildMembersManager(CGuildData* pGuild, std::string&& MembersData) : m_pGuild(pGuild)
{
	// Create a new instance of CGuildRequestsManager with pGuild as the parameter
	m_pRequests = new CGuildRequestsManager(pGuild);

	// Initialize CGuildMembersManager with MembersData using std::move to transfer ownership
	CGuildMembersManager::Init(std::move(MembersData));
}

// Destructor
CGuildMembersManager::~CGuildMembersManager()
{
	// Delete all member data objects
	for(auto pIterMember : m_apMembers)
	{
		delete pIterMember.second;
		pIterMember.second = nullptr;
	}

	delete m_pRequests;
	m_apMembers.clear();
}

// Join a guild
GUILD_MEMBER_RESULT CGuildMembersManager::Join(int AccountID)
{
	// Check if the member is already in the guild
	if(m_apMembers.find(AccountID) != m_apMembers.end() || CGuildData::IsAccountMemberGuild(AccountID))
	{
		return GUILD_MEMBER_RESULT::JOIN_ALREADY_IN_GUILD;
	}

	// Check if there are free slots available in the guild
	if(!HasFreeSlots())
	{
		return GUILD_MEMBER_RESULT::NO_AVAILABLE_SLOTS;
	}

	// Create a new guild member data and add it to the guild members map
	m_apMembers[AccountID] = new CGuildMemberData(m_pGuild, AccountID, m_pGuild->GetRanks()->GetDefaultRank());

	// Reinitialize the guild for the player if they are online
	if(CPlayer* pPlayer = GS()->GetPlayerByUserID(AccountID))
	{
		pPlayer->Account()->ReinitializeGuild();
	}

	// Add a join message to the guild history and send chat message
	const char* pNickname = Instance::GetServer()->GetAccountNickname(AccountID);
	m_pGuild->GetLogger()->Add(LOGFLAG_MEMBERS_CHANGES, "'%s' has joined the guild.", pNickname);
	GS()->ChatGuild(m_pGuild->GetID(), "'{STR}' has joined the guild!", pNickname);

	// Save the guild members data
	Save();
	return GUILD_MEMBER_RESULT::SUCCESSFUL;
}

// Kick a member from the guild
GUILD_MEMBER_RESULT CGuildMembersManager::Kick(int AccountID)
{
	// Check if the player is the guild leader
	if(m_pGuild->GetLeaderUID() == AccountID)
	{
		return GUILD_MEMBER_RESULT::KICK_IS_OWNER;
	}

	// Check if the player is a member of the guild
	if(auto Iter = m_apMembers.find(AccountID); Iter != m_apMembers.end())
	{
		// Delete the member data object and remove it from the member list
		delete (*Iter).second;
		m_apMembers.erase(Iter);

		// Reinitialize the guild for the player
		if(CPlayer* pPlayer = GS()->GetPlayerByUserID(AccountID))
		{
			pPlayer->Account()->ReinitializeGuild();
			pPlayer->m_VotesData.UpdateVotes(MENU_MAIN);
		}

		// Add a left message to the guild history and send chat message
		const char* pNickname = Instance::GetServer()->GetAccountNickname(AccountID);
		m_pGuild->GetLogger()->Add(LOGFLAG_MEMBERS_CHANGES, "'%s' has left the guild.", pNickname);
		GS()->ChatGuild(m_pGuild->GetID(), "'{STR}' has left the guild!", pNickname);

		// Save the guild data
		Save();
		return GUILD_MEMBER_RESULT::SUCCESSFUL;
	}

	return GUILD_MEMBER_RESULT::KICK_DOES_NOT_EXIST;
}

bool CGuildMembersManager::HasFreeSlots() const
{
	return (int)m_apMembers.size() < m_pGuild->GetUpgrades(CGuildData::UPGRADE_AVAILABLE_SLOTS)->getValue();
}

std::pair<int, int> CGuildMembersManager::GetCurrentSlots() const
{
	return std::pair((int)m_apMembers.size(), m_pGuild->GetUpgrades(CGuildData::UPGRADE_AVAILABLE_SLOTS)->getValue());
}

// This function resets the deposit amount for all guild members
void CGuildMembersManager::ResetDeposits()
{
	// Iterate through each member in the member list
	for(auto& [UID, pMember] : m_apMembers)
	{
		// Set the deposit amount for the member to 0
		pMember->SetDeposit(0);
	}

	// Save the updated member list
	Save();
}

int CGuildMembersManager::GetOnlinePlayersCount() const
{
	int Count = 0;
	for(auto& [UID, pMember] : m_apMembers)
	{
		if(GS()->GetPlayerByUserID(UID))
		{
			Count++;
		}
	}
	return Count;
}

// Initialize the guild members from a JSON string
void CGuildMembersManager::Init(std::string&& MembersData)
{
	// Assert by empty
	dbg_assert(m_apMembers.empty(), "");

	// Parse the JSON string
	Tools::Json::parseFromString(MembersData, [this](nlohmann::json& pJson)
	{
		for(auto& pMember : pJson["members"])
		{
			// Get the member ID
			int UID = pMember.value("id", -1);

			// Check if the member ID is valid and not already in the member list
			if(UID > 0 && m_apMembers.find(UID) == m_apMembers.end())
			{
				// Get the rank ID and deposit for the member
				int RID = pMember.value("rank_id", -1);
				int Deposit = pMember.value("deposit", 0);

				// Create a new member data object and add it to the member list
				m_apMembers[UID] = (new CGuildMemberData(m_pGuild, UID, m_pGuild->GetRanks()->Get(RID), Deposit));
			}
		}
	});
}

// Save the guild members to the database
void CGuildMembersManager::Save() const
{
	// Create a JSON object for the member data
	nlohmann::json MembersData;
	for(auto& [UID, pMember] : m_apMembers)
	{
		nlohmann::json memberData;
		memberData["id"] = UID;
		memberData["rank_id"] = pMember->GetRank()->GetID();
		memberData["deposit"] = pMember->GetDeposit();
		MembersData["members"].push_back(memberData);
	}

	// Update the guild data in the database
	Database->Execute<DB::UPDATE, 300>(TW_GUILDS_TABLE, "DefaultRankID = '%d', Members = '%s' WHERE ID = '%d'",
		m_pGuild->GetRanks()->GetDefaultRank()->GetID(), MembersData.dump().c_str(), m_pGuild->GetID());
}

// Get a member by account ID
CGuildMemberData* CGuildMembersManager::Get(int AccountID)
{
	return m_apMembers.find(AccountID) != m_apMembers.end() ? m_apMembers[AccountID] : nullptr;
}