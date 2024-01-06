/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildHouseDoorsController.h"

#include <game/server/mmocore/Components/Guilds/Houses/GuildHouseData.h>

#include <game/server/gamecontext.h>

CGS* CGuildHouseDoorsController::GS() const { return m_pHouse->GS(); }

CGuildHouseDoorsController::CGuildHouseDoorsController(std::string&& JsonDoorData, CGuildHouseData* pHouse) : m_pHouse(pHouse)
{
	CGuildHouseDoorsController::Init(std::move(JsonDoorData), pHouse);
}

// Destructor for the CHouseDoorsController class
CGuildHouseDoorsController::~CGuildHouseDoorsController()
{
	for(auto& p : m_apDoors)
		delete p.second;

	m_apDoors.clear();
}

// Function to open a specific door by its number
void CGuildHouseDoorsController::Open(int Number)
{
	// Open the door
	if(m_apDoors.find(Number) != m_apDoors.end())
		m_apDoors[Number]->Open();
}

// Function to close a specific door by its number
void CGuildHouseDoorsController::Close(int Number)
{
	// Close the door
	if(m_apDoors.find(Number) != m_apDoors.end())
		m_apDoors[Number]->Close();
}

// Function to reverse the state of a specific door by its number
void CGuildHouseDoorsController::Reverse(int Number)
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
void CGuildHouseDoorsController::OpenAll()
{
	// Open the state of the door by its number in iterate
	for(auto& p : m_apDoors)
		Open(p.first);
}

// Function to close all doors
void CGuildHouseDoorsController::CloseAll()
{
	// Close the state of the door by its number in iterate
	for(auto& p : m_apDoors)
		Close(p.first);
}

// Function to reverse the state of all doors
void CGuildHouseDoorsController::ReverseAll()
{
	// Reverse the state of the door by its number in iterate
	for(auto& p : m_apDoors)
		Reverse(p.first);
}

void CGuildHouseDoorsController::Init(std::string&& JsonDoorData, CGuildHouseData* pHouse)
{
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
				m_apDoors.emplace(Number, new CGuildHouseDoor(&GS()->m_World, pHouse, std::string(DoorName), Pos));
				Number++;
			}
		}
	});
}