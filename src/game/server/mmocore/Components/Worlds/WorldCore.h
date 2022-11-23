/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_WORLDSWAP_CORE_H
#define GAME_SERVER_COMPONENT_WORLDSWAP_CORE_H

#include <game/server/mmocore/MmoComponent.h>

#include "WorldData.h"

class CWorldDataCore : public MmoComponent
{
	~CWorldDataCore() override
	{
		CWorldData::Data().clear();
		CWorldSwapPosition::ms_aWorldPositionLogic.clear();
	};

	void OnInitWorld(const char* pWhereLocalWorld) override;

public:
	int GetWorldType() const;
	void FindPosition(int WorldID, vec2 Pos, vec2* OutPos);
	void CheckQuestingOpened(CPlayer* pPlayer, int QuestID) const;
};

#endif