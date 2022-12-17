/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "HouseDoorData.h"

#include "game/server/gamecontext.h"
#include "Entities/HouseDoor.h"
#include "HouseData.h"

// house door data
CHouseDoorData::~CHouseDoorData()
{
	delete m_pDoor;
	m_pDoor = nullptr;
}

void CHouseDoorData::Open()
{
	if(m_pDoor)
	{
		delete m_pDoor;
		m_pDoor = nullptr;
	}
}

void CHouseDoorData::Close()
{
	if(!m_pDoor)
		m_pDoor = new HouseDoor(&m_pGS->m_World, m_Pos, this);
}

void CHouseDoorData::Reverse()
{
	if(m_pDoor)
		Open();
	else
		Close();
}

void CHouseDoorData::AddAccess(int UserID)
{
	if(m_AccessUserIDs.size() >= MAX_HOUSE_INVITED_PLAYERS)
	{
		m_pGS->ChatAccount(m_pHouse->GetAccountID(), "You have reached the limit of the allowed players!");
		return;
	}

	auto p = std::find_if(m_AccessUserIDs.begin(), m_AccessUserIDs.end(), [UserID](const int& Item){ return UserID == Item; });
	if(p == m_AccessUserIDs.end())
	{
		m_AccessUserIDs.push_back(UserID);
		SaveAccessList();
	}
}

void CHouseDoorData::RemoveAccess(int UserID)
{
	auto p = std::find_if(m_AccessUserIDs.begin(), m_AccessUserIDs.end(), [UserID](const int& Item){ return UserID == Item; });
	if(p != m_AccessUserIDs.end())
	{
		m_AccessUserIDs.erase(p);
		SaveAccessList();
	}
}

bool CHouseDoorData::HasAccess(int UserID)
{
	if(m_pHouse->GetAccountID() == UserID)
		return true;
	if(!m_AccessUserIDs.empty())
		return std::find_if(m_AccessUserIDs.begin(), m_AccessUserIDs.end(), [UserID](const int& p){ return p == UserID; }) != m_AccessUserIDs.end();
	return false;
}

int CHouseDoorData::GetAvailableAccessSlots() const
{
	return (int)MAX_HOUSE_INVITED_PLAYERS - m_AccessUserIDs.size();
}

void CHouseDoorData::SaveAccessList() const
{
	std::string AccessData = "";
	for(const auto& UID : m_AccessUserIDs)
		AccessData += std::to_string(UID) + ",";

	if(!AccessData.empty())
		AccessData.pop_back();
	Database->Execute<DB::UPDATE>("tw_houses", "AccessData = '%s' WHERE ID = '%d'", AccessData.c_str(), m_pHouse->GetID());
}
