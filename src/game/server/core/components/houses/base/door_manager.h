#ifndef GAME_SERVER_COMPONENT_HOUSES_BASE_DOOR_MANAGER_H
#define GAME_SERVER_COMPONENT_HOUSES_BASE_DOOR_MANAGER_H

#include "interface_house.h"

class CEntityHouseDoor;
class CDoorManager
{
	CGS* GS() const;
	IHouse* m_pHouse {};
	ska::unordered_map<int, CEntityHouseDoor*> m_apEntDoors {};

public:
	CDoorManager() = delete;
	CDoorManager(IHouse* pHouse, const std::string& DoorsData);
	~CDoorManager();

	ska::unordered_map<int, CEntityHouseDoor*>& GetContainer() { return m_apEntDoors; }
	void Open(int Number);
	void Close(int Number);
	void Reverse(int Number);
	void OpenAll();
	void CloseAll();
	void ReverseAll();

	void AddDoor(const std::string& Doorname, vec2 Position);
	void RemoveDoor(const std::string& Doorname, vec2 Position);
};

#endif