#ifndef GAME_SERVER_CORE_COMPONENTS_WIKI_WIKI_MANAGER_H
#define GAME_SERVER_CORE_COMPONENTS_WIKI_WIKI_MANAGER_H

#include <game/server/core/mmo_component.h>

#include "wiki_data.h"

class CWikiManager : public MmoComponent
{
	~CWikiManager() override
	{
		mystd::freeContainer(CWikiData::Data());
	}

	void OnPreInit() override;
	void OnConsoleInit() override;
	bool OnSendMenuMotd(CPlayer* pPlayer, int Menulist) override;

	CWikiData* GetWikiData(int ID) const;
	static void ConReloadWiki(IConsole::IResult* pResult, void* pUserData);
};

#endif