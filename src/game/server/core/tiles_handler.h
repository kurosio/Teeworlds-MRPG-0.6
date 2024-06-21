/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_TILES_HANDLER_H
#define GAME_SERVER_CORE_TILES_HANDLER_H

#define DEF_TILE_ENTER_ZONE_IMPL(player, menus) \
									GS()->Broadcast(player->GetCID(), BroadcastPriority::MAIN_INFORMATION, 70, "You can see menu in the votes!"); \
									player->m_VotesData.UpdateVotes(menus)
#define DEF_TILE_EXIT_ZONE_IMPL(player) \
									GS()->Broadcast(player->GetCID(), BroadcastPriority::MAIN_INFORMATION, 70, "You have left the active zone!"); \
									player->m_VotesData.UpdateVotes(MENU_MAIN)

// forward declaration
class CCollision;
class CCharacter;

// tile handler
class CTileHandler
{
	CCollision* m_pCollision {};
	CCharacter* m_pCharacter{};
	int m_Marked{};
	int m_MarkEnter{};
	int m_MarkExit{};

public:
	explicit CTileHandler(CCollision* pCollision) : m_pCollision(pCollision) {}

	void Handle(const vec2& Position);

	// tiles
	bool IsEnter(int Index);
	bool IsExit(int Index);
	bool IsActive(int Index) const { return m_Marked == Index; }
	int GetActive() const { return m_Marked; }

	// fold expression helpers
	template <typename ... Ts> bool AreAnyEnter(const Ts... args) { return ((IsEnter(args)) || ...); }
	template <typename ... Ts> bool AreAnyExit(const Ts... args) { return ((IsEnter(args)) || ...); }
	template <typename ... Ts> bool AreAnyActive(const Ts... args) const { return ((m_Marked == args) || ...); }
};

#endif