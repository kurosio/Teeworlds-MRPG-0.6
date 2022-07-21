/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "TileHandle.h"

bool TileHandle::TileEnter(int IndexPlayer, int IndexNeed)
{
	if(IndexPlayer == IndexNeed && !m_Collide[IndexNeed])
	{
		m_Collide[IndexNeed] = true;
		return true;
	}
	return false;
}
bool TileHandle::TileExit(int IndexPlayer, int IndexNeed)
{
	if(IndexPlayer != IndexNeed && m_Collide[IndexNeed])
	{
		m_Collide[IndexNeed] = false;
		return true;
	}
	return false;
}