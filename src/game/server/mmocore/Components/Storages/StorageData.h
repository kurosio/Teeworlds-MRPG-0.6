/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_STORAGE_DATA_H
#define GAME_SERVER_COMPONENT_STORAGE_DATA_H

struct CStorageData
{
	char m_aName[32];
	int m_PosX;
	int m_PosY;
	int m_Currency;
	int m_WorldID;

	static std::map< int, CStorageData > ms_aStorage;
};

#endif