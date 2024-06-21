/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_QUEST_CORE_H
#define GAME_SERVER_COMPONENT_QUEST_CORE_H
#include <game/server/core/mmo_component.h>

#include "QuestDailyBoardData.h"
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
		std::ranges::for_each(CPlayerQuest::Data(), [](auto& pair)
		{
			mrpgstd::cleaning_free_container_data(pair.second);
		});
		mrpgstd::cleaning_free_container_data(CQuestDescription::Data(), CPlayerQuest::Data());
	}

	// This function is called when the module is initialized
	void OnInit() override;

	// This function is called when the player's account is initialized
	void OnPlayerLogin(CPlayer* pPlayer) override;

	// This function is called when the client is reset
	void OnClientReset(int ClientID) override;

	// This function is called when a tile collision is handled by a character
	bool OnCharacterTile(CCharacter* pChr) override;

	// This function is called when a menu list is handled by a player
	bool OnPlayerMenulist(CPlayer* pPlayer, int Menulist) override;

	// This function is called when a vote command is handled by a player
	bool OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, int Extra1, int Extra2, int ReasonNumber, const char* pReason) override;

	// This function is called when a time period is handled by a player
	void OnPlayerTimePeriod(CPlayer* pPlayer, TIME_PERIOD Period) override;

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
	void ShowWantedPlayersBoard(CPlayer* pPlayer) const;
	void ShowDailyQuestsBoard(CPlayer* pPlayer, CQuestsDailyBoard* pBoard) const;

	// Function: GetDailyBoard
	// Input: Pos - a 2D vector representing the position
	// Return: a pointer to a CQuestsDailyBoard object
	CQuestsDailyBoard* GetDailyBoard(vec2 Pos) const;

	void Update(CPlayer* pPlayer);
	void TryAcceptNextStoryQuest(CPlayer* pPlayer, int CheckQuestID);
	void AcceptNextStoryQuestStep(CPlayer* pPlayer);
	int GetUnfrozenItemValue(CPlayer* pPlayer, int ItemID) const;
	int GetCountComplectedQuests(int ClientID) const;
};

#endif