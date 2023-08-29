/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "QuestManager.h"

#include <game/server/gamecontext.h>

void CQuestManager::OnInit()
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_quests_list");
	while(pRes->next())
	{
		const int QUID = pRes->getInt("ID");
		CQuestDataInfo::ms_aDataQuests[QUID].m_QuestID = QUID;
		str_copy(CQuestDataInfo::ms_aDataQuests[QUID].m_aName, pRes->getString("Name").c_str(), sizeof(CQuestDataInfo::ms_aDataQuests[QUID].m_aName));
		str_copy(CQuestDataInfo::ms_aDataQuests[QUID].m_aStoryLine, pRes->getString("StoryLine").c_str(), sizeof(CQuestDataInfo::ms_aDataQuests[QUID].m_aStoryLine));
		CQuestDataInfo::ms_aDataQuests[QUID].m_Gold = pRes->getInt("Money");
		CQuestDataInfo::ms_aDataQuests[QUID].m_Exp = pRes->getInt("Exp");
	}
}

void CQuestManager::OnInitAccount(CPlayer* pPlayer)
{
	const int ClientID = pPlayer->GetCID();
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_accounts_quests", "WHERE UserID = '%d'", pPlayer->Acc().m_UserID);
	while(pRes->next())
	{
		QuestIdentifier ID = pRes->getInt("QuestID");
		QuestState State = (QuestState)pRes->getInt("Type");

		CQuestData(ID, ClientID).Init(State);
	}
}

void CQuestManager::OnResetClient(int ClientID)
{
	for(auto& qp : CQuestData::Data()[ClientID])
	{
		for(auto& pStepBot : qp.second.m_aPlayerSteps)
		{
			pStepBot.second.m_ClientQuitting = true;
			pStepBot.second.UpdateBot();
		}
	}

	CQuestData::Data().erase(ClientID);
}

bool CQuestManager::OnHandleMenulist(CPlayer* pPlayer, int Menulist, bool ReplaceMenu)
{
	const int ClientID = pPlayer->GetCID();
	if(ReplaceMenu)
	{
		CCharacter* pChr = pPlayer->GetCharacter();
		if(!pChr || !pChr->IsAlive())
			return false;

		return false;
	}

	if(Menulist == MENU_JOURNAL_MAIN)
	{
		pPlayer->m_LastVoteMenu = MenuList::MENU_MAIN;

		ShowQuestsMainList(pPlayer);

		GS()->AddVotesBackpage(ClientID);
		return true;
	}

	if(Menulist == MENU_JOURNAL_FINISHED)
	{
		pPlayer->m_LastVoteMenu = MENU_JOURNAL_MAIN;

		ShowQuestsTabList(pPlayer, QuestState::FINISHED);

		GS()->AddVotesBackpage(ClientID);
		return true;
	}

	if(Menulist == MENU_JOURNAL_QUEST_INFORMATION)
	{
		pPlayer->m_LastVoteMenu = MENU_JOURNAL_MAIN;

		const int QuestID = pPlayer->m_TempMenuValue;
		CQuestDataInfo pData = pPlayer->GetQuest(QuestID)->Info();

		pPlayer->GS()->Mmo()->Quest()->ShowQuestsActiveNPC(pPlayer, QuestID);
		pPlayer->GS()->AV(ClientID, "null");
		pPlayer->GS()->AVL(ClientID, "null", "{STR} : Reward", pData.GetName());
		pPlayer->GS()->AVL(ClientID, "null", "Gold: {VAL} Exp: {INT}", pData.m_Gold, pData.m_Exp);

		pPlayer->GS()->AddVotesBackpage(ClientID);
	}

	return false;
}

bool CQuestManager::OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, const int VoteID, const int VoteID2, int Get, const char* GetText)
{
	return false;
}

static const char* GetStateName(QuestState State)
{
	switch(State)
	{
		case QuestState::ACCEPT: return "Active";
		case QuestState::FINISHED: return "Finished";
		default: return "Not active";
	}
}

void CQuestManager::ShowQuestsMainList(CPlayer* pPlayer)
{
	int ClientID = pPlayer->GetCID();

	// information
	const int TotalQuests = CQuestDataInfo::ms_aDataQuests.size();
	const int TotalComplectedQuests = GetClientComplectedQuestsSize(ClientID);
	const int TotalIncomplectedQuests = TotalQuests - TotalComplectedQuests;
	GS()->AVH(ClientID, TAB_INFO_STATISTIC_QUESTS, "Quests statistic");
	GS()->AVM(ClientID, "null", NOPE, TAB_INFO_STATISTIC_QUESTS, "Total quests: {INT}", TotalQuests);
	GS()->AVM(ClientID, "null", NOPE, TAB_INFO_STATISTIC_QUESTS, "Total complected quests: {INT}", TotalComplectedQuests);
	GS()->AVM(ClientID, "null", NOPE, TAB_INFO_STATISTIC_QUESTS, "Total incomplete quests: {INT}", TotalIncomplectedQuests);
	GS()->AV(ClientID, "null");

	// tabs with quests
	ShowQuestsTabList(pPlayer, QuestState::ACCEPT);
	ShowQuestsTabList(pPlayer, QuestState::NO_ACCEPT);

	// show the completed menu
	GS()->AVM(ClientID, "MENU", MENU_JOURNAL_FINISHED, NOPE, "List of completed quests");
}

void CQuestManager::ShowQuestsTabList(CPlayer* pPlayer, QuestState State)
{
	const int ClientID = pPlayer->GetCID();
	GS()->AVL(ClientID, "null", "{STR} quests", GetStateName(State));

	// check first quest story step
	bool IsEmptyList = true;
	std::list < std::string /*stories was checked*/ > StoriesChecked;
	for(const auto& [ID, Quest] : CQuestDataInfo::ms_aDataQuests)
	{
		if(pPlayer->GetQuest(ID)->GetState() != State)
			continue;

		if(State == QuestState::FINISHED)
		{
			ShowQuestID(pPlayer, ID);
			IsEmptyList = false;
			continue;
		}

		const auto& IsAlreadyChecked = std::find_if(StoriesChecked.begin(), StoriesChecked.end(), [=](const std::string& stories)
		{
			return (str_comp_nocase(Quest.GetStory(), stories.c_str()) == 0);
		});
		if(IsAlreadyChecked == StoriesChecked.end())
		{
			StoriesChecked.emplace_back(CQuestDataInfo::ms_aDataQuests[ID].m_aStoryLine);
			ShowQuestID(pPlayer, ID);
			IsEmptyList = false;
		}
	}

	// if the quest list is empty
	if(IsEmptyList)
	{
		GS()->AV(ClientID, "null", "List of quests is empty");
	}
	GS()->AV(ClientID, "null");
}

void CQuestManager::ShowQuestID(CPlayer *pPlayer, int QuestID)
{
	CQuestDataInfo& pData = pPlayer->GetQuest(QuestID)->Info();
	const int ClientID = pPlayer->GetCID();
	const int QuestsSize = pData.GetQuestStorySize();
	const int QuestPosition = pData.GetQuestStoryPosition();

	GS()->AVD(ClientID, "MENU", MENU_JOURNAL_QUEST_INFORMATION, QuestID, NOPE, "{INT}/{INT} {STR}: {STR}", QuestPosition, QuestsSize, pData.GetStory(), pData.GetName());
}

// active npc information display
void CQuestManager::ShowQuestsActiveNPC(CPlayer* pPlayer, int QuestID)
{
	CQuestData* pPlayerQuest = pPlayer->GetQuest(QuestID);
	const int ClientID = pPlayer->GetCID();
	GS()->AVM(ClientID, "null", NOPE, NOPE, "Active NPC for current quests");

	for(auto& pStepBot : CQuestDataInfo::ms_aDataQuests[QuestID].m_StepsQuestBot)
	{
		const QuestBotInfo& BotInfo = pStepBot.second.m_Bot;
		if(!BotInfo.m_HasAction)
			continue;

		const int HideID = (NUM_TAB_MENU + BotInfo.m_SubBotID);
		const vec2 Pos = BotInfo.m_Position / 32.0f;
		CPlayerQuestStepDataInfo& rQuestStepDataInfo = pPlayerQuest->m_aPlayerSteps[pStepBot.first];
		const char* pSymbol = (((pPlayerQuest->GetState() == QuestState::ACCEPT && rQuestStepDataInfo.m_StepComplete) || pPlayerQuest->GetState() == QuestState::FINISHED) ? "✔ " : "\0");

		GS()->AVH(ClientID, HideID, "{STR}Step {INT}. {STR} {STR}(x{INT} y{INT})", pSymbol, BotInfo.m_Step, BotInfo.GetName(), Server()->GetWorldName(BotInfo.m_WorldID), (int)Pos.x, (int)Pos.y);

		// skipped non accepted task list
		if(pPlayerQuest->GetState() != QuestState::ACCEPT)
		{
			GS()->AVM(ClientID, "null", NOPE, HideID, "Quest been completed, or not accepted!");
			continue;
		}

		// show required defeat
		bool NoTasks = true;
		if(!BotInfo.m_RequiredDefeat.empty())
		{
			for(auto& p : BotInfo.m_RequiredDefeat)
			{
				if(DataBotInfo::ms_aDataBot.find(p.m_BotID) != DataBotInfo::ms_aDataBot.end())
				{
					GS()->AVM(ClientID, "null", NOPE, HideID, "- Defeat {STR} [{INT}/{INT}]",
						DataBotInfo::ms_aDataBot[p.m_BotID].m_aNameBot, rQuestStepDataInfo.m_aMobProgress[p.m_BotID], p.m_Count);
					NoTasks = false;
				}
			}
		}

		// show required item's
		if(!BotInfo.m_RequiredItems.empty())
		{
			for(auto& p : BotInfo.m_RequiredItems)
			{
				CPlayerItem* pPlayerItem = pPlayer->GetItem(p.m_ItemID);
				int ClapmItem = clamp(pPlayerItem->GetValue(), 0, p.m_Count);
				GS()->AVM(ClientID, "null", NOPE, HideID, "- Item {STR} [{VAL}/{VAL}]", pPlayerItem->Info()->GetName(), ClapmItem, p.m_Count);
				NoTasks = false;
			}
		}

		// show reward item's
		if(!BotInfo.m_RewardItems.empty())
		{
			for(auto& p : BotInfo.m_RewardItems)
			{
				CItemDescription* pRewardItemInfo = GS()->GetItemInfo(p.m_ItemID);
				GS()->AVM(ClientID, "null", NOPE, HideID, "- Receive {STR}x{VAL}", pRewardItemInfo->GetName(), p.m_Count);
			}
		}

		if(NoTasks)
		{
			GS()->AVM(ClientID, "null", NOPE, HideID, "You just need to talk.");
		}
	}
}

void CQuestManager::QuestShowRequired(CPlayer* pPlayer, QuestBotInfo& pBot, char* aBufQuestTask, int Size)
{
	const int QuestID = pBot.m_QuestID;
	CQuestData* pPlayerQuest = pPlayer->GetQuest(QuestID);
	if(pPlayerQuest->m_aPlayerSteps.find(pBot.m_SubBotID) != pPlayerQuest->m_aPlayerSteps.end())
		pPlayerQuest->m_aPlayerSteps[pBot.m_SubBotID].ShowRequired(pPlayer, aBufQuestTask, Size);
}

void CQuestManager::AddMobProgressQuests(CPlayer* pPlayer, int BotID)
{
	// TODO Optimize algoritm check complected steps
	const int ClientID = pPlayer->GetCID();
	for(auto& pPlayerQuest : CQuestData::Data()[ClientID])
	{
		if(pPlayerQuest.second.GetState() != QuestState::ACCEPT)
			continue;

		for(auto& pStepBot : pPlayerQuest.second.m_aPlayerSteps)
		{
			if(pPlayerQuest.second.GetCurrentStep() == pStepBot.second.m_Bot.m_Step)
				pStepBot.second.AddMobProgress(pPlayer, BotID);
		}
	}
}

void CQuestManager::UpdateArrowStep(CPlayer *pPlayer)
{
	// TODO Optimize algoritm check complected steps
	const int ClientID = pPlayer->GetCID();
	for (auto& pPlayerQuest : CQuestData::Data()[ClientID])
	{
		if(pPlayerQuest.second.GetState() != QuestState::ACCEPT)
			continue;

		for(auto& pStepBot : pPlayerQuest.second.m_aPlayerSteps)
			pStepBot.second.CreateStepArrow(ClientID);
	}
}

void CQuestManager::AcceptNextStoryQuest(CPlayer *pPlayer, int CheckQuestID)
{
	const CQuestDataInfo CheckingQuest = CQuestDataInfo::ms_aDataQuests[CheckQuestID];
	for (auto pQuestData = CQuestDataInfo::ms_aDataQuests.find(CheckQuestID); pQuestData != CQuestDataInfo::ms_aDataQuests.end(); pQuestData++)
	{
		// search next quest story step
		if(str_comp_nocase(CheckingQuest.m_aStoryLine, pQuestData->second.m_aStoryLine) == 0)
		{
			// skip all if a quest story is found that is still active
			if(pPlayer->GetQuest(pQuestData->first)->GetState() == QuestState::ACCEPT)
				break;

			// accept next quest step
			if(pPlayer->GetQuest(pQuestData->first)->Accept())
				break;
		}
	}
}

void CQuestManager::AcceptNextStoryQuestStep(CPlayer* pPlayer)
{
	// check first quest story step search active quests
	std::list < std::string /*stories was checked*/ > StoriesChecked;
	for(const auto& pPlayerQuest : CQuestData::Data()[pPlayer->GetCID()])
	{
		// allow accept next story quest only for complected some quest on story
		if(pPlayerQuest.second.GetState() != QuestState::FINISHED)
			continue;

		// accept next story quest
		const auto& IsAlreadyChecked = std::find_if(StoriesChecked.begin(), StoriesChecked.end(), [=](const std::string& stories)
		{ return (str_comp_nocase(CQuestDataInfo::ms_aDataQuests[pPlayerQuest.first].m_aStoryLine, stories.c_str()) == 0); });
		if(IsAlreadyChecked == StoriesChecked.end())
		{
			StoriesChecked.emplace_front(CQuestDataInfo::ms_aDataQuests[pPlayerQuest.first].m_aStoryLine);
			AcceptNextStoryQuest(pPlayer, pPlayerQuest.first);
		}
	}
}

int CQuestManager::GetUnfrozenItemValue(CPlayer *pPlayer, int ItemID) const
{
	const int ClientID = pPlayer->GetCID();
	int AvailableValue = pPlayer->GetItem(ItemID)->GetValue();
	for (const auto& pPlayerQuest : CQuestData::Data()[ClientID])
	{
		if(pPlayerQuest.second.GetState() != QuestState::ACCEPT)
			continue;

		for(auto& pStepBot : pPlayerQuest.second.m_aPlayerSteps)
		{
			if(!pStepBot.second.m_StepComplete)
				AvailableValue -= pStepBot.second.GetValueBlockedItem(pPlayer, ItemID);
		}
	}
	return max(AvailableValue, 0);
}

int CQuestManager::GetClientComplectedQuestsSize(int ClientID) const
{
	int Total = 0;

	for(auto& [QuestID, Data] : CQuestData::Data()[ClientID])
	{
		if(Data.IsComplected())
			Total++;
	}

	return Total;
}