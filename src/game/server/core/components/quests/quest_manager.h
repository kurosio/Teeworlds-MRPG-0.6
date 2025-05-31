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
		for(auto& [AID, vPlayerQuests] : CPlayerQuest::Data())
		{
			for(auto [QID, pQuest] : vPlayerQuests)
				delete pQuest;
		}
		CPlayerQuest::Data().clear();

		mystd::freeContainer(CQuestsBoard::Data());
		mystd::freeContainer(CQuestDescription::Data());
	}

	void OnPreInit() override;
	void OnPlayerLogin(CPlayer* pPlayer) override;
	void OnClientReset(int ClientID) override;
	void OnCharacterTile(CCharacter* pChr) override;
	bool OnSendMenuVotes(CPlayer* pPlayer, int Menulist) override;
	bool OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, int Extra1, int Extra2, int ReasonNumber, const char* pReason) override;
	void OnPlayerTimePeriod(CPlayer* pPlayer, ETimePeriod Period) override;

public:
	void ShowQuestList(CPlayer* pPlayer) const;
	void ShowQuestsTabList(const char* pTabname, CPlayer* pPlayer, QuestState State) const;

	CQuestsBoard* GetBoardByPos(vec2 Pos) const;
	void ShowQuestsBoardList(CPlayer* pPlayer, CQuestsBoard* pBoard) const;
	void ShowQuestInfo(CPlayer* pPlayer, CQuestDescription* pQuest, bool fromBoard) const;

	void PrepareRequiredBuffer(CPlayer* pPlayer, QuestBotInfo& pBot, char* aBufQuestTask, int Size);
	void TryAppendDefeatProgress(CPlayer* pPlayer, int DefeatedBotID);

	void ResetPeriodQuests(CPlayer* pPlayer, ETimePeriod Period) const;
	void Update(CPlayer* pPlayer);
	void TryAcceptNextQuestChain(CPlayer* pPlayer, int BaseQuestID) const;
	void TryAcceptNextQuestChainAll(CPlayer* pPlayer) const;
	int GetUnfrozenItemValue(CPlayer* pPlayer, int ItemID) const;
	int GetCountCompletedQuests(int ClientID) const;
};

#endif