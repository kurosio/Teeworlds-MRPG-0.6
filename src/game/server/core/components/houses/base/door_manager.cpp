#include "door_manager.h"
#include <game/server/gamecontext.h>

#include "../entities/house_door.h"


CGS* CDoorManager::GS() const
{
	return m_pHouse->GS();
}


CDoorManager::CDoorManager(IHouse* pHouse, const std::string& DoorsData)
{
	// initialize variables
	m_pHouse = pHouse;

	// load doors from json
	mystd::json::parse(DoorsData, [this](nlohmann::json& pJson)
	{
		for(const auto& pDoor : pJson)
		{
			const auto Doorname = pDoor.value("name", "");
			const auto Position = pDoor.value("position", vec2());
			AddDoor(Doorname, Position);
		}
	});
}


CDoorManager::~CDoorManager()
{
	// delete all doors
	for(auto& p : m_apEntDoors)
		delete p.second;

	// clear container
	m_apEntDoors.clear();
}


void CDoorManager::Open(int Number)
{
	if(m_apEntDoors.find(Number) != m_apEntDoors.end())
		m_apEntDoors[Number]->Open();
}


void CDoorManager::Close(int Number)
{
	if(m_apEntDoors.find(Number) != m_apEntDoors.end())
		m_apEntDoors[Number]->Close();
}


void CDoorManager::Reverse(int Number)
{
	if(m_apEntDoors.find(Number) != m_apEntDoors.end())
		m_apEntDoors[Number]->Reverse();
}


void CDoorManager::OpenAll()
{
	for(auto& p : m_apEntDoors)
		Open(p.first);
}


void CDoorManager::CloseAll()
{
	for(auto& p : m_apEntDoors)
		Close(p.first);
}


void CDoorManager::ReverseAll()
{
	for(auto& p : m_apEntDoors)
		Reverse(p.first);
}


void CDoorManager::AddDoor(const std::string& Doorname, vec2 Position)
{
	m_apEntDoors.emplace(m_apEntDoors.size() + 1, new CEntityHouseDoor(&GS()->m_World, m_pHouse, Doorname, Position));
}


void CDoorManager::RemoveDoor(const std::string& Doorname, vec2 Position)
{
	// find the door in the m_apEntDoors
	auto iter = std::find_if(m_apEntDoors.begin(), m_apEntDoors.end(), [&](const std::pair<int, CEntityHouseDoor*>& p)
	{
		return p.second->GetName() == Doorname && p.second->GetPos() == Position;
	});

	// implement removal
	if(iter != m_apEntDoors.end())
	{
		delete iter->second;
		m_apEntDoors.erase(iter);
	}
}