/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GUILD_HOUSE_DOORS_CONTROLLER_H
#define GAME_SERVER_COMPONENT_GUILD_HOUSE_DOORS_CONTROLLER_H

#include "GuildHouseDoorData.h"

class CGS;
class CHouseData;

// The CHouseDoorsController class is responsible for managing the doors of a house
class CGuildHouseDoorsController
{
	friend class CHouseData; // CHouseData class has access to private members of CHouseDoorsController
	CGS* m_pGS {}; // Pointer to the game state object
	CHouseData* m_pHouse {}; // Pointer to the house data object

	ska::unordered_map<int, CHouseDoor*> m_apDoors {}; // Map of door numbers to CHouseDoor objects

public:
	// Constructor
	CGuildHouseDoorsController(CGS* pGS, std::string&& JsonDoorData, CHouseData* pHouse);

	// Destructor
	~CGuildHouseDoorsController();

	// Getters
	ska::unordered_map<int, CHouseDoor*>& GetDoors() { return m_apDoors; } // Get the map of door numbers to CHouseDoor objects

	// Door control methods
	void Open(int Number); // Open a specific door
	void Close(int Number); // Close a specific door
	void Reverse(int Number); // Reverse the state of a specific door

	void OpenAll(); // Open all doors
	void CloseAll(); // Close all doors
	void ReverseAll(); // Reverse the state of all doors
};
#endif