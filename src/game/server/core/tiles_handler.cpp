/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "tiles_handler.h"

#include <game/collision.h>

void CTileHandler::Handle(const vec2& Position)
{
	// initialize variables
	const int Index = m_pCollision->GetParseTilesAt(Position.x, Position.y);

	// check valid index
	if(Index < TILE_AIR || Index >= MAX_TILES)
		return;

	// update to new index
	if(m_Marked != Index)
	{
		m_MarkEnter = Index;
		m_MarkExit = m_Marked;
		m_Marked = Index;
	}
}

bool CTileHandler::IsEnter(int Index)
{
	if (Index == m_MarkEnter)
	{
		m_MarkEnter = TILE_AIR;
		return true;
	}
	return false;
}

bool CTileHandler::IsExit(int Index)
{
	if(Index == m_MarkExit)
	{
		m_MarkExit = TILE_AIR;
		return true;
	}
	return false;
}