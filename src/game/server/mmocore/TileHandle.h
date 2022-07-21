/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_TILE_HANDLE_H
#define GAME_SERVER_TILE_HANDLE_H

#include <game/mapitems.h>

class TileHandle
{
	bool m_Collide[MAX_TILES]{};

public:
	TileHandle() = default;

	// tiles
	bool TileEnter(int IndexPlayer, int IndexNeed);
	bool TileExit(int IndexPlayer, int IndexNeed);
	bool BoolIndex(int Index) const { return m_Collide[Index]; }
};

#endif
