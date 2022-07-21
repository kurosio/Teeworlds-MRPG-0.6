/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_WORLDSWAP_DATA_H
#define GAME_SERVER_COMPONENT_WORLDSWAP_DATA_H

struct CWorldSwapData
{
	int m_RequiredQuestID;
	vec2 m_Position[2];
	int m_WorldID[2];

	static std::map< int, CWorldSwapData > ms_aWorldSwap;
};
struct CWorldSwapPosition
{
	int m_BaseWorldID;
	int m_FindWorldID;
	vec2 m_Position;

	static std::list< CWorldSwapPosition > ms_aWorldPositionLogic;
};

#endif