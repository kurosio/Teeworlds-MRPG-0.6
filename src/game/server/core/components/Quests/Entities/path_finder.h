/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_MMOCORE_COMPONENTS_QUESTS_ENTITIES_PATH_FINDER_H
#define GAME_SERVER_MMOCORE_COMPONENTS_QUESTS_ENTITIES_PATH_FINDER_H

#include <game/server/entity.h>

class CEntityPathArrow : public CEntity
{
	int m_WorldID;
	float m_AreaClipped;
	bool* m_pComplete;
	class CPlayer* m_pPlayer;

public:
	CEntityPathArrow(CGameWorld* pGameWorld, vec2 SearchPos, int WorldID, int ClientID, float AreaClipped, bool* pComplete);

	void Tick() override;
	void Snap(int SnappingClient) override;
};

#endif
