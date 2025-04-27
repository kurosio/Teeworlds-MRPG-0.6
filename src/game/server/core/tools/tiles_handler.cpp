/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "tiles_handler.h"

#include <game/collision.h>

void CTileHandler::Handle(int Index)
{
	// initialize variables
	const int Indices[TILES_LAYER_NUM] =
	{
		m_pCollision->GetMainTileIndex(Index),
		m_pCollision->GetFrontTileIndex(Index),
		m_pCollision->GetExtraTileIndex(Index)
	};


	// handle tiles layers
	for(int i = 0; i < TILES_LAYER_NUM; ++i)
	{
		const int TileIndex = Indices[i];
		if(TileIndex >= TILE_AIR && TileIndex < MAX_TILES)
		{
			if(m_MarkedTiles[i] != TileIndex)
			{
				m_MarkEnter[i] = TileIndex;
				m_MarkExit[i] = m_MarkedTiles[i];
				m_MarkedTiles[i] = TileIndex;
			}
		}
	}
}

bool CTileHandler::IsEnter(int TileIndex)
{
	bool Entered = false;
	for(int i = 0; i < TILES_LAYER_NUM; ++i)
	{
		if(TileIndex == m_MarkEnter[i])
		{
			m_MarkEnter[i] = TILE_AIR;
			Entered = true;
		}
	}
	return Entered;
}

bool CTileHandler::IsExit(int TileIndex)
{
	bool Exited = false;
	for(int i = 0; i < TILES_LAYER_NUM; ++i)
	{
		if(TileIndex == m_MarkExit[i])
		{
			m_MarkExit[i] = TILE_AIR;
			Exited = true;
		}
	}
	return Exited;
}
