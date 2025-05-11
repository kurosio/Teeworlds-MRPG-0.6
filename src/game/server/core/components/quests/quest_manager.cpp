/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "quest_manager.h"

#include <game/server/gamecontext.h>

void CQuestManager::OnPreInit()
{
	// Load quests
	std::unordered_map<int, DBSet> vTempQuestFlags {};
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_quests_list");
	while(pRes->next())
	{
		// initialize variables
		const auto ID = pRes->getInt("ID");
		const auto nextQuestID = pRes->getInt("NextQuestID");
		const auto Name = pRes->getString("Name");
		const auto Gold = pRes->getInt("Money");
		const auto Exp = pRes->getInt("Exp");
		DBSet flagSet(pRes->getString("Flags"));

		// create new element
		auto optionalNextQuestID = nextQuestID > 0 ? std::optional(nextQuestID) : std::nullopt;
		CQuestDescription::CreateElement(ID)->Init(Name, Gold, Exp, optionalNextQuestID);
		vTempQuestFlags[ID] = flagSet;
	}

	// Initialize flags, previous quests ids for chain by nextQuestID
	for(const auto& [ID, pQuest] : CQuestDescription::Data())
	{
		if(auto* pNextQuest = pQuest->GetNextQuest())
			pNextQuest->InitPrevousQuestID(ID);

		pQuest->InitFlags(vTempQuestFlags[ID]);
	}

	// Load boards
	ResultPtr pResBoard = Database->Execute<DB::SELECT>("*", TW_QUEST_BOARDS_TABLE);
	while(pResBoard->next())
	{
		// initialize variables
		const auto ID = pResBoard->getInt("ID");
		const auto Name = pResBoard->getString("Name");
		const auto Pos = vec2((float)pResBoard->getInt("PosX"), (float)pResBoard->getInt("PosY"));
		const auto WorldID = pResBoard->getInt("WorldID");

		// create new element
		CQuestsBoard::CreateElement(ID)->Init(Name, Pos, WorldID);
	}

	// Load board quests
	ResultPtr pResDailyQuest = Database->Execute<DB::SELECT>("*", TW_QUESTS_DAILY_BOARD_LIST);
	while(pResDailyQuest->next())
	{
		// initialize variables
		const auto QuestID = pResDailyQuest->getInt("QuestID");
		const auto BoardID = pResDailyQuest->getInt("DailyBoardID");

		// add quest to board
		auto* pQuest = GS()->GetQuestInfo(QuestID);
		auto* pBoard = CQuestsBoard::Data()[BoardID];
		pQuest->AddFlag(QUEST_FLAG_GRANTED_FROM_BOARD);
		pBoard->GetQuestList().push_back(pQuest);
	}

	// initialize by chain flags for all next quests
	for(const auto& [ID, pQuest] : CQuestDescription::Data())
	{
		auto* pPrevQuest = pQuest->GetPreviousQuest();
		if(pPrevQuest != nullptr)
			continue;

		const auto Flags = pQuest->GetFlags();
		auto* pNextQuest = pQuest->GetNextQuest();
		while(pNextQuest != nullptr)
		{
			pNextQuest->SetFlags(Flags);
			pNextQuest = pNextQuest->GetNextQuest();
		}
	}
}

void CQuestManager::OnPlayerLogin(CPlayer* pPlayer)
{
	// initialize player quests
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_accounts_quests", "WHERE UserID = '{}'", pPlayer->Account()->GetID());
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
	mystd::freeContainer(CPlayerQuest::Data()[ClientID]);
}

void CQuestManager::OnCharacterTile(CCharacter* pChr)
{
	CPlayer* pPlayer = pChr->GetPlayer();

	HANDLE_TILE_VOTE_MENU(pPlayer, pChr, TILE_QUEST_BOARD, MENU_BOARD, {}, {});
}

bool CQuestManager::OnSendMenuVotes(CPlayer* pPlayer, int Menulist)
{
	// initialize variables
	CCharacter* pChr = pPlayer->GetCharacter();
	const int ClientID = pPlayer->GetCID();

	// quest board
	if(Menulist == MENU_BOARD)
	{
		// show quest board
		CQuestsBoard* pBoard = GetBoardByPos(pChr->m_Core.m_Pos);
		ShowQuestsBoardList(pChr->GetPlayer(), pBoard);
		return true;
	}

	// quest board selected
	if(Menulist == MENU_BOARD_QUEST_SELECT)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_BOARD);

		if(const auto QuestID = pPlayer->m_VotesData.GetExtraID())
		{
			auto* pQuestInfo = GS()->GetQuestInfo(QuestID.value());
			ShowQuestInfo(pPlayer, pQuestInfo, true);
			VoteWrapper::AddEmptyline(ClientID);
		}

		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// quest journal
	if(Menulist == MENU_JOURNAL_MAIN)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_MAIN);

		// show quest journal
		ShowQuestList(pPlayer);
		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	// quest journal information
	if(Menulist == MENU_JOURNAL_QUEST_DETAILS)
	{
		pPlayer->m_VotesData.SetLastMenuID(MENU_JOURNAL_MAIN);

		if(const auto QuestID = pPlayer->m_VotesData.GetExtraID())
		{
			auto* pQuestInfo = GS()->GetQuestInfo(QuestID.value());
			ShowQuestInfo(pPlayer, pQuestInfo, false);
			VoteWrapper::AddEmptyline(ClientID);
		}

		VoteWrapper::AddBackpage(ClientID);
		return true;
	}

	return false;
}

bool CQuestManager::OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, const int Extra1, const int Extra2, int ReasonNumber, const char* pReason)
{
	//const int ClientID = pPlayer->GetCID();


	// change quest state refuse or accept
	if(PPSTR(pCmd, "QUEST_STATE") == 0)
	{
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
	// reset period quests
	ResetPeriodQuests(pPlayer, Period);
	pPlayer->m_VotesData.UpdateCurrentVotes();
}

void CQuestManager::ShowQuestList(CPlayer* pPlayer) const
{
	// Initialize variables
	int ClientID = pPlayer->GetCID();
	const int TotalQuests = static_cast<int>(CQuestDescription::Data().size());
	const int TotalCompletedQuests = GetCountCompletedQuests(ClientID);
	const int TotalIncompleteQuests = TotalQuests - TotalCompletedQuests;
	const float CompletionPercentage = translate_to_percent(TotalQuests, TotalCompletedQuests);

	// Information
	VoteWrapper VInfo(ClientID, VWF_STYLE_SIMPLE | VWF_ALIGN_TITLE | VWF_SEPARATE, "Quest Statistics");
	VInfo.Add("Total quests: {}", TotalQuests);
	VInfo.Add("Completed quests: {} ({~.2}%)", TotalCompletedQuests, CompletionPercentage);
	VInfo.Add("Incomplete quests: {} ({~.2}%)", TotalIncompleteQuests, 100 - CompletionPercentage);
	VoteWrapper::AddEmptyline(ClientID);

	// Tabs with quests
	ShowQuestsTabList("\u2833 Accepted", pPlayer, QuestState::Accepted);
	ShowQuestsTabList("\u2634 Completed", pPlayer, QuestState::Finished);
}


void CQuestManager::ShowQuestsTabList(const char* pTabname, CPlayer* pPlayer, QuestState State) const
{
	const int ClientID = pPlayer->GetCID();

	VoteWrapper VTab(ClientID, VWF_STYLE_SIMPLE | VWF_ALIGN_TITLE, "{} quests", pTabname);
	for(const auto& [ID, pQuestInfo] : CQuestDescription::Data())
	{
		auto pPlayerQuest = pPlayer->GetQuest(ID);
		if(pPlayerQuest->GetState() == State)
			VTab.AddMenu(MENU_JOURNAL_QUEST_DETAILS, ID, "{}", pQuestInfo->GetName());
	}

	VoteWrapper::AddEmptyline(ClientID);
}

void CQuestManager::PrepareRequiredBuffer(CPlayer* pPlayer, QuestBotInfo& pBot, char* aBufQuestTask, int Size)
{
	const int QuestID = pBot.m_QuestID;
	CPlayerQuest* pQuest = pPlayer->GetQuest(QuestID);
	CQuestStep* pStep = pQuest->GetStepByMob(pBot.m_ID);
	pStep->FormatStringTasks(aBufQuestTask, Size);
}

void CQuestManager::TryAppendDefeatProgress(CPlayer* pPlayer, int DefeatedBotID)
{
	// TODO Optimize algoritm check complected steps
	const int ClientID = pPlayer->GetCID();
	for(auto& [ID, pQuest] : CPlayerQuest::Data()[ClientID])
	{
		// only for accepted quests
		if(pQuest->GetState() != QuestState::Accepted)
			continue;

		// check current steps and append
		for(auto& pStepBot : pQuest->m_vObjectives)
			pStepBot->AppendDefeatProgress(DefeatedBotID);
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

	// lambda function to show quest board by group list
	auto ShowQuestBoardByGroupList = [this](CPlayer* pPlayer, CQuestsBoard* pBoard, const char* pTitle, int QuestFlag)
	{
		const auto& vQuestsData = pBoard->GetQuestList();
		if(vQuestsData.empty())
			return;

		VoteWrapper VGroup(pPlayer->GetCID(), VWF_STYLE_SIMPLE, pTitle);
		for(const auto& pQuestInfo : vQuestsData)
		{
			if(!pQuestInfo || !pQuestInfo->HasFlag(QuestFlag))
				continue;

			auto* pCurrentQuest = pQuestInfo;
			while(pCurrentQuest && pPlayer->GetQuest(pCurrentQuest->GetID())->IsCompleted())
				pCurrentQuest = pCurrentQuest->GetNextQuest();

			if(pCurrentQuest)
			{
				const auto* pPlayerQuest = pPlayer->GetQuest(pCurrentQuest->GetID());
				const char* pStateIndicator = (pPlayerQuest->IsAccepted() ? "✔" : "✖");
				const char* pQuestName = pCurrentQuest->GetName();
				const auto chainLength = pCurrentQuest->GetChainLength();

				if(chainLength > 1)
				{
					const auto currentChainPos = pCurrentQuest->GetCurrentChainPos();
					VGroup.AddMenu(MENU_BOARD_QUEST_SELECT, pPlayerQuest->GetID(), "({}) {} ({} of {})", pStateIndicator, pQuestName, currentChainPos, chainLength);
				}
				else
				{
					VGroup.AddMenu(MENU_BOARD_QUEST_SELECT, pPlayerQuest->GetID(), "({}) {}", pStateIndicator, pQuestName);
				}
			}
		}
		VoteWrapper::AddEmptyline(pPlayer->GetCID());
	};

	// information
	VoteWrapper VBoard(ClientID, VWF_STYLE_STRICT_BOLD|VWF_SEPARATE|VWF_ALIGN_TITLE, "Board: {}", pBoard->GetName());
	VBoard.AddItemValue(itActivityCoin);
	VoteWrapper::AddEmptyline(ClientID);

	// add groups
	ShowQuestBoardByGroupList(pPlayer, pBoard, "\u2696 Available daily", QUEST_FLAG_TYPE_DAILY);
	ShowQuestBoardByGroupList(pPlayer, pBoard, "\u2696 Available weekly", QUEST_FLAG_TYPE_WEEKLY);
	ShowQuestBoardByGroupList(pPlayer, pBoard, "\u2696 Available repeatable", QUEST_FLAG_TYPE_REPEATABLE);
	ShowQuestBoardByGroupList(pPlayer, pBoard, "\u2696 Available side", QUEST_FLAG_TYPE_SIDE);

	// wanted players
	VoteWrapper VWanted(ClientID, VWF_SEPARATE | VWF_STYLE_SIMPLE, "Wanted players list");
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		CPlayer* pPl = GS()->GetPlayer(i, true);
		if(!pPl || !pPl->Account()->IsCrimeMaxedOut())
			continue;

		const int Reward = minimum(translate_to_percent_rest(pPlayer->Account()->GetGold(), (float)g_Config.m_SvArrestGoldOnDeath), pPlayer->Account()->GetGold());
		VWanted.Add("{} (Reward: {} gold)", Server()->ClientName(i), Reward);
		{
			VWanted.BeginDepth();
			VWanted.Add("Last seen in: {}", Server()->GetWorldName(pPl->GetCurrentWorldID()));
			VWanted.EndDepth();
		}
	}
}

void CQuestManager::ShowQuestInfo(CPlayer* pPlayer, CQuestDescription* pQuest, bool fromBoard) const
{
	// initialize variables
	const int ClientID = pPlayer->GetCID();
	auto* pPlayerQuest = pPlayer->GetQuest(pQuest->GetID());

	// detailed information
	VoteWrapper VInfo(ClientID, VWF_STYLE_STRICT_BOLD, "\u2690 Quest Details");
	VInfo.Add("Name: {}", pQuest->GetName());
	VInfo.Add("Experience: {}", pQuest->Reward().GetExperience());
	VInfo.Add("Gold: {$}", pQuest->Reward().GetGold());
	VInfo.Add("Total Steps: {}", pQuest->m_vObjectives.size());
	VInfo.Add("Status: {}", (pPlayerQuest->IsAccepted() ? "Accepted" : "Not Accepted"));
	VoteWrapper::AddEmptyline(ClientID);

	// add buttons
	VoteWrapper VButtons(ClientID, VWF_STYLE_DOUBLE | VWF_SEPARATE | VWF_ALIGN_TITLE, "Action's");
	const auto* pNextQuest = pQuest->GetNextQuest();
	const auto* pPrevQuest = pQuest->GetPreviousQuest();
	const int Menulist = fromBoard ? MENU_BOARD_QUEST_SELECT : MENU_JOURNAL_QUEST_DETAILS;

	// next quest button
	if(pNextQuest)
		VButtons.AddMenu(Menulist, pNextQuest->GetID(), "Next: \u27A1 {}", pNextQuest->GetName());

	// current quest
	if(pPlayerQuest->IsCompleted())
		VButtons.Add("\u2714 Quest completed!");
	else if(pPrevQuest && !pPlayer->GetQuest(pPrevQuest->GetID())->IsCompleted())
		VButtons.Add("\u26A0 Prev quest incomplete!");
	else if(!pQuest->CanBeAcceptOrRefuse())
		VButtons.Add("\u26A0 Auto-activated!");
	else
		VButtons.AddOption("QUEST_STATE", pPlayerQuest->GetID(), (pPlayerQuest->IsAccepted() ? "Refuse" : "Accept"));

	// prev quest button
	if(pPrevQuest)
		VButtons.AddMenu(Menulist, pPrevQuest->GetID(), "Previous: \u2B05 {}", pPrevQuest->GetName());
	VoteWrapper::AddEmptyline(ClientID);

	// step information
	for(const auto& [stepNumber, subSteps] : pQuest->m_vObjectives)
	{
		VoteWrapper VStep(ClientID, VWF_ALIGN_TITLE | VWF_STYLE_SIMPLE | VWF_SEPARATE, "\u270E Step {}", stepNumber);

		// check if the step is locked
		const bool isStepLocked = !pPlayerQuest->IsCompleted() &&
			((pPlayerQuest->IsAccepted() && stepNumber > pPlayerQuest->GetStepPos()) ||
				(!pPlayerQuest->IsAccepted() && stepNumber != 1));

		if(isStepLocked)
		{
			VStep.Add("Locked for now.");
			VoteWrapper::AddEmptyline(ClientID);
			continue;
		}

		// show the step required
		for(const auto& subStep : subSteps)
		{
			const auto& questBotInfo = subStep.m_Bot;
			bool hasTask = false;

			VStep.MarkList().Add("\u2659 From {}:", questBotInfo.GetName());
			VStep.BeginDepth();

			// required defeats
			for(const auto& requiredDefeat : questBotInfo.m_vRequiredDefeats)
			{
				const auto& botInfo = DataBotInfo::ms_aDataBot[requiredDefeat.m_BotID];
				const auto pPlayerStep = pPlayerQuest->GetStepByMob(questBotInfo.m_ID);

				if(!pPlayerQuest->IsAccepted() || !pPlayerStep)
				{
					VStep.MarkList().Add("Defeat {} x{}", botInfo.m_aNameBot, requiredDefeat.m_RequiredCount);
				}
				else
				{
					int currentCount = pPlayerStep->m_aMobProgress[requiredDefeat.m_BotID].m_Count;
					int requiredCount = requiredDefeat.m_RequiredCount;
					VStep.MarkList().Add("Defeat {} [{}/{}]", botInfo.m_aNameBot, currentCount, requiredCount);
				}
				hasTask = true;
			}

			// required items
			for(const auto& requiredItem : questBotInfo.m_vRequiredItems)
			{
				if(!pPlayerQuest->IsAccepted())
				{
					VStep.MarkList().Add("Collect {} x{}", requiredItem.m_Item.Info()->GetName(), requiredItem.m_Item.GetValue());
				}
				else
				{
					auto pPlayerItem = pPlayer->GetItem(requiredItem.m_Item);
					int clampedValue = clamp(pPlayerItem->GetValue(), 0, requiredItem.m_Item.GetValue());
					VStep.MarkList().Add("Collect {} [{}/{}]", requiredItem.m_Item.Info()->GetName(), clampedValue, requiredItem.m_Item.GetValue());
				}
				hasTask = true;
			}

			// required actions
			for(size_t i = 0; i < questBotInfo.m_vRequiredMoveAction.size(); ++i)
			{
				const auto& requiredAction = questBotInfo.m_vRequiredMoveAction[i];
				const auto pPlayerStep = pPlayerQuest->GetStepByMob(questBotInfo.m_ID);

				if(!pPlayerQuest->IsAccepted() || !pPlayerStep)
				{
					VStep.MarkList().Add("{}", requiredAction.m_TaskName);
				}
				else
				{
					bool isCompleted = pPlayerStep->m_aMoveActionProgress[i];
					VStep.MarkList().Add("{} [{}]", requiredAction.m_TaskName, isCompleted ? "\u2714" : "\u2718");
				}
				hasTask = true;
			}

			if(!hasTask)
			{
				VStep.Add("Only required talking.");
			}

			VStep.EndDepth();
		}

		VoteWrapper::AddEmptyline(ClientID);
	}
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
	const auto ClientID = pPlayer->GetCID();
	const auto AccountID = pPlayer->Account()->GetID();
	std::string questIDsToReset{};

	// reset period quests
	for(auto& [QuestID, pQuest] : CPlayerQuest::Data()[ClientID])
	{
		if((Period == WEEK_STAMP && pQuest->Info()->HasFlag(QUEST_FLAG_TYPE_WEEKLY)) ||
			(Period == DAILY_STAMP && pQuest->Info()->HasFlag(QUEST_FLAG_TYPE_DAILY)))
		{
			// only in one direction forward only (skip quests what has previous quest)
			if(pQuest->Info()->GetPreviousQuest() != nullptr)
				continue;

			// reset full next quest line
			auto* pCurrentQuestInfo = pQuest->Info();
			while(pCurrentQuestInfo != nullptr)
			{
				auto* pCurrentQuest = pPlayer->GetQuest(pCurrentQuestInfo->GetID());
				if(pCurrentQuest->IsAccepted() || pCurrentQuest->IsCompleted())
				{
					questIDsToReset += std::to_string(pCurrentQuest->GetID()) + ",";
					pCurrentQuest->Reset();
				}

				pCurrentQuestInfo = pCurrentQuestInfo->GetNextQuest();
			}
		}
	}

	// is not empty try remove from database
	if(!questIDsToReset.empty())
	{
		questIDsToReset.pop_back();
		Database->Execute<DB::REMOVE>("tw_accounts_quests", "WHERE QuestID IN ({}) AND UserID = '{}'", questIDsToReset.c_str(), AccountID);
		GS()->Chat(ClientID, "The daily quests have been updated.");
	}
}

void CQuestManager::Update(CPlayer* pPlayer)
{
	// initialize variables
	const auto ClientID = pPlayer->GetCID();
	const auto& vPlayerQuests = CPlayerQuest::Data()[ClientID];

	// try update quests
	for(auto& [ID, pQuest] : vPlayerQuests)
	{
		if(pQuest->GetState() == QuestState::Accepted)
			pQuest->Update();
	}
}

void CQuestManager::TryAcceptNextQuestChain(CPlayer* pPlayer, int BaseQuestID) const
{
	// check valid quest data
	CQuestDescription* pVerifyQuestInfo = GS()->GetQuestInfo(BaseQuestID);
	if(!pVerifyQuestInfo)
		return;

	// try to accept next quest
	if(const auto* pNextQuest = pVerifyQuestInfo->GetNextQuest())
	{
		auto* pPlayerNextQuest = pPlayer->GetQuest(pNextQuest->GetID());
		if(pPlayerNextQuest && pPlayerNextQuest->GetState() == QuestState::NoAccepted)
			pPlayerNextQuest->Accept();
	}
}

void CQuestManager::TryAcceptNextQuestChainAll(CPlayer* pPlayer) const
{
	// initialize variables
	int ClientID = pPlayer->GetCID();
	const auto& vPlayerQuests = CPlayerQuest::Data()[ClientID];

	// try to accept next quest
	std::ranges::for_each(vPlayerQuests, [this, pPlayer](const auto& pair)
	{
		if(pair.second->GetState() == QuestState::Finished)
			TryAcceptNextQuestChain(pPlayer, pair.first);
	});
}

int CQuestManager::GetUnfrozenItemValue(CPlayer* pPlayer, int ItemID) const
{
	const int ClientID = pPlayer->GetCID();
	int AvailableValue = pPlayer->GetItem(ItemID)->GetValue();
	for(const auto& [ID, pQuest] : CPlayerQuest::Data()[ClientID])
	{
		if(pQuest->GetState() != QuestState::Accepted)
			continue;

		/*for(auto& pStepBot : pQuest->m_vObjectives)
		{
			if(!pStepBot.second.m_StepComplete)
				AvailableValue -= pStepBot.second.GetNumberBlockedItem(ItemID);
		}*/
	}
	return maximum(AvailableValue, 0);
}

int CQuestManager::GetCountCompletedQuests(int ClientID) const
{
	auto& playerQuests = CPlayerQuest::Data()[ClientID];
	return (int)std::ranges::count_if(playerQuests, [](const auto& pair) { return pair.second->IsCompleted(); });;
}