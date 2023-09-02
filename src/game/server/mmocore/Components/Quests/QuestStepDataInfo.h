/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_QUEST_STEP_DATA_INFO_H
#define GAME_SERVER_COMPONENT_QUEST_STEP_DATA_INFO_H

#include <game/server/mmocore/Components/Bots/BotData.h>

class CGS;
class CPlayer;

// ##############################################################
// ################# GLOBAL STEP STRUCTURE ######################
class CQuestStepDescription
{
public:
	QuestBotInfo m_Bot{};
	void UpdateBot();
	bool IsActiveStep(CGS* pGS) const;
};

// ##############################################################
// ################# PLAYER STEP STRUCTURE ######################
class CPlayerQuestStep : public CQuestStepDescription
{
public:
	std::unordered_map < int /*BotID*/, int/*Count*/ > m_aMobProgress { };
	std::deque < bool > m_aMoveToProgress{};

	bool m_StepComplete{};
	bool m_ClientQuitting{};

	int GetValueBlockedItem(CPlayer* pPlayer, int ItemID) const;
	bool IsComplete(CPlayer* pPlayer);
	bool Finish(CPlayer* pPlayer);
	void PostFinish(CPlayer* pPlayer);

	void AppendDefeatProgress(CPlayer* pPlayer, int DefeatedBotID);
	void UpdatePathNavigator(int ClientID);
	void CreateVarietyTypesRequiredItems(CPlayer* pPlayer);
	void FormatStringTasks(CPlayer* pPlayer, char* aBufQuestTask, int Size);

	void Update(int ClientID);
};

#endif