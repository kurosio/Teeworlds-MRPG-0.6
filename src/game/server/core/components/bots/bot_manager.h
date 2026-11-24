/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_COMPONENTS_BOTS_BOT_MANAGER_H
#define GAME_SERVER_CORE_COMPONENTS_BOTS_BOT_MANAGER_H
#include <game/server/core/mmo_component.h>

#include "bot_data.h"

class CBotManager : public MmoComponent
{
	~CBotManager() override
	{
		// free data
		mystd::freeContainer(DataBotInfo::ms_aDataBot, QuestBotInfo::ms_aQuestBot, NpcBotInfo::ms_aNpcBot, MobBotInfo::ms_aMobBot);
	};

	void OnPreInit() override;
	void OnInitWorld(int WorldID) override;

	void InitQuestBots(int WorldID);
	void InitNPCBots(int WorldID);
	void InitMobsBots(int WorldID);

public:
	static int GetQuestNPC(int MobID);
	void ConAddCharacterBot(int ClientID, const char* pCharacter);
};

#endif