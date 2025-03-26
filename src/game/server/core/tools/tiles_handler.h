/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_TOOLS_TILES_HANDLER_H
#define GAME_SERVER_CORE_TOOLS_TILES_HANDLER_H

constexpr int TILES_LAYER_NUM = 3;

#define HANDLE_TILE_MOTD_MENU(pPlayer, pChr, tile, motdMenu) \
	if(pChr->GetTiles()->IsEnter(tile)) \
	{ \
		GS()->SendMenuMotd(pPlayer, motdMenu); \
	} \
	else if(pChr->GetTiles()->IsExit(tile) && pPlayer->IsSameMotdMenu(motdMenu)) \
	{ \
		GS()->Broadcast(pPlayer->GetCID(), BroadcastPriority::MainInformation, 50, "Goodbye!"); \
		pPlayer->CloseMotdMenu(); \
	} \
	else if(pChr->GetTiles()->IsActive(tile)) \
	{ \
		if (Server()->Tick() % Server()->TickSpeed() == 0) \
			GS()->Broadcast(pPlayer->GetCID(), BroadcastPriority::GameInformation, 50, "Welcome! Press the 'self kill' key to open the menu."); \
		\
		if (Server()->Input()->IsKeyClicked(ClientID, KEY_EVENT_SELF_KILL)) \
		{ \
			GS()->SendMenuMotd(pPlayer, motdMenu); \
		} \
	} \


#define HANDLE_TILE_VOTE_MENU(pPlayer, pChr, tile, voteMenu, enterActions, exitActions) \
	if (pChr->GetTiles()->IsEnter(tile)) \
	{ \
		GS()->Broadcast(pPlayer->GetCID(), BroadcastPriority::MainInformation, 70, "You can see menu in the votes!"); \
		pPlayer->m_VotesData.UpdateVotes(voteMenu); \
		enterActions \
	} \
	else if (pChr->GetTiles()->IsExit(tile)) \
	{ \
		GS()->Broadcast(pPlayer->GetCID(), BroadcastPriority::MainInformation, 70, "You have left the active zone!"); \
		pPlayer->m_VotesData.UpdateVotes(MENU_MAIN); \
		exitActions \
	}

class CCollision;
class CCharacter;
class CTileHandler
{
	CCollision* m_pCollision {};
	CCharacter* m_pCharacter {};
	int m_MarkedTiles[TILES_LAYER_NUM] {};
	int m_MarkEnter[TILES_LAYER_NUM] {};
	int m_MarkExit[TILES_LAYER_NUM] {};
	int m_MoveRestrictions {};

public:
	explicit CTileHandler(CCollision* pCollision, CCharacter* pCharacter)
		: m_pCollision(pCollision), m_pCharacter(pCharacter) {}

	void Handle(const vec2& Position);

	// tiles
	bool IsEnter(int Index);
	bool IsExit(int Index);
	bool IsActive(int Index) const { return m_MarkedTiles[0] == Index || m_MarkedTiles[1] == Index || m_MarkedTiles[2] == Index; }

	// fold expression helpers
	template <typename ... Ts> bool AreAnyEnter(const Ts... args) { return ((IsEnter(args)) || ...); }
	template <typename ... Ts> bool AreAnyExit(const Ts... args) { return ((IsEnter(args)) || ...); }
	template <typename ... Ts> bool AreAnyActive(const Ts... args) const { return ((IsActive(args)) || ...); }
};

#endif