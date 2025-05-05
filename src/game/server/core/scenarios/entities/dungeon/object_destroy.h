/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_SCENARIOS_ENTITIES_DUNGEON_OBJECT_DESTROY_H
#define GAME_SERVER_SCENARIOS_ENTITIES_DUNGEON_OBJECT_DESTROY_H

#include <game/server/entity.h>

class CEntityObjectDestroy : public CEntity
{
	int m_NumClick {};
	int m_CurrentClick {};
	int m_RespawnTick {};

public:
	CEntityObjectDestroy(CGameWorld* pGameWorld, vec2 Pos, int NumClick);

	void Tick() override;
	void Snap(int SnappingClient) override;

	bool IsActive() const { return !m_RespawnTick; }

private:
	void Spawn();
};

#endif
