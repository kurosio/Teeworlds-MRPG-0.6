/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "QuestManager.h"

#include <engine/shared/config.h>
#include <game/server/gamecontext.h>

constexpr auto TW_QUESTS_DAILY_BOARD = "tw_quests_daily_boards";
constexpr auto TW_QUESTS_DAILY_BOARDS_LIST = "tw_quests_daily_board_list";

// This function is called during the initialization of the Quest Manager object.
void CQuestManager::OnInit()
{
	// Execute a SELECT query to retrieve all columns from the "tw_quests_list" table
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_quests_list");
	while(pRes->next())
	{
		// Retrieve the values for each column from the current row of the result set
		QuestIdentifier ID = pRes->getInt("ID");
		std::string Name = pRes->getString("Name").c_str();
		std::string Story = pRes->getString("StoryLine").c_str();
		int Gold = pRes->getInt("Money");
		int Exp = pRes->getInt("Exp");

		// Initialize a CQuestDescription object with the retrieved values
		CQuestDescription::CreateElement(ID)->Init(Name, Story, Gold, Exp);
	}

	// Create a container to store the daily quest data
	std::unordered_map< int, CQuestsDailyBoard::ContainerDailyQuests > DailyQuests;

	// Execute a SELECT query on the database and store the result pointer in pResBoard
	ResultPtr pResBoard = Database->Execute<DB::SELECT>("*", TW_QUESTS_DAILY_BOARD);
	while(pResBoard->next())
	{
		// Retrieve the values for each column from the current row of the result set
		QuestDailyBoardIdentifier ID = pResBoard->getInt("ID");
		std::string Name = pResBoard->getString("Name").c_str();
		vec2 Pos = vec2((float)pResBoard->getInt("PosX"), (float)pResBoard->getInt("PosY"));
		int WorldID = pResBoard->getInt("WorldID");

		// Initialize the daily quest with the server parameters
		CQuestsDailyBoard(ID).Init(Name, Pos, WorldID);
	}

	// Execute a SELECT query on the database and store the result pointer in pResDailyQuest
	ResultPtr pResDailyQuest = Database->Execute<DB::SELECT>("*", TW_QUESTS_DAILY_BOARDS_LIST);
	while(pResDailyQuest->next())
	{
		// Retrieve the values for each column from the current row of the result set
		QuestIdentifier QuestID = pResDailyQuest->getInt("QuestID");
		QuestDailyBoardIdentifier BoardID = pResDailyQuest->getInt("DailyBoardID");

		// Push the quest information into the corresponding daily quest container
		CQuestDescription* pQuestInfo = GS()->GetQuestInfo(QuestID);
		pQuestInfo->MarkDaily();
		DailyQuests[BoardID].push_back(pQuestInfo);
	}

	// Update the daily quest information for each daily quest board
	for(auto& [BoardID, DataContainer] : DailyQuests)
	{
		CQuestsDailyBoard::Data()[BoardID].m_vpDailyQuests = DataContainer;
	}
}

// This method is called when a player's account is initialized.
void CQuestManager::OnInitAccount(CPlayer* pPlayer)
{
	// Get the client ID of the player
	const int ClientID = pPlayer->GetCID();

	// Execute a select query to fetch all rows from the "tw_accounts_quests" table where UserID is equal to the ID of the player's account
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_accounts_quests", "WHERE UserID = '%d'", pPlayer->Account()->GetID());
	while(pRes->next())
	{
		// Get the QuestID and Type values from the current row
		QuestIdentifier ID = pRes->getInt("QuestID");
		QuestState State = (QuestState)pRes->getInt("Type");

		// Initialize a new instance of CPlayerQuest with the QuestID and ClientID, and set its state to the retrieved Type value
		CPlayerQuest::CreateElement(ID, ClientID)->Init(State);
	}
}

void CQuestManager::OnResetClient(int ClientID)
{
	for(auto& pQuest : CPlayerQuest::Data()[ClientID])
		delete pQuest.second;
	CPlayerQuest::Data().erase(ClientID);
}

bool CQuestManager::OnHandleTile(CCharacter* pChr, int IndexCollision)
{
	// Get the player object client ID associated with the character object
	CPlayer* pPlayer = pChr->GetPlayer();
	const int ClientID = pPlayer->GetCID();

	// Check if the player entered the shop zone
	if(pChr->GetHelper()->TileEnter(IndexCollision, TILE_DAILY_BOARD))
	{
		// Send message about entering the shop zone to the player
		_DEF_TILE_ENTER_ZONE_IMPL(pPlayer, MENU_DAILY_BOARD);
		return true;
	}
	// Check if the player exited the shop zone
	else if(pChr->GetHelper()->TileExit(IndexCollision, TILE_DAILY_BOARD))
	{
		// Send message about exiting the shop zone to the player
		_DEF_TILE_EXIT_ZONE_IMPL(pPlayer);
		return true;
	}

	return false;
}

bool CQuestManager::OnHandleMenulist(CPlayer* pPlayer, int Menulist)
{
	// Retrieve the character object client ID associated with the player
	CCharacter* pChr = pPlayer->GetCharacter();
	const int ClientID = pPlayer->GetCID();

	if(Menulist == MENU_DAILY_BOARD)
	{
		// Get the daily board for the player character's position
		CQuestsDailyBoard* pDailyBoard = GetDailyBoard(pChr->m_Core.m_Pos);
		ShowDailyQuestsBoard(pChr->GetPlayer(), pDailyBoard);

		// Show wanted players board
		ShowWantedPlayersBoard(pChr->GetPlayer());
		return true;
	}

	// Check if the Menulist is equal to MENU_JOURNAL_MAIN
	if(Menulist == MENU_JOURNAL_MAIN)
	{
		// Set the player's LastVoteMenu to MENU_MAIN
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);

		// Show the main list of quests to the player
		ShowQuestsMainList(pPlayer);

		// Add the Votes Backpage for the client
		CVoteWrapper::AddBackpage(ClientID);

		// Return true to indicate success
		return true;
	}

	// Check if `Menulist` is equal to `MENU_JOURNAL_FINISHED`
	if(Menulist == MENU_JOURNAL_FINISHED)
	{
		// Update the player's last vote menu to `MENU_JOURNAL_MAIN`
		pPlayer->m_VotesData.SetLastMenuID(MENU_JOURNAL_MAIN);

		// Show the list of quests in the finished state for the player
		ShowQuestsTabList(pPlayer, QuestState::FINISHED);

		// Add the Backpage votes for the player's client ID
		CVoteWrapper::AddBackpage(ClientID);

		// Return true to indicate success
		return true;
	}

	// If the Menulist is equal to MENU_JOURNAL_QUEST_INFORMATION, execute the following code
	if(Menulist == MENU_JOURNAL_QUEST_INFORMATION)
	{
		// Set the LastVoteMenu of the player to MENU_JOURNAL_MAIN
		pPlayer->m_VotesData.SetLastMenuID(MENU_JOURNAL_MAIN);

		// Get the quest information for the specified QuestID
		const int QuestID = pPlayer->m_VotesData.GetMenuTemporaryInteger();
		CQuestDescription* pQuestInfo = pPlayer->GetQuest(QuestID)->Info();

		// Show the active NPC for the quest to the player
		pPlayer->GS()->Core()->QuestManager()->ShowQuestActivesNPC(pPlayer, QuestID);

		// Add the vote information for the player's client
		/*
		pPlayer->GS()->AV(ClientID, "null");
		pPlayer->GS()->AVL(ClientID, "null", "{STR} : Reward", pQuestInfo->GetName());
		pPlayer->GS()->AVL(ClientID, "null", "Gold: {VAL} Exp: {INT}", pQuestInfo->GetRewardGold(), pQuestInfo->GetRewardExp());
		*/

		// Add the votes back page to the player's client
		CVoteWrapper::AddBackpage(ClientID);

		// Return true to indicate success
		return true;
	}

	return false;
}

bool CQuestManager::OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, const int VoteID, const int VoteID2, int Get, const char* GetText)
{
	const int ClientID = pPlayer->GetCID();
	if(PPSTR(CMD, "DAILY_QUEST_STATE") == 0)
	{
		// Get the daily quest board for VoteID2
		CQuestsDailyBoard* pBoard = GS()->GetQuestDailyBoard(VoteID2);

		// If the daily quest board is not found, return true
		if(!pBoard)
			return true;

		// Check if there are quests available for the player on the board
		if(!pBoard->QuestsAvailables(pPlayer))
		{
			// If there are more than 3 assignments per day that can be accepted, send a chat message to the client with the error message
			GS()->Chat(ClientID, "More than 3 assignments per day cannot be accepted/finished.");
			return true;
		}

		// If there is no quest associated with the VoteID or the quest is already completed, return true
		CPlayerQuest* pQuest = pPlayer->GetQuest(VoteID);
		if(!pQuest || pQuest->IsCompleted())
			return true;

		// If the quest is active, refuse it. Otherwise, accept it.
		if(pQuest->IsAccepted())
			pQuest->Refuse();
		else
			pQuest->Accept();

		// Return true to indicate the action was successful and update votes
		pPlayer->m_VotesData.UpdateCurrentVotes();
		return true;
	}

	return false;
}

// This function is called when a player's time period changes in the quest manager
void CQuestManager::OnPlayerHandleTimePeriod(CPlayer* pPlayer, TIME_PERIOD Period)
{
	// Get the client ID of the player
	int ClientID = pPlayer->GetCID();

	// If the time period is set to DAILY_STAMP
	if(Period == TIME_PERIOD::DAILY_STAMP)
	{
		// Reset the daily quests for the specified player
		for(auto& p : CQuestsDailyBoard::Data())
		{
			p.second.ClearDailyQuests(pPlayer);
		}

		// Call the UpdateVotes function to update the player's voting menu and send chat message
		GS()->Chat(ClientID, "The daily quests have been updated.");
		GS()->Chat(ClientID, "Visit the quest board for new quests!");
		pPlayer->m_VotesData.UpdateCurrentVotes();
	}
}

// function to get the name of a quest state based on the given quest state
static const char* GetStateName(QuestState State)
{
	// switch statement to check the value of the quest state
	switch(State)
	{
		// if the quest state is ACCEPT, return "Active"
		case QuestState::ACCEPT:
		return "Active";

		// if the quest state is FINISHED, return "Finished"
		case QuestState::FINISHED:
		return "Finished";

		// for any other quest state, return "Not active"
		default:
		return "Not active";
	}
}

void CQuestManager::ShowQuestsMainList(CPlayer* pPlayer)
{
	int ClientID = pPlayer->GetCID();

	// information
	const int TotalQuests = (int)CQuestDescription::Data().size();
	const int TotalComplectedQuests = GetCountComplectedQuests(ClientID);
	const int TotalIncomplectedQuests = TotalQuests - TotalComplectedQuests;

	CVoteWrapper VInfo(ClientID, VWF_STYLE_STRICT, "Quests statistic");
	VInfo.Add("Total quests: {INT}", TotalQuests);
	VInfo.Add("Total complected quests: {INT}", TotalComplectedQuests);
	VInfo.Add("Total incomplete quests: {INT}", TotalIncomplectedQuests);

	// tabs with quests
	ShowQuestsTabList(pPlayer, QuestState::ACCEPT);
	ShowQuestsTabList(pPlayer, QuestState::NO_ACCEPT);

	// show the completed menu
	CVoteWrapper(ClientID).AddMenu(MENU_JOURNAL_FINISHED, "List of completed quests");
}

void CQuestManager::ShowQuestsTabList(CPlayer* pPlayer, QuestState State)
{
	const int ClientID = pPlayer->GetCID();
	CVoteWrapper(ClientID).Add("{STR} quests", GetStateName(State));

	// check first quest story step
	bool IsEmptyList = true;
	std::list < std::string /*stories was checked*/ > StoriesChecked;
	for(const auto& [ID, pQuestInfo] : CQuestDescription::Data())
	{
		if(pPlayer->GetQuest(ID)->GetState() != State)
			continue;

		if(State == QuestState::FINISHED)
		{
			ShowQuestID(pPlayer, ID);
			IsEmptyList = false;
			continue;
		}

		const auto& IsAlreadyChecked = std::find_if(StoriesChecked.begin(), StoriesChecked.end(), 
			[pStory = pQuestInfo->GetStory()](const std::string& stories)
		{
			return (str_comp_nocase(pStory, stories.c_str()) == 0);
		});

		if(IsAlreadyChecked == StoriesChecked.end())
		{
			StoriesChecked.emplace_back(pQuestInfo->GetStory());
			ShowQuestID(pPlayer, ID);
			IsEmptyList = false;
		}
	}

	// if the quest list is empty
	if(IsEmptyList)
	{
		CVoteWrapper(ClientID).Add("List of quests is empty");
	}

	CVoteWrapper::AddLine(ClientID);
}

// This function displays the information about a specific quest
void CQuestManager::ShowQuestID(CPlayer* pPlayer, int QuestID) const
{
	// Get the quest information for the given QuestID
	CQuestDescription* pQuestInfo = pPlayer->GetQuest(QuestID)->Info();

	// If the quest is a daily ques
	if(pQuestInfo->IsDaily())
		return;

	// Get the size of the quest's story and the current position in the story
	// Display the quest information to the player using the AVD() function
	const int QuestsSize = pQuestInfo->GetStoryQuestsNum();
	const int QuestPosition = pQuestInfo->GetStoryQuestPosition();
	CVoteWrapper(pPlayer->GetCID()).AddMenu(MENU_JOURNAL_QUEST_INFORMATION, QuestID, "{INT}/{INT} {STR}: {STR}",
		QuestPosition, QuestsSize, pQuestInfo->GetStory(), pQuestInfo->GetName());
}

// active npc information display
void CQuestManager::ShowQuestActivesNPC(CPlayer* pPlayer, int QuestID) const
{
	CPlayerQuest* pPlayerQuest = pPlayer->GetQuest(QuestID);
	const int ClientID = pPlayer->GetCID();

	CVoteWrapper(ClientID).Add("Active NPC for current quests");
	/*for(auto& pStepBot : CQuestDescription::Data()[QuestID]->m_vSteps)
	{
		const QuestBotInfo& BotInfo = pStepBot.second.m_Bot;
		if(!BotInfo.m_HasAction)
			continue;

		const vec2 Pos = BotInfo.m_Position / 32.0f;
		CQuestStep& rQuestStepDataInfo = pPlayerQuest->m_vSteps[pStepBot.first];
		const char* pSymbol = (((pPlayerQuest->GetState() == QuestState::ACCEPT && rQuestStepDataInfo.m_StepComplete) || pPlayerQuest->GetState() == QuestState::FINISHED) ? "✔ " : "\0");

		CVoteWrapper VStep(ClientID, VWF_UNIQUE|VWF_STYLE_SIMPLE, "{STR}Step {INT}. {STR} {STR}(x{INT} y{INT})", 
			pSymbol, BotInfo.m_StepPos, BotInfo.GetName(), Server()->GetWorldName(BotInfo.m_WorldID), (int)Pos.x, (int)Pos.y);

		// skipped non accepted task list
		if(pPlayerQuest->GetState() != QuestState::ACCEPT)
		{
			VStep.Add("Quest been completed, or not accepted!");
			continue;
		}

		// show required defeat
		bool NoTasks = true;
		if(!BotInfo.m_vRequiredDefeat.empty())
		{
			for(auto& p : BotInfo.m_vRequiredDefeat)
			{
				if(DataBotInfo::ms_aDataBot.find(p.m_BotID) != DataBotInfo::ms_aDataBot.end())
				{
					VStep.Add("- Defeat {STR} [{INT}/{INT}]", DataBotInfo::ms_aDataBot[p.m_BotID].m_aNameBot, rQuestStepDataInfo.m_aMobProgress[p.m_BotID].m_Count, p.m_Value);
					NoTasks = false;
				}
			}
		}

		// show required item's
		if(!BotInfo.m_vRequiredItems.empty())
		{
			for(auto& pRequired : BotInfo.m_vRequiredItems)
			{
				CPlayerItem* pPlayerItem = pPlayer->GetItem(pRequired.m_Item);
				int ClapmItem = clamp(pPlayerItem->GetValue(), 0, pRequired.m_Item.GetValue());

				VStep.Add("- Item {STR} [{VAL}/{VAL}]", pPlayerItem->Info()->GetName(), ClapmItem, pRequired.m_Item.GetValue());
				NoTasks = false;
			}
		}

		// show reward item's
		if(!BotInfo.m_RewardItems.empty())
		{
			for(auto& pRewardItem : BotInfo.m_RewardItems)
			{
				VStep.Add("- Receive {STR}x{VAL}", pRewardItem.Info()->GetName(), pRewardItem.GetValue());
			}
		}

		// show move to
		if(!BotInfo.m_vRequiredMoveAction.empty())
		{
			VStep.Add("- Some action is required");
		}

		if(NoTasks)
		{
			VStep.Add("- No task");
		}
	}*/
}

void CQuestManager::QuestShowRequired(CPlayer* pPlayer, QuestBotInfo& pBot, char* aBufQuestTask, int Size)
{
	const int QuestID = pBot.m_QuestID;
	CPlayerQuest* pQuest = pPlayer->GetQuest(QuestID);
	CQuestStep* pStep = pQuest->GetStepByMob(pBot.m_ID);
	pStep->FormatStringTasks(aBufQuestTask, Size);
}

void CQuestManager::AppendDefeatProgress(CPlayer* pPlayer, int DefeatedBotID)
{
	// TODO Optimize algoritm check complected steps
	const int ClientID = pPlayer->GetCID();
	for(auto& [ID, pQuest] : CPlayerQuest::Data()[ClientID])
	{
		// only for accepted quests
		if(pQuest->GetState() != QuestState::ACCEPT)
			continue;

		// check current steps and append
		for(auto& pStepBot : pQuest->m_vSteps)
			pStepBot.AppendDefeatProgress(DefeatedBotID);
	}
}

void CQuestManager::ShowWantedPlayersBoard(CPlayer* pPlayer) const
{
	const int ClientID = pPlayer->GetCID();

	CVoteWrapper VWanted(ClientID, VWF_SEPARATE_CLOSED|VWF_STYLE_SIMPLE, "Wanted players list");
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		CPlayer* pPlayer = GS()->GetPlayer(i, true);
		if(!pPlayer || !pPlayer->Account()->IsRelationshipsDeterioratedToMax())
			continue;

		CPlayerItem* pItemGold = pPlayer->GetItem(itGold);
		const int Reward = minimum(translate_to_percent_rest(pItemGold->GetValue(), (float)g_Config.m_SvArrestGoldAtDeath), pItemGold->GetValue());
		VWanted.Add("{STR} (Reward {VAL} gold)", Server()->ClientName(i), Reward);
		{
			VWanted.BeginDepth();
			VWanted.Add("Last seen: {STR}", Server()->GetWorldName(pPlayer->GetPlayerWorldID()));
			VWanted.EndDepth();
		}
	}
}

void CQuestManager::ShowDailyQuestsBoard(CPlayer* pPlayer, CQuestsDailyBoard* pBoard) const
{
	// Get the client's ID
	const int ClientID = pPlayer->GetCID();

	// Check if the pBoard variable is null
	if(!pBoard)
	{
		CVoteWrapper(ClientID).Add("Daily board don't work");
		return;
	}

	// Daily board information
	CVoteWrapper VDailyBoard(ClientID, VWF_STYLE_STRICT_BOLD, "Daily board: {STR}", pBoard->GetName());
	VDailyBoard.Add("Acceptable quests: ({INT} of {INT})", pBoard->QuestsAvailables(pPlayer), (int)MAX_DAILY_QUESTS_BY_BOARD);
	VDailyBoard.AddItemValue(itAlliedSeals);
	VDailyBoard.AddLine();

	for(const auto& pDailyQuestInfo : pBoard->m_vpDailyQuests)
	{
		// If the quest is completed, skip to the next iteration
		const auto* pQuest = pPlayer->GetQuest(pDailyQuestInfo->GetID());
		if(pQuest->IsCompleted())
			continue;

		// Determine the state indicator and action name based on whether the quest is active or not
		const char* StateIndicator = (pQuest->IsAccepted() ? "✔" : "×");
		const char* ActionName = (pQuest->IsAccepted() ? "Refuse" : "Accept");
		const char* QuestName = pDailyQuestInfo->GetName();

		// Display the quest information to the player
		CVoteWrapper VQuest(ClientID, VWF_UNIQUE | VWF_STYLE_SIMPLE, "({STR}) {STR}", StateIndicator, QuestName);
		VQuest.Add("Reward:");
		{
			VQuest.BeginDepth();
			VQuest.Add("Gold: {VAL}", pDailyQuestInfo->GetRewardGold());
			VQuest.Add("Exp: {VAL}", pDailyQuestInfo->GetRewardExp());
			VQuest.Add("{STR}: {VAL}", GS()->GetItemInfo(itAlliedSeals)->GetName(), g_Config.m_SvDailyQuestAlliedSealsReward);
			VQuest.EndDepth();
		}
		VQuest.AddLine();
		VQuest.AddOption("DAILY_QUEST_STATE", pDailyQuestInfo->GetID(), pBoard->GetID(), "{STR} quest", ActionName);
		VQuest.AddLine();
	}

	CVoteWrapper::AddLine(ClientID);
}

// The function takes a parameter "Pos" of type "vec2" and returns a pointer to "CQuestsDailyBoard" object
CQuestsDailyBoard* CQuestManager::GetDailyBoard(vec2 Pos) const
{
	// Iterate through the map "CQuestsDailyBoard::Data()"
	for(auto it = CQuestsDailyBoard::Data().begin(); it != CQuestsDailyBoard::Data().end(); ++it)
	{
		// Check if the distance between the position of the current "CQuestsDailyBoard" object and the given "Pos" is less than 200
		if(distance(it->second.GetPos(), Pos) < 200)
			return &(it->second);
	}

	return nullptr;
}

void CQuestManager::Update(CPlayer* pPlayer)
{
	const int ClientID = pPlayer->GetCID();
	for(auto& [ID, pQuest] : CPlayerQuest::Data()[ClientID])
	{
		if(pQuest->GetState() != QuestState::ACCEPT)
			continue;

		pQuest->Update();
	}
}

void CQuestManager::TryAcceptNextStoryQuest(CPlayer* pPlayer, int CheckQuestID)
{
	// Check if the quest with CheckQuestID is a daily quest
	CQuestDescription* pVerifyQuestInfo = GS()->GetQuestInfo(CheckQuestID);
	if(!pVerifyQuestInfo || pVerifyQuestInfo->IsDaily())
		return;

	// Loop through all quest descriptions
	for(auto& [valQuestID, pQuestInfo] : CQuestDescription::Data())
	{
		// Check if the story of the current quest description is the same as the story of CheckingQuest
		if(str_comp_nocase(pVerifyQuestInfo->GetStory(), pQuestInfo->GetStory()) == 0)
		{
			CPlayerQuest* pQuest = pPlayer->GetQuest(valQuestID);
			if(pQuestInfo->IsDaily() || pQuest->GetState() == QuestState::FINISHED)
				continue;

			if(pQuest->GetState() == QuestState::ACCEPT)
				break;

			pQuest->Accept();
		}
	}
}

void CQuestManager::AcceptNextStoryQuestStep(CPlayer* pPlayer)
{
	// Create a list to store the stories that have been checked
	std::list<std::string> StoriesChecked;

	// Loop through each active quest for the player
	for(const auto& [ID, pQuest] : CPlayerQuest::Data()[pPlayer->GetCID()])
	{
		// Check if the quest is finished, if not, skip it
		CQuestDescription* pQuestInfo = GS()->GetQuestInfo(ID);
		if(pQuest->GetState() != QuestState::FINISHED)
			continue;

		// Check if the quest is a daily quest, if yes, skip it
		if(pQuestInfo->IsDaily())
			continue;

		// Check if the story of the quest has already been checked
		const auto& IsAlreadyChecked = std::find_if(StoriesChecked.begin(), StoriesChecked.end(),
			[pQuestInfo](const std::string& stories)
			{
				return (str_comp_nocase(pQuestInfo->GetStory(), stories.c_str()) == 0);
			});

		// If the story has not been checked, add it to the checked list and accept the next story quest
		if(IsAlreadyChecked == StoriesChecked.end())
		{
			StoriesChecked.emplace_front(pQuestInfo->GetStory());
			TryAcceptNextStoryQuest(pPlayer, ID);
		}
	}
}

int CQuestManager::GetUnfrozenItemValue(CPlayer* pPlayer, int ItemID) const
{
	const int ClientID = pPlayer->GetCID();
	int AvailableValue = pPlayer->GetItem(ItemID)->GetValue();
	for(const auto& [ID, pQuest] : CPlayerQuest::Data()[ClientID])
	{
		if(pQuest->GetState() != QuestState::ACCEPT)
			continue;

		/*for(auto& pStepBot : pQuest->m_vSteps)
		{
			if(!pStepBot.second.m_StepComplete)
				AvailableValue -= pStepBot.second.GetNumberBlockedItem(ItemID);
		}*/
	}
	return maximum(AvailableValue, 0);
}

// This function returns the count of completed quests for a specific client
int CQuestManager::GetCountComplectedQuests(int ClientID) const
{
	// Initialize the total count of completed quests to 0
	int Total = 0;

	for(const auto& [ID, pQuest] : CPlayerQuest::Data()[ClientID])
	{
		// Check if the quest data is marked as completed
		if(pQuest->IsCompleted())
			Total++;
	}

	// Return the total count of completed quests
	return Total;
}