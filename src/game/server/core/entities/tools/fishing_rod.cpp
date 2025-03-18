#include "fishing_rod.h"

#include <game/server/gamecontext.h>

CEntityFishingRod::CEntityFishingRod(CGameWorld* pGameWorld, int ClientID, vec2 Position)
	: CEntity(pGameWorld, CGameWorld::ENTTYPE_PATH_FINDER, Position, 0, ClientID)
{
	GameWorld()->InsertEntity(this);
}

void CEntityFishingRod::Tick()
{
}

void CEntityFishingRod::Snap(int SnappingClient)
{
}