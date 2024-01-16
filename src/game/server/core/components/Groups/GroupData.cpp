/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GroupData.h"

#include "game/server/gamecontext.h"
#include "game/server/core/tools/dbset.h"

// This function initializes the GroupData object with the provided parameters
void GroupData::Init(int OwnerUID, int Color, DBSet&& SetAccountIDs)
{
	// init access list
	for(auto& p : SetAccountIDs.GetDataItems())
	{
		// Convert the data item to an integer
		if(int UID = std::atoi(p.c_str()); UID > 0)
		{
			// If the integer is greater than 0, add it to the access user IDs set
			m_AccountIds.insert(UID);
		}
	}

	m_TeamColor = Color;
	m_OwnerUID = OwnerUID;
	m_pData[m_ID] = *this;
}

// Function to add AccountID
bool GroupData::Add(int AccountID)
{
	// Get the game server instance and player
	CGS* pGS = (CGS*)Instance::GetServer()->GameServer();
	CPlayer* pPlayer = pGS->GetPlayerByUserID(AccountID);

	// Check if the player exists
	if(pPlayer && pPlayer->Account()->GetGroup())
	{
		// Send a chat message to the player indicating they are already in a group
		pGS->ChatAccount(AccountID, "You're already in a group!");
		return false;
	}

	// Add AccountID to the m_AccountIds set and check if it was already present
	if(m_AccountIds.insert(AccountID).second)
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
		pGS->ChatAccount(AccountID, "You joined the group!");
		return true;
	}

	// Return false to indicate that the account was not added to the group
	return false;
}

// Function to remove a group from the data based on the account ID
bool GroupData::Remove(int AccountID)
{
	// Get the game server instance and player
	CGS* pGS = (CGS*)Instance::GetServer()->GameServer();
	CPlayer* pPlayer = pGS->GetPlayerByUserID(AccountID);

	// Check if the size of m_AccountIds is greater than 1 and if m_OwnerUID matches AccountID
	if((int)m_AccountIds.size() > 1 && m_OwnerUID == AccountID)
	{
		// If the conditions are met, send a message to pGS to notify the account with AccountID
		pGS->ChatAccount(AccountID, "Before you leave a group as a leader, kick all players out or transfer leadership rights.");
		return false;
	}

	// Check if pPlayer exists and if the player is not in the group or is in a different group
	if(pPlayer && (!pPlayer->Account()->GetGroup() || pPlayer->Account()->GetGroup()->GetID() != m_ID))
	{
		// Send a chat message to the player's account indicating that they are not in the group
		pGS->ChatAccount(AccountID, "You're not in the group!");
		return false;
	}

	// Check if the AccountID exists in m_AccountIds and erase it
	if(m_AccountIds.erase(AccountID) > 0)
	{
		// Reinitialize the player's group
		if(pPlayer)
		{
			pPlayer->Account()->ReinitializeGroup();
		}

		// If m_AccountIds is empty, disband the group
		if(m_AccountIds.empty())
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
	const ska::unordered_set<int> ReinitilizedAccounts = m_AccountIds;
	// Clear the original account IDs set
	m_AccountIds.clear();

	// Reinitialize all accounts in the game
	for(auto& AID : ReinitilizedAccounts)
	{
		// Get the game server instance and player
		CGS* pGS = (CGS*)Instance::GetServer()->GameServer();
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
void GroupData::ChangeOwner(int AccountID)
{
	// Check if the account ID was found
	dbg_assert(m_AccountIds.find(AccountID) != m_AccountIds.end(), "[Group system] account not included inside accountids");

	// Send chat messages to the previous owner and the new owner
	CGS* pGS = (CGS*)Instance::GetServer()->GameServer();
	pGS->ChatAccount(m_OwnerUID, "You've transferred ownership of the group!");
	pGS->ChatAccount(AccountID, "You are now the new owner of the group!");

	// Update the owner UID with the new account ID
	m_OwnerUID = AccountID;

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

	// Reserve memory for the vector m_AccountIds to avoid frequent reallocation
	StrAccountIds.reserve(m_AccountIds.size() * 8);

	// Iterate through each user ID in the m_AccountIds unordered set
	for(const auto& UID : m_AccountIds)
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
	Database->Execute<DB::UPDATE>(TW_GROUPS_TABLE, "AccountIDs = '%s', OwnerUID = '%d', Color = '%d' WHERE ID = '%d'", StrAccountIds.c_str(), m_OwnerUID, m_TeamColor, m_ID);
}
