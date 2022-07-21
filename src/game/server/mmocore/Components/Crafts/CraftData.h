/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_CRAFT_DATA_H
#define GAME_SERVER_COMPONENT_CRAFT_DATA_H

struct CCraftData
{
	int m_aRequiredItemID[3];
	int	m_aRequiredItemsValues[3];
	int m_ItemID;
	int m_ItemValue;
	int m_Price;
	int m_WorldID;

	static std::map<int, CCraftData> ms_aCraft;
};

#endif