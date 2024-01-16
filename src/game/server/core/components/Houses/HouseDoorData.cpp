/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "Entities/HouseDoor.h"
#include "HouseData.h"

// Constructor for CHouseDoor class
CHouseDoor::CHouseDoor(CGameWorld* pWorld, CHouseData* pHouse, std::string&& Name, vec2 Pos)
	: m_Name(std::move(Name)), m_Pos(Pos)
{
	// Create a new CEntityHouseDoor object and assign it to m_pDoor
	m_pDoor = new CEntityHouseDoor(pWorld, Pos, this, pHouse);
}

// Destructor for CHouseDoor class
CHouseDoor::~CHouseDoor()
{
	// Delete the CEntityHouseDoor object and set m_pDoor to nullptr
	delete m_pDoor;
	m_pDoor = nullptr;
}

// Check if the door is closed
bool CHouseDoor::IsClosed() const
{
	return m_pDoor->IsClosed();
}

// Open the door
void CHouseDoor::Open() const
{
	m_pDoor->Open();
}

// Close the door
void CHouseDoor::Close() const
{
	m_pDoor->Close();
}