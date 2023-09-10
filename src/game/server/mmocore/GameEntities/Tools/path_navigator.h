/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_PATH_NAVIGATOR_POINT_H
#define GAME_SERVER_ENTITIES_PATH_NAVIGATOR_POINT_H

#include <game/server/entity.h>

#include "game/server/mmocore/PathFinderData.h"

class CEntityPathNavigator : public CEntity
{
	int m_WorldID {};
	int m_ClientID {};
	CPathFinderPrepared m_Data{};

public:
	CEntityPathNavigator(CGameWorld* pGameWorld, vec2 StartPos, vec2 EndPos, int ClientID, int WorldID);

	void SetNavigatorPositions(vec2 StartPos, vec2 EndPos, int WorldID);

	void Tick() override;
	void Snap(int SnappingClient) override;
};

#endif
