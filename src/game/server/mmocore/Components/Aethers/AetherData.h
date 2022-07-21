/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_AETHER_DATA_H
#define GAME_SERVER_COMPONENT_AETHER_DATA_H

struct CAetherData
{
	int m_TeleX;
	int m_TeleY;
	int m_WorldID;
	char m_aName[64];

	static std::map< int, CAetherData > ms_aTeleport;
};

#endif