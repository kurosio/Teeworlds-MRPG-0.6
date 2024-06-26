/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "quest_manager.h"

#include <game/server/gamecontext.h>

void CQuestManager::OnInit()
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_quests_list");
	while(pRes->next())
	{
		// initialize variables
		QuestIdentifier ID = pRes->getInt("ID");
		int nextQuestID = pRes->getInt("NextQuestID");
		DBSet flagSet = std::string(pRes->getString("Flags").c_str());
		std::string Name = pRes->getString("Name").c_str();
		int Gold = pRes->getInt("Money");
		int Exp = pRes->getInt("Exp");

		// create new element
		auto optionalNextQuestID = nextQuestID > 0 ? std::optional(nextQuestID) : std::nullopt;
		CQuestDescription::CreateElement(ID)->Init(Name, Gold, Exp, optionalNextQuestID, flagSet);
	}

	// initialize boards
	ResultPtr pResBoard = Database->Execute<DB::SELECT>("*", TW_QUEST_BOARDS_TABLE);
	while(pResBoard->next())
	{
		// initialize variables
		int ID = pResBoard->getInt("ID");
		std::string Name = pResBoard->getString("Name").c_str();
		vec2 Pos = vec2((float)pResBoard->getInt("PosX"), (float)pResBoard->getInt("PosY"));
		int WorldID = pResBoard->getInt("WorldID");

		// create new element
		CQuestsBoard::CreateElement(ID)->Init(Name, Pos, WorldID);
	}

	// initialize board quests
	std::unordered_map< int, std::deque<CQuestDescription*> > vInitializeList;
	ResultPtr pResDailyQuest = Database->Execute<DB::SELECT>("*", TW_QUESTS_DAILY_BOARD_LIST);
	while(pResDailyQuest->next())
	{
		// initialize variables
		QuestIdentifier QuestID = pResDailyQuest->getInt("QuestID");
		int BoardID = pResDailyQuest->getInt("DailyBoardID");

		// add quest to board
		vInitializeList[BoardID].push_back(GS()->GetQuestInfo(QuestID));
	}
	for(auto& [BoardID, DataContainer] : vInitializeList)
		CQuestsBoard::Data()[BoardID]->m_vpQuests = DataContainer;
}

void CQuestManager::OnPlayerLogin(CPlayer* pPlayer)
{
	// initialize player quests
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_accounts_quests", "WHERE UserID = '%d'", pPlayer->Account()->GetID());
	while(pRes->next())
	{
		// initialize variables
		QuestIdentifier ID = pRes->getInt("QuestID");
		QuestState State = (QuestState)pRes->getInt("Type");

		// create new element
		CPlayerQuest::CreateElement(ID, pPlayer->GetCID())->Init(State);
	}
}

void CQuestManager::OnClientReset(int ClientID)
{
	mrpgstd::free_container(CPlayerQuest::Data()[ClientID]);
}

bool CQuestManager::OnCharacterTile(CCharacter* pChr)
{
	// initialize variables
	CPlayer* pPlayer = pChr->GetPlayer();

	// quest board
	if(pChr->GetTiles()->IsEnter(TILE_QUEST_BOARD))
	{
		DEF_TILE_ENTER_ZONE_IMPL(pPlayer, MENU_QUEST_BOARD);
		return true;
	}
	else if(pChr->GetTiles()->IsExit(TILE_QUEST_BOARD))
	{
		DEF_TILE_EXIT_ZONE_IMPL(pPlayer);
		return true;
	}

	return false;
}

bool CQuestManager::OnPlayerMenulist(CPlayer* pPlayer, int Menulist)
{
	// initialize variables
	CCharacter* pChr = pPlayer->GetCharacter();
	const int ClientID = pPlayer->GetCID();

	// quest board
	if(Menulist == MENU_QUEST_BOARD)
	{
		CQuestsBoard* pBoard = GetBoardByPos(pChr->m_Core.m_Pos);
		ShowQuestsBoardList(pChr->GetPlayer(), pBoard);
		return true;
	}

	// quest board selected
	if(Menulist == MENU_QUEST_BOARD_SELECTED)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_QUEST_BOARD);
		CQuestsBoard* pBoard = GetBoardByPos(pChr->m_Core.m_Pos);
		ShowQuestsBoardQuest(pPlayer, pBoard, pPlayer->m_VotesData.GetExtraID());
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
		VoteWrapper::AddBackpage(ClientID);

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
		VoteWrapper::AddBackpage(ClientID);

		// Return true to indicate success
		return true;
	}

	// If the Menulist is equal to MENU_JOURNAL_QUEST_INFORMATION, execute the following code
	if(Menulist == MENU_JOURNAL_QUEST_INFORMATION)
	{
		// Set the LastVoteMenu of the player to MENU_JOURNAL_MAIN
		pPlayer->m_VotesData.SetLastMenuID(MENU_JOURNAL_MAIN);

		// Get the quest information for the specified QuestID
		const int QuestID = pPlayer->m_VotesData.GetExtraID();
		CQuestDescription* pQuestInfo = pPlayer->GetQuest(QuestID)->Info();

		// Show the active NPC for the quest to the player
		pPlayer->GS()->Core()->QuestManager()->ShowQuestActivesNPC(pPlayer, QuestID);

		// Add the vote information for the player's client
		/*
		pPlayer->GS()->AV(ClientID, "null");
		pPlayer->GS()->AVL(ClientID, "null", "{} : Reward", pQuestInfo->GetName());
		pPlayer->GS()->AVL(ClientID, "null", "Gold: {} Exp: {}", pQuestInfo->GetRewardGold(), pQuestInfo->GetRewardExp());
		*/

		// Add the votes back page to the player's client
		VoteWrapper::AddBackpage(ClientID);

		// Return true to indicate success
		return true;
	}

	return false;
}

bool CQuestManager::OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, const int Extra1, const int Extra2, int ReasonNumber, const char* pReason)
{
	const int ClientID = pPlayer->GetCID();
	if(PPSTR(pCmd, "QUEST_STATE") == 0)
	{
		// check valid board
		CQuestsBoard* pBoard = GS()->GetQuestBoard(Extra2);
		if(!pBoard)
			return true;

		// check valid quest
		CPlayerQuest* pQuest = pPlayer->GetQuest(Extra1);
		if(!pQuest || pQuest->IsCompleted())
			return true;

		// toggle quest state
		if(pQuest->IsAccepted())
			pQuest->Refuse();
		else
			pQuest->Accept();

		// update current votes
		pPlayer->m_VotesData.UpdateCurrentVotes();
		return true;
	}

	return false;
}

// This function is called when a player's time period changes in the quest manager
void CQuestManager::OnPlayerTimePeriod(CPlayer* pPlayer, ETimePeriod Period)
{
	// Get the client ID of the player
	int ClientID = pPlayer->GetCID();

	// daily reset quests
	if(Period == DAILY_STAMP)
	{
		ResetPeriodQuests(pPlayer, DAILY_STAMP);
		pPlayer->m_VotesData.UpdateCurrentVotes();
	}

	// weekly reset quests
	if(Period == WEEK_STAMP)
	{
		ResetPeriodQuests(pPlayer, WEEK_STAMP);
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

	VoteWrapper VInfo(ClientID, VWF_STYLE_STRICT, "Quests statistic");
	VInfo.Add("Total quests: {}", TotalQuests);
	VInfo.Add("Total complected quests: {}", TotalComplectedQuests);
	VInfo.Add("Total incomplete quests: {}", TotalIncomplectedQuests);

	// tabs with quests
	ShowQuestsTabList(pPlayer, QuestState::ACCEPT);
	ShowQuestsTabList(pPlayer, QuestState::NO_ACCEPT);

	// show the completed menu
	VoteWrapper(ClientID).AddMenu(MENU_JOURNAL_FINISHED, "List of completed quests");
}

void CQuestManager::ShowQuestsTabList(CPlayer* pPlayer, QuestState State)
{
	const int ClientID = pPlayer->GetCID();
	VoteWrapper(ClientID).Add("{} quests", GetStateName(State));

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

	}

	// if the quest list is empty
	if(IsEmptyList)
	{
		VoteWrapper(ClientID).Add("List of quests is empty");
	}

	VoteWrapper::AddLine(ClientID);
}

// This function displays the information about a specific quest
void CQuestManager::ShowQuestID(CPlayer* pPlayer, int QuestID) const
{
	// Get the quest information for the given QuestID
	CQuestDescription* pQuestInfo = pPlayer->GetQuest(QuestID)->Info();

	// If the quest is a daily ques
	//if(pQuestInfo->IsDaily())
	//	return;

	// Get the size of the quest's story and the current position in the story
	VoteWrapper(pPlayer->GetCID()).AddMenu(MENU_JOURNAL_QUEST_INFORMATION, QuestID, "{}", pQuestInfo->GetName());
}

// active npc information display
void CQuestManager::ShowQuestActivesNPC(CPlayer* pPlayer, int QuestID) const
{
	CPlayerQuest* pPlayerQuest = pPlayer->GetQuest(QuestID);
	const int ClientID = pPlayer->GetCID();

	VoteWrapper(ClientID).Add("Active NPC for current quests");
	/*for(auto& pStepBot : CQuestDescription::Data()[QuestID]->m_vSteps)
	{
		const QuestBotInfo& BotInfo = pStepBot.second.m_Bot;
		if(!BotInfo.m_HasAction)
			continue;

		const vec2 Pos = BotInfo.m_Position / 32.0f;
		CQuestStep& rQuestStepDataInfo = pPlayerQuest->m_vSteps[pStepBot.first];
		const char* pSymbol = (((pPlayerQuest->GetState() == QuestState::ACCEPT && rQuestStepDataInfo.m_StepComplete) || pPlayerQuest->GetState() == QuestState::FINISHED) ? "✔ " : "\0");

		VoteWrapper VStep(ClientID, VWF_UNIQUE|VWF_STYLE_SIMPLE, "{}Step {}. {} {}(x{} y{})", 
			pSymbol, BotInfo.m_StepPos, BotInfo.GetName(), Server()->GetWorldName(BotInfo.m_WorldID), (int)Pos.x, (int)Pos.y);

		// skipped non accepted task list
		if(pPlayerQuest->GetState() != QuestState::ACCEPT)
		{
			VStep.Add("Quest been completed, or not accepted!");
			continue;
		}

		// show required defeat
		bool NoTasks = true;
		if(!BotInfo.m_vRequiredDefeats.empty())
		{
			for(auto& p : BotInfo.m_vRequiredDefeats)
			{
				if(DataBotInfo::ms_aDataBot.find(p.m_BotID) != DataBotInfo::ms_aDataBot.end())
				{
					VStep.Add("- Defeat {} [{}/{}]", DataBotInfo::ms_aDataBot[p.m_BotID].m_aNameBot, rQuestStepDataInfo.m_aMobProgress[p.m_BotID].m_Count, p.m_Value);
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

				VStep.Add("- Item {} [{}/{}]", pPlayerItem->Info()->GetName(), ClapmItem, pRequired.m_Item.GetValue());
				NoTasks = false;
			}
		}

		// show reward item's
		if(!BotInfo.m_RewardItems.empty())
		{
			for(auto& pRewardItem : BotInfo.m_RewardItems)
			{
				VStep.Add("- Receive {}x{}", pRewardItem.Info()->GetName(), pRewardItem.GetValue());
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

void CQuestManager::AppendQuestBoardGroup(CPlayer* pPlayer, CQuestsBoard* pBoard, class VoteWrapper* pWrapper, int QuestFlag) const
{
	// daily quests
	for(const auto& pQuestInfo : pBoard->m_vpQuests)
	{
		// check daily flag
		if(!pQuestInfo->HasFlag(QuestFlag))
			continue;

		// check is completed quest
		const auto* pQuest = pPlayer->GetQuest(pQuestInfo->GetID());
		if(pQuest->IsCompleted())
			continue;

		// initialize variables
		const char* StateIndicator = (pQuest->IsAccepted() ? "✔" : "×");
		//const char* ActionName = (pQuest->IsAccepted() ? "Refuse" : "Accept");
		const char* QuestName = pQuestInfo->GetName();

		// ...
		pWrapper->AddMenu(MENU_QUEST_BOARD_SELECTED, pQuestInfo->GetID(), "({}) {}", StateIndicator, QuestName);
	}
}

void CQuestManager::ShowQuestsBoardList(CPlayer* pPlayer, CQuestsBoard* pBoard) const
{
	// initialize variables
	const int ClientID = pPlayer->GetCID();

	// check board valid
	if(!pBoard)
	{
		VoteWrapper(ClientID).Add("Quest board don't work");
		return;
	}

	// information
	VoteWrapper VBoard(ClientID, VWF_STYLE_STRICT_BOLD|VWF_SEPARATE|VWF_ALIGN_TITLE, "Board: {}", pBoard->GetName());
	VBoard.AddItemValue(itAlliedSeals);
	VoteWrapper::AddEmptyline(ClientID);

	// add groups
	auto appendQuestGroup = [&](const char* pTitle, int Count, int Flag)
	{
		if(Count > 0)
		{
			VoteWrapper VGroup(ClientID, VWF_STYLE_SIMPLE, pTitle, Count);
			AppendQuestBoardGroup(pPlayer, pBoard, &VGroup, Flag);
			VoteWrapper::AddEmptyline(ClientID);
		}
	};
	appendQuestGroup("\u2696 Available daily {}", pBoard->CountAvailableDailyQuests(pPlayer), QUEST_FLAG_TYPE_DAILY);
	appendQuestGroup("\u2696 Available weekly {}", pBoard->CountAvailableWeeklyQuests(pPlayer), QUEST_FLAG_TYPE_WEEKLY);
	appendQuestGroup("\u2696 Available repeatable {}", pBoard->CountAvailableRepeatableQuests(pPlayer), QUEST_FLAG_TYPE_REPEATABLE);
	appendQuestGroup("\u2696 Available side {}", pBoard->CountAvailableSideQuests(pPlayer), QUEST_FLAG_TYPE_SIDE);

	// wanted players
	VoteWrapper VWanted(ClientID, VWF_SEPARATE | VWF_STYLE_SIMPLE, "Wanted players list");
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		CPlayer* pPlayer = GS()->GetPlayer(i, true);
		if(!pPlayer || !pPlayer->Account()->IsRelationshipsDeterioratedToMax())
			continue;

		CPlayerItem* pItemGold = pPlayer->GetItem(itGold);
		const int Reward = minimum(translate_to_percent_rest(pItemGold->GetValue(), (float)g_Config.m_SvArrestGoldAtDeath), pItemGold->GetValue());
		VWanted.Add("{} (Reward {} gold)", Server()->ClientName(i), Reward);
		{
			VWanted.BeginDepth();
			VWanted.Add("Last seen: {}", Server()->GetWorldName(pPlayer->GetPlayerWorldID()));
			VWanted.EndDepth();
		}
	}
}

void CQuestManager::ShowQuestsBoardQuest(CPlayer* pPlayer, CQuestsBoard* pBoard, int QuestID) const
{
	// check valid board
	if(!pBoard)
		return;

	// initialize variables
	const int ClientID = pPlayer->GetCID();
	CQuestDescription* pQuest = GS()->GetQuestInfo(QuestID);

	// detail information
	VoteWrapper VInfo(ClientID, VWF_SEPARATE_OPEN | VWF_STYLE_STRICT_BOLD, "\u2690 Detail information");
	VInfo.Add("Name: {}", pQuest->GetName());
	VInfo.Add("Exp: {}, Gold: {}", pQuest->Reward().GetExperience(), pQuest->Reward().GetGold());
	VoteWrapper::AddEmptyline(ClientID);

	// add button
	CPlayerQuest* pPlayerQuest = pPlayer->GetQuest(QuestID);
	const char* ActionName = (pPlayerQuest->IsAccepted() ? "Refuse" : "Accept");
	VoteWrapper(ClientID).AddOption("QUEST_STATE", pPlayerQuest->GetID(), pBoard->GetID(), ActionName);

	// add backpage
	VoteWrapper::AddEmptyline(ClientID);
	VoteWrapper::AddBackpage(ClientID);
}

CQuestsBoard* CQuestManager::GetBoardByPos(vec2 Pos) const
{
	for(auto& [key, board] : CQuestsBoard::Data())
	{
		if(distance(board->GetPos(), Pos) < 200)
			return board;
	}

	return nullptr;
}

void CQuestManager::ResetPeriodQuests(CPlayer* pPlayer, ETimePeriod Period) const
{
	// initialize variables
	int clientID = pPlayer->GetCID();
	int accountID = pPlayer->Account()->GetID();
	std::string questIDsToReset{};

	// reset daily quests
	for(auto& [QuestID, pQuest] : CPlayerQuest::Data()[clientID])
	{
		if((Period == WEEK_STAMP && pQuest->Info()->HasFlag(QUEST_FLAG_TYPE_WEEKLY)) ||
			(Period == DAILY_STAMP && pQuest->Info()->HasFlag(QUEST_FLAG_TYPE_DAILY)))
		{
			if(pQuest->IsAccepted() || pQuest->IsCompleted())
			{
				questIDsToReset += std::to_string(QuestID) + ",";
				pQuest->Reset();
			}
		}
	}

	// is not empty try remove from database
	if(!questIDsToReset.empty())
	{
		questIDsToReset.pop_back();
		Database->Execute<DB::REMOVE>("tw_accounts_quests", "WHERE QuestID IN (%s) AND UserID = '%d'", questIDsToReset.c_str(), accountID);
		GS()->Chat(clientID, "The daily quests have been updated.");
	}
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

void CQuestManager::TryAcceptNextQuestChain(CPlayer* pPlayer, int BaseQuestID) const
{
	// check verify quest valid
	CQuestDescription* pVerifyQuestInfo = GS()->GetQuestInfo(BaseQuestID);
	if(!pVerifyQuestInfo)
		return;

	// check if there's a next quest to accept
	auto nextQuestID = pVerifyQuestInfo->GetNextQuestID();
	if(nextQuestID.has_value())
	{
		// initialize variables
		CPlayerQuest* pNextQuest = pPlayer->GetQuest(nextQuestID.value());

		// accept next quest
		if(pNextQuest && pNextQuest->GetState() == QuestState::NO_ACCEPT)
			pNextQuest->Accept();
	}
}

void CQuestManager::TryAcceptNextQuestAll(CPlayer* pPlayer) const
{
	// try to accept next story quest
	for(const auto& [ID, pQuest] : CPlayerQuest::Data()[pPlayer->GetCID()])
	{
		if(pQuest->GetState() == QuestState::FINISHED)
			TryAcceptNextQuestChain(pPlayer, ID);
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