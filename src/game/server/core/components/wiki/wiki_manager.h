#ifndef GAME_SERVER_CORE_COMPONENTS_WIKI_WIKI_MANAGER_H
#define GAME_SERVER_CORE_COMPONENTS_WIKI_WIKI_MANAGER_H

#include <game/server/core/mmo_component.h>

#include "wiki_data.h"

class CWikiManager : public MmoComponent
{
public:
	void OnPreInit() override;
	bool OnSendMenuMotd(CPlayer* pPlayer, int Menulist) override;

	CWikiData* GetWikiData(int ID) const;
};

#endif