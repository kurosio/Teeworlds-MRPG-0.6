/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "tiles_handler.h"

#include <game/collision.h>

void CTileHandler::Handle(const vec2& Position)
{
	// initialize variables
	const int Indices[2] = {
		m_pCollision->GetParseTilesAt(Position.x, Position.y),
		m_pCollision->GetParseFrontTilesAt(Position.x, Position.y)
	};

	// handle tiles layers
	for(int i = 0; i < 2; ++i)
	{
		const int Index = Indices[i];
		if(Index >= TILE_AIR && Index < MAX_TILES)
		{
			if(m_MarkedTiles[i] != Index)
			{
				m_MarkEnter[i] = Index;
				m_MarkExit[i] = m_MarkedTiles[i];
				m_MarkedTiles[i] = Index;
			}
		}
	}
}

bool CTileHandler::IsEnter(int Index)
{
	bool Entered = false;
	for(int i = 0; i < 2; ++i)
	{
		if(Index == m_MarkEnter[i])
		{
			m_MarkEnter[i] = TILE_AIR;
			Entered = true;
		}
	}
	return Entered;
}

bool CTileHandler::IsExit(int Index)
{
	bool Exited = false;
	for(int i = 0; i < 2; ++i)
	{
		if(Index == m_MarkExit[i])
		{
			m_MarkExit[i] = TILE_AIR;
			Exited = true;
		}
	}
	return Exited;
}
