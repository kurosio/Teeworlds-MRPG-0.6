/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "tiles_handler.h"

#include <game/server/gamecontext.h>
#include <game/server/entities/character.h>

void CTileHandler::Handler()
{
	// initialize variables
	const int Index = m_pGS->Collision()->GetParseTilesAt(m_pCharacter->m_Core.m_Pos.x, m_pCharacter->m_Core.m_Pos.y);

	// check valid index
	if(Index < 0 || Index >= MAX_TILES)
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
