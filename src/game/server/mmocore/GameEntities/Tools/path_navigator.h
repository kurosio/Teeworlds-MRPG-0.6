/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_PATH_NAVIGATOR_POINT_H
#define GAME_SERVER_ENTITIES_PATH_NAVIGATOR_POINT_H

#include <game/server/entity.h>

#include "game/server/mmocore/PathFinderData.h"

class CEntityPathNavigator : public CEntity
{
	bool m_Snapping {};
	int m_StepPos {};
	CEntity* m_pParent {};
	CPathFinderPrepared m_Data {};
	int m_TickLastIdle {};
	vec2 m_LastPos {};
	int64 m_Mask {};

public:
	CEntityPathNavigator(CGameWorld* pGameWorld, CEntity* pParent, vec2 StartPos, vec2 SearchPos, int WorldID, int64 Mask = -1);

	void Tick() override;
	void PostSnap() override;
};

#endif
