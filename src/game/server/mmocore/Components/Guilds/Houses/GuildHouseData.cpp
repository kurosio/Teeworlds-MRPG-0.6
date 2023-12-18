/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "GuildHouseData.h"

CGuildHouseData::~CGuildHouseData()
{
	delete m_pDecorations;
	delete m_pDoors;
}
