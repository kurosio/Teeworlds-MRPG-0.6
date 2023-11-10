/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GROUP_CORE_H
#define GAME_SERVER_COMPONENT_GROUP_CORE_H
#include <game/server/mmocore/MmoComponent.h>

#include "GroupData.h"

class CGroupManager : public MmoComponent
{
	~CGroupManager() override
	{
		GroupData::Data().clear();
	}

	void OnInit() override;

public:
};

#endif
