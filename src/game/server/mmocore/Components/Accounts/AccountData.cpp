/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "AccountData.h"

#include "game/server/mmocore/Components/Houses/HouseData.h"
#include <game/server/mmocore/Components/Groups/GroupData.h>

std::map < int, CAccountData > CAccountData::ms_aData;
std::map < int, CAccountTempData > CAccountTempData::ms_aPlayerTempData;

// Set the ID of the account
void CAccountData::SetUniqueID(int ID)
{
	// Check if the ID has already been set
	dbg_assert(m_ID <= 0, "Unique AccountID cannot change the value more than 1 time");

	// Set the ID
	m_ID = ID;

	// Initialize account data
	ReinitializeHouse();
	ReinitializeGroup();
}

// This function initializes the house data for the account
void CAccountData::ReinitializeHouse()
{
	// Iterate through all the house data objects
	for(const auto& p : CHouseData::Data())
	{
		// Check if the account ID of the house data object matches the account ID of the current account
		if(p->GetAccountID() == m_ID)
		{
			// Set the house data pointer of the account to the current house data object
			m_pHouseData = p.get();
			return; // Exit the function
		}
	}

	// If no matching house data object is found, set the house data pointer of the account to nullptr
	m_pHouseData = nullptr;
}

// 
void CAccountData::ReinitializeGroup()
{
	// Iterate through all the group data objects
	for(auto& p : GroupData::Data())
	{
		// Check if the account ID of the group data object matches the account ID of the current account
		auto& Accounts = p.second.GetAccounts();
		if(Accounts.find(m_ID) != Accounts.end())
		{
			// Set the group data pointer of the account to the current group data object
			m_pGroupData = &p.second;
			return; // Exit the function
		}
	}

	// If no matching group data object is found, set the group data pointer of the account to nullptr
	m_pGroupData = nullptr;
}