#ifndef GAME_SERVER_GAMEMODES_DUNGEON_H
#define GAME_SERVER_GAMEMODES_DUNGEON_H

#include <game/server/entity.h>

class CEntityDungeonWaitingDoor : public CEntity
{
	int m_Closed {};

public:
	CEntityDungeonWaitingDoor(CGameWorld *pGameWorld, vec2 Pos);

	void Tick() override;
	void Snap(int SnappingClient) override;

	void Open() { m_Closed = false; }
	void Close() { m_Closed = true; }
};

#endif
