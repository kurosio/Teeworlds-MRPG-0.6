/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_TILES_HANDLER_H
#define GAME_SERVER_CORE_TILES_HANDLER_H

#include <game/mapitems.h>

#define _DEF_TILE_ENTER_ZONE_IMPL(player, menus) \
									GS()->Broadcast(player->GetCID(), BroadcastPriority::MAIN_INFORMATION, 70, "You can see menu in the votes!"); \
									player->m_VotesData.UpdateVotes(menus)
#define _DEF_TILE_EXIT_ZONE_IMPL(player) \
									GS()->Broadcast(player->GetCID(), BroadcastPriority::MAIN_INFORMATION, 70, "You have left the active zone!"); \
									player->m_VotesData.UpdateVotes(MENU_MAIN)

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
