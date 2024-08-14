/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_TILES_HANDLER_H
#define GAME_SERVER_CORE_TILES_HANDLER_H

#define HANDLE_TILE_MOTD_MENU(pPlayer, pChr, tile, motdMenu) \
	if(pChr->GetTiles()->IsEnter(tile)) \
	{ \
		GS()->Broadcast(pPlayer->GetCID(), BroadcastPriority::GAME_INFORMATION, 50, "Welcome! Press the 'self kill' key to open the menu."); \
	} \
	else if(pChr->GetTiles()->IsExit(tile) && pPlayer->IsSameMotdMenu(motdMenu)) \
	{ \
		GS()->Broadcast(pPlayer->GetCID(), BroadcastPriority::MAIN_INFORMATION, 50, "Goodbye!"); \
		pPlayer->CloseMotdMenu(); \
	} \
	else if(pChr->GetTiles()->IsActive(tile)) \
	{ \
		if (Server()->Tick() % Server()->TickSpeed() == 0) \
			GS()->Broadcast(pPlayer->GetCID(), BroadcastPriority::GAME_INFORMATION, 50, "Welcome! Press the 'self kill' key to open the menu."); \
		\
		if (Server()->Input()->IsKeyClicked(ClientID, KEY_EVENT_SELF_KILL)) \
		{ \
			GS()->SendMenuMotd(pPlayer, motdMenu); \
		} \
	} \


#define HANDLE_TILE_VOTE_MENU(pPlayer, pChr, tile, voteMenu, enterActions, exitActions) \
	if (pChr->GetTiles()->IsEnter(tile)) \
	{ \
		GS()->Broadcast(pPlayer->GetCID(), BroadcastPriority::MAIN_INFORMATION, 70, "You can see menu in the votes!"); \
		pPlayer->m_VotesData.UpdateVotes(voteMenu); \
		enterActions \
	} \
	else if (pChr->GetTiles()->IsExit(tile)) \
	{ \
		GS()->Broadcast(pPlayer->GetCID(), BroadcastPriority::MAIN_INFORMATION, 70, "You have left the active zone!"); \
		pPlayer->m_VotesData.UpdateVotes(MENU_MAIN); \
		exitActions \
	}

// forward declaration
class CCollision;
class CCharacter;

// tile handler
class CTileHandler
{
	CCollision* m_pCollision {};
	CCharacter* m_pCharacter{};
	int m_MarkedTiles[2] {};
	int m_MarkEnter[2] {};
	int m_MarkExit[2] {};

public:
	explicit CTileHandler(CCollision* pCollision) : m_pCollision(pCollision) {}

	void Handle(const vec2& Position);

	// tiles
	bool IsEnter(int Index);
	bool IsExit(int Index);
	bool IsActive(int Index) const { return m_MarkedTiles[0] == Index || m_MarkedTiles[1] == Index; }

	// fold expression helpers
	template <typename ... Ts> bool AreAnyEnter(const Ts... args) { return ((IsEnter(args)) || ...); }
	template <typename ... Ts> bool AreAnyExit(const Ts... args) { return ((IsEnter(args)) || ...); }
	template <typename ... Ts> bool AreAnyActive(const Ts... args) const { return ((IsActive(args)) || ...); }
};

#endif