/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_TILES_HANDLER_H
#define GAME_SERVER_CORE_TILES_HANDLER_H

#include <game/mapitems.h>

#define DEF_TILE_ENTER_ZONE_IMPL(player, menus) \
									GS()->Broadcast(player->GetCID(), BroadcastPriority::MAIN_INFORMATION, 70, "You can see menu in the votes!"); \
									player->m_VotesData.UpdateVotes(menus)
#define DEF_TILE_EXIT_ZONE_IMPL(player) \
									GS()->Broadcast(player->GetCID(), BroadcastPriority::MAIN_INFORMATION, 70, "You have left the active zone!"); \
									player->m_VotesData.UpdateVotes(MENU_MAIN)

// forward declaration
class CGS;
class CCharacter;

// tile handler
class CTileHandler
{
	CGS* m_pGS {};
	CCharacter* m_pCharacter{};
	int m_Marked{};
	int m_MarkEnter{};
	int m_MarkExit{};

public:
	explicit CTileHandler(CGS* pGS, CCharacter* pCharacter) : m_pGS(pGS), m_pCharacter(pCharacter) {}


	void Handler();

	// tiles
	bool IsEnter(int Index);
	bool IsExit(int Index);
	bool IsActive(int Index) const { return m_Marked == Index; }
};

#endif
