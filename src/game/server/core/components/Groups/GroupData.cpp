/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GroupData.h"

#include "game/server/gamecontext.h"

// Function to add AccountID
bool GroupData::Add(int AccountID)
{
	// Get the game server instance and player
	CGS* pGS = (CGS*)Instance::Server()->GameServer();
	CPlayer* pPlayer = pGS->GetPlayerByUserID(AccountID);

	// Check if the player exists
	if(pPlayer && pPlayer->Account()->GetGroup())
	{
		// Send a chat message to the player indicating they are already in a group
		pGS->ChatAccount(AccountID, "You're already in a group!");
		return false;
	}

	// Check if the size of m_vAccountIds is greater than or equal to MAX_GROUP_MEMBERS
	if(IsFull())
	{
		// Send a chat message to the player indicating that the group is full
		pGS->ChatAccount(AccountID, "The group is full!");
		return false;
	}

	// Add AccountID to the m_vAccountIds set and check if it was already present
	if(m_vAccountIds.insert(AccountID).second)
	{
		// Check if the player exists
		if(pPlayer)
		{
			// Reinitialize the player's group
			pPlayer->Account()->ReinitializeGroup();
		}

		// Save changes
		Save();

		// Send a chat message to the player's account
		pGS->ChatAccount(AccountID, "You have become a member of the group!");
		return true;
	}

	// Return false to indicate that the account was not added to the group
	return false;
}

// Function to remove a group from the data based on the account ID
bool GroupData::Remove(int AccountID)
{
	// Get the game server instance and player
	CGS* pGS = (CGS*)Instance::Server()->GameServer();
	CPlayer* pPlayer = pGS->GetPlayerByUserID(AccountID);

	// Check if the size of m_vAccountIds is greater than 1 and if m_LeaderUID matches AccountID
	if((int)m_vAccountIds.size() > 1 && m_LeaderUID == AccountID)
	{
		pGS->ChatAccount(AccountID, "Before you leave a group as a leader, kick all players out or transfer leadership rights.");
		return false;
	}

	// Check if pPlayer exists and if the player is not in the group or is in a different group
	if(pPlayer && (!pPlayer->Account()->GetGroup() || pPlayer->Account()->GetGroup()->GetID() != m_ID))
	{
		pGS->ChatAccount(AccountID, "You're not in a group!");
		return false;
	}

	// Check if the AccountID exists in m_vAccountIds and erase it
	if(m_vAccountIds.erase(AccountID) > 0)
	{
		// Reinitialize the player's group
		if(pPlayer)
		{
			pPlayer->Account()->ReinitializeGroup();
		}

		// If m_vAccountIds is empty, disband the group
		if(m_vAccountIds.empty())
		{
			// Send chat and disband group
			pGS->ChatAccount(AccountID, "The group was disbanded.");
			Disband();
		}
		else
		{
			// Save changes to the group
			Save();
		}

		// Send a chat message to the player's account
		pGS->ChatAccount(AccountID, "You left the group.");
		return true;
	}
	return false;
}

// This function is used to disband a group of data.
void GroupData::Disband()
{
	// Create a copy of the account IDs in a set called ReinitilizedAccounts
	const ska::unordered_set<int> ReinitilizedAccounts = m_vAccountIds;
	// Clear the original account IDs set
	m_vAccountIds.clear();

	// Reinitialize all accounts in the game
	for(auto& AID : ReinitilizedAccounts)
	{
		// Get the game server instance and player
		CGS* pGS = (CGS*)Instance::Server()->GameServer();
		CPlayer* pPlayer = pGS->GetPlayerByUserID(AID);

		// Check if the player exists
		if(pPlayer)
		{
			// Reinitialize the player's group
			pPlayer->Account()->ReinitializeGroup();
		}
	}

	// Remove the group from the database and m_pData
	Database->Execute<DB::REMOVE>(TW_GROUPS_TABLE, "WHERE ID = '%d'", m_ID);
	m_pData.erase(m_ID);
}

// This function changes the owner of the group data to the specified account ID.
void GroupData::ChangeLeader(int AccountID)
{
	// Check if the account ID was found
	dbg_assert(m_vAccountIds.find(AccountID) != m_vAccountIds.end(), "[Group system] account not included inside accountids");

	// Send chat messages to the previous owner and the new owner
	CGS* pGS = (CGS*)Instance::Server()->GameServer();
	pGS->ChatAccount(m_LeaderUID, "You've transferred ownership of the group!");
	pGS->ChatAccount(AccountID, "You are now the new owner of the group!");

	// Update the owner UID with the new account ID
	m_LeaderUID = AccountID;

	// Save the changes
	Save();
}

// This function changes the color of the group
void GroupData::ChangeColor(int NewColor)
{
	// Clamp the new color value between 1 and 63
	m_TeamColor = clamp(NewColor, 1, 63);

	// Save the new color value
	Save();
}

void GroupData::Save() const
{
	// Create a string variable to store the access data
	std::string StrAccountIds;

	// Reserve memory for the vector m_vAccountIds to avoid frequent reallocation
	StrAccountIds.reserve(m_vAccountIds.size() * 8);

	// Iterate through each user ID in the m_vAccountIds unordered set
	for(const auto& UID : m_vAccountIds)
	{
		// Convert the user ID to a string and append it to the AccountIds string
		StrAccountIds += std::to_string(UID);
		// Append a comma after each user ID
		StrAccountIds += ',';
	}

	// If the AccountIds string is not empty
	if(!StrAccountIds.empty())
		// Remove the last character (the extra comma) from the AccountIds string
		StrAccountIds.pop_back();


	// Update the "AccountIDs" column in the TW_GROUPS_TABLE table of the database with the updated StrAccountIDs string
	// for the group with the specified ID (m_ID)
	Database->Execute<DB::UPDATE>(TW_GROUPS_TABLE, "AccountIDs = '%s', GetOwnerUID = '%d', Color = '%d' WHERE ID = '%d'", StrAccountIds.c_str(), m_LeaderUID, m_TeamColor, m_ID);
}
