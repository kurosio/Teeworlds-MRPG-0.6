/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_PATH_NAVIGATOR_POINT_H
#define GAME_SERVER_ENTITIES_PATH_NAVIGATOR_POINT_H

#include <game/server/entity.h>

#include "game/server/core/utilities/pathfinder_data.h"

class CEntityPathNavigator : public CEntity
{
	bool m_StartByCreating{};
	int m_StepPos {};
	CEntity* m_pParent {};
	CPathFinderPrepared m_Data {};
	vec2 m_LastPos {};
	int64_t m_Mask {};
	int m_TickLastIdle {};
	int m_TickCountDown {};
	bool m_Projectile {};

public:
	CEntityPathNavigator(CGameWorld* pGameWorld, CEntity* pParent, bool StartByCreating, vec2 FromPos, vec2 SearchPos, int WorldID, bool Projectile, int64_t Mask = -1);
	bool PreparedPathData();

	ska::unordered_map<int, vec2> getFinishedContainer(ska::unordered_map<int, vec2>& pathContainer);

	void Tick() override;
	void Snap(int SnappingClient) override;
	void PostSnap() override;

private:
	void Move();
};

#endif
