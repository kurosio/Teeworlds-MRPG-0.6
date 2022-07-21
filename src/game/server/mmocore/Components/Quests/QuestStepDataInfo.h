/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_QUEST_STEP_DATA_INFO_H
#define GAME_SERVER_COMPONENT_QUEST_STEP_DATA_INFO_H

#include <game/server/mmocore/Components/Bots/BotData.h>

class CGS;
class CPlayer;

// ##############################################################
// ################# GLOBAL STEP STRUCTURE ######################
class CQuestStepDataInfo
{
public:
	QuestBotInfo* m_Bot;

	void UpdateBot(CGS* pGS);
	bool IsActiveStep(CGS* pGS) const;
};

// ##############################################################
// ################# PLAYER STEP STRUCTURE ######################
class CPlayerQuestStepDataInfo : public CQuestStepDataInfo
{
public:
	int m_MobProgress[2];
	bool m_StepComplete;
	bool m_ClientQuitting;

	int GetValueBlockedItem(CPlayer* pPlayer, int ItemID) const;
	bool IsCompleteItems(CPlayer* pPlayer) const;
	bool IsCompleteMobs(CPlayer* pPlayer) const;
	bool Finish(CPlayer* pPlayer, bool FinalStepTalking);
	void DoCollectItem(CPlayer* pPlayer);

	void AddMobProgress(CPlayer* pPlayer, int BotID);
	void CreateStepArrow(CPlayer* pPlayer);
	void CreateStepDropTakeItems(CPlayer* pPlayer);
	void ShowRequired(CPlayer* pPlayer, const char* TextTalk);
};

#endif