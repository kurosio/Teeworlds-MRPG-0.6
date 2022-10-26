/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_HOUSE_DATA_H
#define GAME_SERVER_COMPONENT_HOUSE_DATA_H

struct CHouseData
{
	int m_PosX;
	int m_PosY;
	int m_DoorX;
	int m_DoorY;
	int m_Price;
	char m_aClass[32];
	int m_UserID;
	int m_Bank;
	int m_PlantID;
	int m_PlantPosX;
	int m_PlantPosY;
	int m_WorldID;
	class HouseDoor* m_Door;

	static std::map< int, CHouseData > ms_aHouse;
};

#endif