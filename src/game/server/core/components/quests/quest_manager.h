/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_QUEST_CORE_H
#define GAME_SERVER_COMPONENT_QUEST_CORE_H
#include <game/server/core/mmo_component.h>

#include "quest_board_data.h"
#include "quest_data.h"

/*
 * CQuestManager class is a subclass of MmoComponent class.
 * It is responsible for managing quests in the game.
 * It inherits all the properties and methods of the MmoComponent class.
 */
class CQuestManager : public MmoComponent
{
	// Destructor which overrides the base class destructor
	~CQuestManager() override
	{
		// free data
		mrpgstd::free_container(CQuestDescription::Data(), CPlayerQuest::Data());
	}

	void OnInit() override;
	void OnPlayerLogin(CPlayer* pPlayer) override;
	void OnClientReset(int ClientID) override;
	bool OnCharacterTile(CCharacter* pChr) override;
	bool OnPlayerMenulist(CPlayer* pPlayer, int Menulist) override;
	bool OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, int Extra1, int Extra2, int ReasonNumber, const char* pReason) override;
	void OnPlayerTimePeriod(CPlayer* pPlayer, ETimePeriod Period) override;

public:
	// Check if a given QuestID is valid for a given ClientID
	bool IsValidQuest(int QuestID, int ClientID = -1) const
	{
		// Check if the QuestID exists in the list of available quests
		if(CQuestDescription::Data().find(QuestID) != CQuestDescription::Data().end())
		{
			// Check if the ClientID is within the valid range
			if(ClientID < 0 || ClientID >= MAX_PLAYERS)
				return true;

			// Check if the ClientID has the given QuestID
			if(CPlayerQuest::Data()[ClientID].find(QuestID) != CPlayerQuest::Data()[ClientID].end())
				return true;
		}

		return false;
	}

	// Function to display the main list of quests for a player
	void ShowQuestsMainList(CPlayer* pPlayer);

	// Function to show active quests npc for a player with a specific quest ID
	void ShowQuestActivesNPC(CPlayer* pPlayer, int QuestID) const;

private:
	// Function to show a list of quests for a player, filtered by their state.
	// Parameters:
	// - pPlayer: pointer to the player for whom to display the quests
	// - State: the state of the quests to display (e.g. active, completed, etc.)
	void ShowQuestsTabList(CPlayer* pPlayer, QuestState State);

	// Function to show the details of a specific quest for a player.
	// Parameters:
	// - pPlayer: pointer to the player for whom to display the quest details
	// - QuestID: the ID of the quest to display
	void ShowQuestID(CPlayer* pPlayer, int QuestID) const;

public:
	void QuestShowRequired(CPlayer* pPlayer, QuestBotInfo& pBot, char* aBufQuestTask, int Size);

	void AppendDefeatProgress(CPlayer* pPlayer, int DefeatedBotID);

	void AppendQuestBoardGroup(CPlayer* pPlayer, CQuestsBoard* pBoard, class VoteWrapper* pWrapper, int QuestFlag) const;
	void ShowQuestsBoardList(CPlayer* pPlayer, CQuestsBoard* pBoard) const;
	void ShowQuestsBoardQuest(CPlayer* pPlayer, CQuestsBoard* pBoard, int QuestID) const;

	// Function: GetBoardByPos
	// Input: Pos - a 2D vector representing the position
	// Return: a pointer to a CQuestsBoard object
	CQuestsBoard* GetBoardByPos(vec2 Pos) const;
	void ResetPeriodQuests(CPlayer* pPlayer, ETimePeriod Period) const;


	void Update(CPlayer* pPlayer);
	void TryAcceptNextQuestChain(CPlayer* pPlayer, int BaseQuestID) const;
	void TryAcceptNextQuestAll(CPlayer* pPlayer) const;
	int GetUnfrozenItemValue(CPlayer* pPlayer, int ItemID) const;
	int GetCountComplectedQuests(int ClientID) const;
};

#endif