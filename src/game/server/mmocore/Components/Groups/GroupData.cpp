/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GroupData.h"

#include "game/server/mmocore/Utils/DBSet.h"

void GroupData::Init(std::string AccessIdsList)
{
	// init access list
	DBSet m_Set(AccessIdsList);
	for(auto& p : m_Set.GetDataItems())
	{
		if(int UID = std::atoi(p.c_str()); UID > 0)
			m_AccountIds.push_back(UID);
	}

	m_pData[m_ID] = *this;
}

bool GroupData::Add(int AccountID)
{
	// Search for the AccountID in the m_AccountIds vector using the std::find function
	auto p = std::find(m_AccountIds.begin(), m_AccountIds.end(), AccountID);
	if(p != m_AccountIds.end())
	{
		// If the AccountID is found in the vector
		return false;
	}

	// If the AccountID is not found in the vector
	// Add the AccountID to the m_AccountIds vector and save database
	m_AccountIds.push_back(AccountID);
	Save();
	return true;
}

bool GroupData::Remove(int AccountID)
{
	// Find the specified account ID in the m_AccountIds vector
	const auto element = std::find(m_AccountIds.begin(), m_AccountIds.end(), AccountID);
	if(element == m_AccountIds.end())
	{
		// If the account ID is not found, return false
		return false;
	}

	// Remove the account ID from the m_AccountIds vector
	m_AccountIds.erase(element);

	// If m_AccountIds is not empty
	if(!m_AccountIds.empty())
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

bool GroupData::CheckGroup(int AccountID)
{
	// Check if the vector m_AccountIds is not empty
	if(!m_AccountIds.empty())
	{
		// Use std::find_if to search for an element that matches the AccountID
		// The lambda function is used to check if each element equals AccountID
		// If a matching element is found, return true
		if(std::find_if(m_AccountIds.begin(), m_AccountIds.end(), [AccountID](const int& p) { return p == AccountID; }) != m_AccountIds.end())
		{
			return true;
		}
	}

	// If the vector is empty or no matching element is found, return false
	return false;
}

void GroupData::Save() const
{
	// Create a string variable to store the account IDs in a comma-separated format
	std::string StrAccountIDs("");
	for(const auto& UID : m_AccountIds)
	{
		// Convert the account ID to a string and append it to the StrAccountIDs string with a comma
		StrAccountIDs += std::to_string(UID) + ",";
	}

	// Remove the last comma from the StrAccountIDs string if it is not empty
	if(!StrAccountIDs.empty())
	{
		StrAccountIDs.pop_back();
	}

	// Update the "AccountIDs" column in the "tw_groups" table of the database with the updated StrAccountIDs string
	// for the group with the specified ID (m_ID)
	Database->Execute<DB::UPDATE>("tw_groups", "AccountIDs = '%s' WHERE ID = '%d'", StrAccountIDs.c_str(), m_ID);
}
