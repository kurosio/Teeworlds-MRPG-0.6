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

	void InitQuestBots(const char* pWhereLocalWorld);
	void InitNPCBots(const char* pWhereLocalWorld);
	void InitMobsBots(const char* pWhereLocalWorld);

public:
	void ProcessingTalkingNPC(int OwnID, int TalkingID, const char* Message, int Emote, int TalkedFlag = TALKED_FLAG_FULL) const;
	void DialogBotStepNPC(CPlayer* pPlayer, int MobID, int Progress, int TalkedID, const char *pText = "empty");
	void DialogBotStepQuest(CPlayer* pPlayer, int MobID, int Progress, int TalkedID);
	void ShowBotQuestTaskInfo(CPlayer* pPlayer, int MobID, int Progress);
	static int GetQuestNPC(int MobID);
	static const char *GetMeaninglessDialog();

	void ConAddCharacterBot(int ClientID, const char* pCharacter);
};

#endif