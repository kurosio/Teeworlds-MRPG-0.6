/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_TILES_HANDLER_H
#define GAME_SERVER_CORE_TILES_HANDLER_H

#include <game/mapitems.h>

#define _DEF_TILE_ENTER_ZONE_SEND_MSG_INFO(player) GS()->Broadcast(player->GetCID(), BroadcastPriority::MAIN_INFORMATION, 70, "You can see menu in the votes!")
#define _DEF_TILE_EXIT_ZONE_SEND_MSG_INFO(player) \
									GS()->Broadcast(player->GetCID(), BroadcastPriority::MAIN_INFORMATION, 70, "You have left the active zone!"); \
									player->m_ZoneInvertMenu = false; \
									player->m_ZoneMenuSelectedID = -1

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
