/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "tiles_handler.h"

#include <game/collision.h>

void CTileHandler::Handle(const vec2& Position)
{
	// initialize variables
	const int Indices[TILES_LAYER_NUM] =
	{
		m_pCollision->GetMainTileIndex(Position.x, Position.y),
		m_pCollision->GetFrontTileIndex(Position.x, Position.y),
		m_pCollision->GetExtraTileIndex(Position.x, Position.y)
	};

	// handle tiles layers
	for(int i = 0; i < TILES_LAYER_NUM; ++i)
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
	for(int i = 0; i < TILES_LAYER_NUM; ++i)
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
	for(int i = 0; i < TILES_LAYER_NUM; ++i)
	{
		if(Index == m_MarkExit[i])
		{
			m_MarkExit[i] = TILE_AIR;
			Exited = true;
		}
	}
	return Exited;
}
