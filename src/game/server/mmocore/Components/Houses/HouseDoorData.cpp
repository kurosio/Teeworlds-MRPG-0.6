/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "HouseDoorData.h"

#include "Entities/HouseDoor.h"
#include "HouseData.h"

#include <game/server/mmocore/Utils/DBSet.h>
#include <game/server/gamecontext.h>

CHouseDoorData::CHouseDoorData(CGS* pGS, std::string&& AccessData, std::string&& JsonDoorData, CHouseData* pHouse)
	: m_pGS(pGS), m_pHouse(pHouse)
{
	// Reserve memory for the unordered set m_AccessUserIDs to avoid frequent reallocation
	m_AccessUserIDs.reserve(MAX_HOUSE_INVITED_PLAYERS);

	// Parse the JSON string using the Tools::Json::parseFromString function
	Tools::Json::parseFromString(JsonDoorData, [&](const nlohmann::json& pJsonArray)
	{
		int UniqueID = 0;
		m_apDoors.reserve(pJsonArray.size());
		for(const auto& pJsonDoor : pJsonArray)
		{
			// Check if the door name is not empty
			std::string DoorName = pJsonDoor.value("name", "");
			vec2 Pos = vec2(pJsonDoor.value("x", 0), pJsonDoor.value("y", 0));
			if(!DoorName.empty())
			{
				// Add the door data to the m_apDoors map using the door name as the key
				m_apDoors.emplace(UniqueID, CHouseDoorInfo(std::string(DoorName), Pos));
				UniqueID++;
			}
		}
	});

	//Initialize access list
	DBSet m_Set(AccessData);
	for(auto& p : m_Set.GetDataItems())
	{
		// Convert the data item to an integer
		if(int UID = std::atoi(p.c_str()); UID > 0)
		{
			// If the integer is greater than 0, add it to the access user IDs set
			m_AccessUserIDs.insert(UID);
		}
	}
}

// Destructor for the CHouseDoorData class
CHouseDoorData::~CHouseDoorData()
{
	for(auto p : m_apDoors)
	{
		delete p.second.m_pDoor;
	}

	m_apDoors.clear();
}

// Open the house door
void CHouseDoorData::Open(int UniqueDoorID)
{
	// Check if the door exists
	if(m_apDoors.find(UniqueDoorID) != m_apDoors.end() && m_apDoors[UniqueDoorID].m_pDoor)
	{
		// Delete the door object
		delete m_apDoors[UniqueDoorID].m_pDoor;
		m_apDoors[UniqueDoorID].m_pDoor = nullptr;
	}
}

// This function is used to close the house door.
void CHouseDoorData::Close(int UniqueDoorID)
{
	// Check if the door object is not already created.
	if(m_apDoors.find(UniqueDoorID) != m_apDoors.end() && !m_apDoors[UniqueDoorID].m_pDoor)
	{
		// Create a new HouseDoor object and assign it to m_pDoor.
		m_apDoors[UniqueDoorID].m_pDoor = new HouseDoor(&m_pGS->m_World, m_apDoors[UniqueDoorID].m_Pos, this);
	}
}

// This function is used to reverse the state of the house door
void CHouseDoorData::Reverse(int UniqueDoorID)
{
	if(m_apDoors.find(UniqueDoorID) == m_apDoors.end())
		return;

	// Check if the door pointer is not null
	if(m_apDoors[UniqueDoorID].m_pDoor)
		// If the door is currently closed, open it
		Open(UniqueDoorID);
	else
		// If the door is currently open, close it
		Close(UniqueDoorID);
}

// Function to open all doors in the house
void CHouseDoorData::OpenAll()
{
	// Iterate through all doors in the house
	for(auto& p : m_apDoors)
		Open(p.first);
}

// Function to close all doors in the house
void CHouseDoorData::CloseAll()
{
	// Iterate through all doors in the house
	for(auto& p : m_apDoors)
		Close(p.first);
}

// Function to reverse the state of all doors in the house
void CHouseDoorData::ReverseAll()
{
	// Iterate through all doors in the house
	for(auto& p : m_apDoors)
		Reverse(p.first);
}

void CHouseDoorData::AddAccess(int UserID)
{
	// Check if the size of the m_AccessUserIDs set is greater than or equal to the maximum number of invited players allowed
	if(m_AccessUserIDs.size() >= MAX_HOUSE_INVITED_PLAYERS)
	{
		m_pGS->ChatAccount(m_pHouse->GetAccountID(), "You have reached the limit of the allowed players!");
		return;
	}

	// Add UserID to the m_AccessUserIDs set and check if it was already present
	if(!m_AccessUserIDs.insert(UserID).second)
	{
		// UserID was already present in the set
		return;
	}

	// Save the updated access list
	SaveAccessList();
}

// Function to remove access for a specific user from the house door data
void CHouseDoorData::RemoveAccess(int UserID)
{
	// Check if the UserID exists in the m_AccessUserIDs set and erase it
	if(m_AccessUserIDs.erase(UserID) > 0)
	{
		// If the UserID was successfully erased, save the updated access list
		SaveAccessList();
	}
}

// Function to check if a user has access to a house door
bool CHouseDoorData::HasAccess(int UserID)
{
	// Check if the account ID of the house matches the given user ID
	if(m_pHouse->GetAccountID() == UserID)
		return true;

	// Check if the given user ID is present in the set of access user IDs
	return m_AccessUserIDs.find(UserID) != m_AccessUserIDs.end();
}

// Returns the number of available access slots for the house door
int CHouseDoorData::GetAvailableAccessSlots() const
{
	// Calculate the number of available access slots by subtracting the number of current access users from the maximum allowed invited players
	return (int)MAX_HOUSE_INVITED_PLAYERS - (int)m_AccessUserIDs.size();
}

void CHouseDoorData::SaveAccessList() const
{
	// Create a string variable to store the access data
	std::string AccessData;

	// Reserve memory for the vector m_AccessUserIDs to avoid frequent reallocation
	AccessData.reserve(m_AccessUserIDs.size() * 8);

	// Iterate through each user ID in the m_AccessUserIDs unordered set
	for(const auto& UID : m_AccessUserIDs)
	{
		// Convert the user ID to a string and append it to the AccessData string
		AccessData += std::to_string(UID);
		// Append a comma after each user ID
		AccessData += ',';
	}

	// If the AccessData string is not empty
	if(!AccessData.empty())
		// Remove the last character (the extra comma) from the AccessData string
		AccessData.pop_back();

	// Execute an update query on the Database object
	Database->Execute<DB::UPDATE>(TW_HOUSES_TABLE, "AccessData = '%s' WHERE ID = '%d'", AccessData.c_str(), m_pHouse->GetID());
}
