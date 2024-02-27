/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "HouseDoorsController.h"

#include "HouseData.h"

#include <game/server/core/tools/dbset.h>
#include <game/server/gamecontext.h>

CHouseDoorsController::CHouseDoorsController(CGS* pGS, std::string&& AccessData, std::string&& JsonDoorData, CHouseData* pHouse)
	: m_pGS(pGS), m_pHouse(pHouse)
{
	// Reserve memory for the unordered set m_AccessUserIDs to avoid frequent reallocation
	m_AccessUserIDs.reserve(MAX_HOUSE_DOOR_INVITED_PLAYERS);

	// Parse the JSON string using the Tools::Json::parseFromString function initialize doors
	Tools::Json::parseFromString(JsonDoorData, [&](const nlohmann::json& pJsonArray)
	{
		int Number = 1;
		m_apDoors.reserve(pJsonArray.size());
		for(const auto& pJsonDoor : pJsonArray)
		{
			// Check if the door name is not empty
			std::string DoorName = pJsonDoor.value("name", "");
			vec2 Pos = vec2(pJsonDoor.value("x", 0), pJsonDoor.value("y", 0));
			if(!DoorName.empty())
			{
				// Add the door data to the m_apDoors map using the door name as the key
				m_apDoors.emplace(Number, new CHouseDoor(&pGS->m_World, pHouse, std::string(DoorName), Pos));
				Number++;
			}
		}
	});

	//Initialize access list
	DBSet m_Set(AccessData);
	for(auto& p : m_Set.GetDataItems())
	{
		// Convert the data item to an integer
		if(int UID = std::atoi(p.first.c_str()); UID > 0)
		{
			// If the integer is greater than 0, add it to the access user IDs set
			m_AccessUserIDs.insert(UID);
		}
	}
}

// Destructor for the CHouseDoorsController class
CHouseDoorsController::~CHouseDoorsController()
{
	for(auto& p : m_apDoors)
		delete p.second;

	m_apDoors.clear();
}

// Function to open a specific door by its number
void CHouseDoorsController::Open(int Number)
{
	// Open the door
	if(m_apDoors.find(Number) != m_apDoors.end())
		m_apDoors[Number]->Open();
}

// Function to close a specific door by its number
void CHouseDoorsController::Close(int Number)
{
	// Close the door
	if(m_apDoors.find(Number) != m_apDoors.end())
		m_apDoors[Number]->Close();
}

// Function to reverse the state of a specific door by its number
void CHouseDoorsController::Reverse(int Number)
{
	// Check if the door exists in the map
	if(m_apDoors.find(Number) == m_apDoors.end())
		return;

	// Check if the door is closed
	if(m_apDoors[Number]->IsClosed())
		Open(Number); // Open the door
	else
		Close(Number); // Close the door
}

// Function to open all doors
void CHouseDoorsController::OpenAll()
{
	// Open the state of the door by its number in iterate
	for(auto& p : m_apDoors)
		Open(p.first);
}

// Function to close all doors
void CHouseDoorsController::CloseAll()
{
	// Close the state of the door by its number in iterate
	for(auto& p : m_apDoors)
		Close(p.first);
}

// Function to reverse the state of all doors
void CHouseDoorsController::ReverseAll()
{
	// Reverse the state of the door by its number in iterate
	for(auto& p : m_apDoors)
		Reverse(p.first);
}

void CHouseDoorsController::AddAccess(int UserID)
{
	// Check if the size of the m_AccessUserIDs set is greater than or equal to the maximum number of invited players allowed
	if(m_AccessUserIDs.size() >= MAX_HOUSE_DOOR_INVITED_PLAYERS)
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
void CHouseDoorsController::RemoveAccess(int UserID)
{
	// Check if the UserID exists in the m_AccessUserIDs set and erase it
	if(m_AccessUserIDs.erase(UserID) > 0)
	{
		// If the UserID was successfully erased, save the updated access list
		SaveAccessList();
	}
}

// Function to check if a user has access to a house door
bool CHouseDoorsController::HasAccess(int UserID)
{
	// Check if the account ID of the house matches the given user ID
	if(m_pHouse->GetAccountID() == UserID)
		return true;

	// Check if the given user ID is present in the set of access user IDs
	return m_AccessUserIDs.find(UserID) != m_AccessUserIDs.end();
}

// Returns the number of available access slots for the house door
int CHouseDoorsController::GetAvailableAccessSlots() const
{
	// Calculate the number of available access slots by subtracting the number of current access users from the maximum allowed invited players
	return (int)MAX_HOUSE_DOOR_INVITED_PLAYERS - (int)m_AccessUserIDs.size();
}

void CHouseDoorsController::SaveAccessList() const
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