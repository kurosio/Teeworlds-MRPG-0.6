/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildMembersManager.h"

#include <game/server/mmocore/Components/Guilds/GuildData.h>
#include <game/server/gamecontext.h>

// Get the game server
CGS* CGuildMembersController::GS() const { return m_pGuild->GS(); }

// Constructor
CGuildMembersController::CGuildMembersController(CGuildData* pGuild, std::string&& MembersData) : m_pGuild(pGuild)
{
	CGuildMembersController::Init(std::move(MembersData));
}

// Destructor
CGuildMembersController::~CGuildMembersController()
{
	// Delete all member data objects
	for(auto pIterMember : m_apMembers)
	{
		delete pIterMember.second;
		pIterMember.second = nullptr;
	}

	m_apMembers.clear();
}

// Join a guild
GUILD_MEMBER_RESULT CGuildMembersController::Join(int AccountID)
{
	// Check if the player is already in the guild
	if(m_apMembers.find(AccountID) != m_apMembers.end())
	{
		return GUILD_MEMBER_RESULT::JOIN_ALREADY_IN_GUILD;
	}

	// Create a new member data object and add it to the member list
	m_apMembers[AccountID] = new CGuildMemberData(m_pGuild, AccountID, m_pGuild->GetRanks()->GetDefaultRank());

	// Reinitialize the guild for the player
	if(CPlayer* pPlayer = GS()->GetPlayerByUserID(AccountID))
	{
		pPlayer->Account()->ReinitializeGuild();
	}

	// Save the guild data
	Save();
	return GUILD_MEMBER_RESULT::SUCCESSFUL;
}

// Kick a member from the guild
GUILD_MEMBER_RESULT CGuildMembersController::Kick(int AccountID)
{
	// Check if the player is the guild leader
	if(m_pGuild->GetLeaderUID() == AccountID)
	{
		return GUILD_MEMBER_RESULT::CANT_KICK_LEADER;
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
			GS()->UpdateVotes(pPlayer->GetCID(), MENU_MAIN);
		}

		// Save the guild data
		Save();
		return GUILD_MEMBER_RESULT::SUCCESSFUL;
	}

	return GUILD_MEMBER_RESULT::KICK_DOES_NOT_EXIST;
}

// Initialize the guild members from a JSON string
void CGuildMembersController::Init(std::string&& MembersData)
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
void CGuildMembersController::Save() const
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
CGuildMemberData* CGuildMembersController::Get(int AccountID)
{
	return m_apMembers.find(AccountID) != m_apMembers.end() ? m_apMembers[AccountID] : nullptr;
}