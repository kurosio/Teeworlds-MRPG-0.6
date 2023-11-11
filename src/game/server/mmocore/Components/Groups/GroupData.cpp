/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GroupData.h"

#include "game/server/gamecontext.h"
#include "game/server/mmocore/Utils/DBSet.h"

void GroupData::Init(int OwnerUID, int Color, DBSet&& SetAccountIDs)
{
	// init access list
	for(auto& p : SetAccountIDs.GetDataItems())
	{
		if(int UID = std::atoi(p.c_str()); UID > 0)
		{
			m_AccountIds[UID] = true;
		}
	}

	m_TeamColor = Color;
	m_OwnerUID = OwnerUID;
	m_pData[m_ID] = *this;
}

bool GroupData::Add(class CGS* pGS, int AccountID)
{
	// Verify if pGS exists and if the player with the specified AccountID exists in pGS
	if(pGS && pGS->GetPlayerByUserID(AccountID))
	{
		// Get a pointer to the player object using the AccountID
		CPlayer* pPlayer = pGS->GetPlayerByUserID(AccountID);

		// Check if the player is already part of a group
		if(pPlayer->Acc().GetGroup())
		{
			// Send a chat message to the player indicating they are already in a group
			pGS->ChatAccount(AccountID, "You're already in a group!");
			return false;
		}

		// Update the player's Account's GroupID with the m_ID value
		pPlayer->Acc().m_GroupID = m_ID;
	}

	// Search for the AccountID in the m_AccountIds
	if(m_AccountIds.find(AccountID) != m_AccountIds.end())
		return false;

	// If the AccountID is not found in the vector
	// Add the AccountID to the m_AccountIds vector and save database
	pGS->ChatAccount(AccountID, "You joined the group!");
	m_AccountIds[AccountID] = true;
	Save();
	return true;
}

bool GroupData::Remove(class CGS* pGS, int AccountID)
{
	// Check if the size of m_AccountIds is greater than 1 and if m_OwnerUID matches AccountID
	if((int)m_AccountIds.size() > 1 && m_OwnerUID == AccountID)
	{
		// If the conditions are met, send a message to pGS to notify the account with AccountID
		pGS->ChatAccount(AccountID, "Before you leave a group as a leader, kick all players out or transfer leadership rights.");
		return false;
	}

	// Check if pGS and pGS->GetPlayerByUserID(AccountID) are both valid
	if(pGS && pGS->GetPlayerByUserID(AccountID))
	{
		// Get the player with the specified AccountID and group data
		CPlayer* pPlayer = pGS->GetPlayerByUserID(AccountID);
		GroupData* pGroup = pPlayer->Acc().GetGroup();

		// Check if the player is not in any group or if they are not in the specified group
		if(!pGroup || pGroup->GetID() != m_ID)
		{
			// Send a message to the player indicating that they are not in the group
			pGS->ChatAccount(AccountID, "You're not in the group!");
			return false;
		}

		// Remove the player from the group by setting their group ID to -1
		pPlayer->Acc().m_GroupID = -1;
	}

	// Pass the AccountID and the message "You left the group!" as parameters to the function
	pGS->ChatAccount(AccountID, "You left the group!");

	// Find the specified account ID in the m_AccountIds vector
	if(m_AccountIds.find(AccountID) != m_AccountIds.end())
	{
		// Check if the size of the m_AccountIds vector is 1
		if(m_AccountIds.size() == 1)
		{
			// Send a chat message to notify the account with ID AccountID that the group was disbanded
			pGS->ChatAccount(AccountID, "The group was disbanded.");
		}

		// Remove the element from the m_AccountIds vector
		m_AccountIds.erase(AccountID);

		// If m_AccountIds is not empty
		if(m_AccountIds.empty())
		{
			// Remove the group from the database and m_pData
			Database->Execute<DB::REMOVE>("tw_groups", "WHERE ID = '%d'", m_ID);
			m_pData.erase(m_ID);
			return true;
		}

		// Save the changes
		Save();
		return true;
	}

	return false;
}

bool GroupData::Disband()
{
	// Set for all accounts group id -1
	for(auto& Account : GetAccounts())
	{
		CGS* pGS = (CGS*)Instance::GetServer()->GameServer();
		CPlayer* pPlayer = pGS->GetPlayerByUserID(Account.first);
		if(pPlayer)
			pPlayer->Acc().m_GroupID = -1;
	}

	// Remove the group from the database and m_pData
	Database->Execute<DB::REMOVE>("tw_groups", "WHERE ID = '%d'", m_ID);
	m_pData.erase(m_ID);
	return true;
}

void GroupData::ChangeOwner(CGS* pGS, int AccountID)
{
	// Check if the account ID was found
	dbg_assert(m_AccountIds.find(AccountID) != m_AccountIds.end(), "[Group system] account not included inside accountids");

	// Send chat messages to the previous owner and the new owner
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
	// Create a string variable to store the account IDs in a comma-separated format
	std::string StrAccountIDs("");
	for(const auto& Account : m_AccountIds)
	{
		// Convert the account ID to a string and append it to the StrAccountIDs string with a comma
		StrAccountIDs += std::to_string(Account.first) + ",";
	}

	// Remove the last comma from the StrAccountIDs string if it is not empty
	if(!StrAccountIDs.empty())
	{
		StrAccountIDs.pop_back();
	}

	// Update the "AccountIDs" column in the "tw_groups" table of the database with the updated StrAccountIDs string
	// for the group with the specified ID (m_ID)
	Database->Execute<DB::UPDATE>("tw_groups", "AccountIDs = '%s', OwnerUID = '%d', Color = '%d' WHERE ID = '%d'", StrAccountIDs.c_str(), m_OwnerUID, m_TeamColor, m_ID);
}
