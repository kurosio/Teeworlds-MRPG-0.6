/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_BOT_CORE_H
#define GAME_SERVER_COMPONENT_BOT_CORE_H
#include <game/server/core/mmo_component.h>

#include "BotData.h"

class CBotManager : public MmoComponent
{
	~CBotManager() override
	{
		// free data
		mystd::freeContainer(DataBotInfo::ms_aDataBot, QuestBotInfo::ms_aQuestBot, NpcBotInfo::ms_aNpcBot, MobBotInfo::ms_aMobBot);
	};

	void OnPreInit() override;
	void OnInitWorld(const std::string& SqlQueryWhereWorld) override;
	bool OnClientMessage(int MsgID, void* pRawMsg, int ClientID) override;

	void InitQuestBots(const char* pWhereLocalWorld);
	void InitNPCBots(const char* pWhereLocalWorld);
	void InitMobsBots(const char* pWhereLocalWorld);

public:
	static int GetQuestNPC(int MobID);
	void ConAddCharacterBot(int ClientID, const char* pCharacter);
};

#endif