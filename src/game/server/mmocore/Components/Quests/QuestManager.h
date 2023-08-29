/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_QUEST_CORE_H
#define GAME_SERVER_COMPONENT_QUEST_CORE_H
#include <game/server/mmocore/MmoComponent.h>

#include "QuestData.h"

enum QuestInteractive
{
	// INTERACTIVE_RANDOM_ACCEPT_ITEM = 1, // Todo this, it is necessary to lead to a fully understandable view of the conversation steps and refactoring it
	// INTERACTIVE_DROP_AND_TAKE_IT = 2,
	//INTERACTIVE_SHOW_ITEMS = 3,
};

class CQuestManager : public MmoComponent
{
	~CQuestManager() override
	{
		CQuestDataInfo::Data().clear();
		CQuestData::Data().clear();
	}

	void OnInit() override;
	void OnInitAccount(CPlayer* pPlayer) override;
	void OnResetClient(int ClientID) override;
	bool OnHandleMenulist(CPlayer* pPlayer, int Menulist, bool ReplaceMenu) override;
	bool OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, int VoteID, int VoteID2, int Get, const char* GetText) override;

public:
	bool IsValidQuest(int QuestID, int ClientID = -1) const
	{
		if (CQuestDataInfo::Data().find(QuestID) != CQuestDataInfo::Data().end())
		{
			if (ClientID < 0 || ClientID >= MAX_PLAYERS)
				return true;
			if (CQuestData::Data()[ClientID].find(QuestID) != CQuestData::Data()[ClientID].end())
				return true;
		}
		return false;
	}

	void ShowQuestsMainList(CPlayer* pPlayer);
	void ShowQuestsActiveNPC(CPlayer* pPlayer, int QuestID);

private:
	void ShowQuestsTabList(CPlayer* pPlayer, QuestState State);
	void ShowQuestID(CPlayer *pPlayer, int QuestID);

public:
	void QuestShowRequired(CPlayer* pPlayer, QuestBotInfo& pBot, char* aBufQuestTask, int Size);

	void AddMobProgressQuests(CPlayer* pPlayer, int BotID);

	void UpdateArrowStep(CPlayer *pPlayer);
	void AcceptNextStoryQuest(CPlayer* pPlayer, int CheckQuestID);
	void AcceptNextStoryQuestStep(CPlayer* pPlayer);
	int GetUnfrozenItemValue(CPlayer* pPlayer, int ItemID) const;
	int GetClientComplectedQuestsSize(int ClientID) const;
};

#endif