#ifndef GAME_SERVER_WORLDMODES_DUNGEON_ENTITIES_PROGRESS_DOOR_H
#define GAME_SERVER_WORLDMODES_DUNGEON_ENTITIES_PROGRESS_DOOR_H

#include <game/server/entity.h>

class CEntityDungeonProgressDoor : public CEntity
{
	int m_BotID;
	bool m_OpenedDoor;

public:
	CEntityDungeonProgressDoor(CGameWorld* pGameWorld, vec2 Pos, int BotID);

	void Tick() override;
	void Snap(int SnappingClient) override;

	bool Update();
	void ResetDoor() { m_OpenedDoor = false; };
};

#endif
