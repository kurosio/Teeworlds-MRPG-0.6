/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GUILD_HOUSE_DOORS_CONTROLLER_H
#define GAME_SERVER_COMPONENT_GUILD_HOUSE_DOORS_CONTROLLER_H

#include "GuildHouseDoorData.h"

class CGS;
class CGuildHouseData;

class CGuildHouseDoorsController
{
	CGS* GS() const;

	CGuildHouseData* m_pHouse {};
	ska::unordered_map<int, CGuildHouseDoor*> m_apDoors {};

public:
	CGuildHouseDoorsController() = delete;
	CGuildHouseDoorsController(std::string&& JsonDoorData, CGuildHouseData* pHouse);
	~CGuildHouseDoorsController();

	ska::unordered_map<int, CGuildHouseDoor*>& GetContainer() { return m_apDoors; }

	// Door control methods
	void Open(int Number); // Open a specific door
	void Close(int Number); // Close a specific door
	void Reverse(int Number); // Reverse the state of a specific door

	void OpenAll(); // Open all doors
	void CloseAll(); // Close all doors
	void ReverseAll(); // Reverse the state of all doors

private:
	void Init(std::string&& JsonDoorData, CGuildHouseData* pHouse);
};
#endif