/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_HOUSE_DOORS_CONTROLLER_H
#define GAME_SERVER_COMPONENT_HOUSE_DOORS_CONTROLLER_H

#include "HouseDoorData.h"

class CGS;
class CHouseData;

// The CHouseDoorsController class is responsible for managing the doors of a house
class CHouseDoorsController
{
	friend class CHouseData; // CHouseData class has access to private members of CHouseDoorsController
	CGS* m_pGS {}; // Pointer to the game state object
	CHouseData* m_pHouse {}; // Pointer to the house data object

	ska::unordered_map<int, CHouseDoor*> m_apDoors {}; // Map of door numbers to CHouseDoor objects
	ska::unordered_set<int> m_AccessUserIDs {}; // Set of user IDs with access to the house

public:
	// Constructor
	CHouseDoorsController(CGS* pGS, std::string&& AccessData, std::string&& JsonDoorData, CHouseData* pHouse);

	// Destructor
	~CHouseDoorsController();

	// Getters
	ska::unordered_set<int>& GetAccesses() { return m_AccessUserIDs; } // Get the set of user IDs with access to the house
	ska::unordered_map<int, CHouseDoor*>& GetDoors() { return m_apDoors; } // Get the map of door numbers to CHouseDoor objects

	// Access control methods
	void AddAccess(int UserID); // Add access for a user
	void RemoveAccess(int UserID); // Remove access for a user
	bool HasAccess(int UserID); // Check if a user has access
	int GetAvailableAccessSlots() const; // Get the number of available access slots

	// Door control methods
	void Open(int Number); // Open a specific door
	void Close(int Number); // Close a specific door
	void Reverse(int Number); // Reverse the state of a specific door

	void OpenAll(); // Open all doors
	void CloseAll(); // Close all doors
	void ReverseAll(); // Reverse the state of all doors

private:
	void SaveAccessList() const; // Save the access list to a file
};
#endif