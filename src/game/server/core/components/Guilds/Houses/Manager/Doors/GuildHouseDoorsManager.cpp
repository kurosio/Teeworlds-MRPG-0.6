/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildHouseDoorsManager.h"

#include <game/server/core/components/Guilds/Houses/GuildHouseData.h>

#include <game/server/gamecontext.h>

CGS* CGuildHouseDoorManager::GS() const { return m_pHouse->GS(); }

CGuildHouseDoorManager::CGuildHouseDoorManager(CGuildHouseData* pHouse) : m_pHouse(pHouse) {}

// Destructor for the CHouseDoorsController class
CGuildHouseDoorManager::~CGuildHouseDoorManager()
{
	for(auto& p : m_apDoors)
		delete p.second;

	m_apDoors.clear();
}

// Function to open a specific door by its number
void CGuildHouseDoorManager::Open(int Number)
{
	// Open the door
	if(m_apDoors.find(Number) != m_apDoors.end())
		m_apDoors[Number]->Open();
}

// Function to close a specific door by its number
void CGuildHouseDoorManager::Close(int Number)
{
	// Close the door
	if(m_apDoors.find(Number) != m_apDoors.end())
		m_apDoors[Number]->Close();
}

// Function to reverse the state of a specific door by its number
void CGuildHouseDoorManager::Reverse(int Number)
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
void CGuildHouseDoorManager::OpenAll()
{
	// Open the state of the door by its number in iterate
	for(auto& p : m_apDoors)
		Open(p.first);
}

// Function to close all doors
void CGuildHouseDoorManager::CloseAll()
{
	// Close the state of the door by its number in iterate
	for(auto& p : m_apDoors)
		Close(p.first);
}

// Function to reverse the state of all doors
void CGuildHouseDoorManager::ReverseAll()
{
	// Reverse the state of the door by its number in iterate
	for(auto& p : m_apDoors)
		Reverse(p.first);
}

void CGuildHouseDoorManager::AddDoor(const char* pDoorname, vec2 Position)
{
	// Add the door to the m_apDoors map using the door name as the key
	m_apDoors.emplace(m_apDoors.size() + 1, new CGuildHouseDoor(&GS()->m_World, m_pHouse, std::string(pDoorname), Position));
}

void CGuildHouseDoorManager::RemoveDoor(const char* pDoorname, vec2 Position)
{
	auto iter = std::find_if(m_apDoors.begin(), m_apDoors.end(), [&](const std::pair<int, CGuildHouseDoor*>& p)
	{
		return p.second->GetName() == pDoorname && p.second->GetPos() == Position;
	});

	if(iter != m_apDoors.end())
	{
		delete iter->second;
		m_apDoors.erase(iter);
	}
}