/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_BOT_CORE_H
#define GAME_SERVER_COMPONENT_BOT_CORE_H
#include <game/server/mmocore/MmoComponent.h>

#include "BotData.h"

class CBotCore : public MmoComponent
{
	~CBotCore() override
	{
		DataBotInfo::ms_aDataBot.clear();
		QuestBotInfo::ms_aQuestBot.clear();
		NpcBotInfo::ms_aNpcBot.clear();
		MobBotInfo::ms_aMobBot.clear();
	};

	void OnInit() override;
	void OnInitWorld(const char* pWhereLocalWorld) override;
	bool OnMessage(int MsgID, void* pRawMsg, int ClientID) override;

	void InitQuestBots(const char* pWhereLocalWorld);
	void InitNPCBots(const char* pWhereLocalWorld);
	void InitMobsBots(const char* pWhereLocalWorld);

	void SendChatDialog(bool PlayerTalked, int BotType, int MobID, int ClientID, const char* pText);

public:
	void DialogBotStepNPC(CPlayer* pPlayer, int MobID, int Progress, const char *pText = nullptr);
	void DialogBotStepQuest(CPlayer* pPlayer, int MobID, int Progress, bool ExecutionStep);
	static int GetQuestNPC(int MobID);
	static const char *GetMeaninglessDialog();
	
	bool ShowGuideDropByWorld(int WorldID, CPlayer* pPlayer);

	void ConAddCharacterBot(int ClientID, const char* pCharacter);
};

#endif