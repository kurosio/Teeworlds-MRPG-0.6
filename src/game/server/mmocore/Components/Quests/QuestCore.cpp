/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "QuestCore.h"

#include <game/server/gamecontext.h>

void QuestCore::OnInit()
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

void QuestCore::OnInitAccount(CPlayer* pPlayer)
{
	const int ClientID = pPlayer->GetCID();
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_accounts_quests", "WHERE UserID = '%d'", pPlayer->Acc().m_UserID);
	while(pRes->next())
	{
		const int QuestID = pRes->getInt("QuestID");
		CQuestData::ms_aPlayerQuests[ClientID][QuestID].m_pPlayer = pPlayer;
		CQuestData::ms_aPlayerQuests[ClientID][QuestID].m_QuestID = QuestID;
		CQuestData::ms_aPlayerQuests[ClientID][QuestID].m_State = (QuestState)pRes->getInt("Type");
		CQuestData::ms_aPlayerQuests[ClientID][QuestID].LoadSteps();
	}
}

void QuestCore::OnResetClient(int ClientID)
{
	for(auto& qp : CQuestData::ms_aPlayerQuests[ClientID])
	{
		for(auto& pStepBot : qp.second.m_StepsQuestBot)
		{
			pStepBot.second.m_ClientQuitting = true;
			pStepBot.second.UpdateBot();
		}
	}

	CQuestData::ms_aPlayerQuests.erase(ClientID);
}

bool QuestCore::OnHandleMenulist(CPlayer* pPlayer, int Menulist, bool ReplaceMenu)
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
		CQuestDataInfo pData = pPlayer->GetQuest(QuestID).Info();

		pPlayer->GS()->Mmo()->Quest()->ShowQuestsActiveNPC(pPlayer, QuestID);
		pPlayer->GS()->AV(ClientID, "null");
		pPlayer->GS()->AVL(ClientID, "null", "{STR} : Reward", pData.GetName());
		pPlayer->GS()->AVL(ClientID, "null", "Gold: {VAL} Exp: {INT}", pData.m_Gold, pData.m_Exp);

		pPlayer->GS()->AddVotesBackpage(ClientID);
	}

	return false;
}

bool QuestCore::OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, const int VoteID, const int VoteID2, int Get, const char* GetText)
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

void QuestCore::ShowQuestsMainList(CPlayer* pPlayer)
{
	ShowQuestsTabList(pPlayer, QuestState::ACCEPT);
	ShowQuestsTabList(pPlayer, QuestState::NO_ACCEPT);

	// show the completed menu
	GS()->AVM(pPlayer->GetCID(), "MENU", MENU_JOURNAL_FINISHED, NOPE, "List of completed quests");
}

void QuestCore::ShowQuestsTabList(CPlayer* pPlayer, QuestState State)
{
	const int ClientID = pPlayer->GetCID();
	GS()->AVL(ClientID, "null", "{STR} quests", GetStateName(State));

	// check first quest story step
	bool IsEmptyList = true;
	std::list < std::string /*stories was checked*/ > StoriesChecked;
	for(const auto& [ID, Quest] : CQuestDataInfo::ms_aDataQuests)
	{
		if(pPlayer->GetQuest(ID).GetState() != State)
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

void QuestCore::ShowQuestID(CPlayer *pPlayer, int QuestID)
{
	CQuestDataInfo pData = pPlayer->GetQuest(QuestID).Info();
	const int ClientID = pPlayer->GetCID();
	const int QuestsSize = pData.GetQuestStorySize();
	const int QuestPosition = pData.GetQuestStoryPosition();

	GS()->AVD(ClientID, "MENU", MENU_JOURNAL_QUEST_INFORMATION, QuestID, NOPE, "{INT}/{INT} {STR}: {STR}", QuestPosition, QuestsSize, pData.GetStory(), pData.GetName());
}

// active npc information display
void QuestCore::ShowQuestsActiveNPC(CPlayer* pPlayer, int QuestID)
{
	CQuestData& pPlayerQuest = pPlayer->GetQuest(QuestID);
	const int ClientID = pPlayer->GetCID();
	GS()->AVM(ClientID, "null", NOPE, NOPE, "Active NPC for current quests");

	for(auto& pStepBot : CQuestDataInfo::ms_aDataQuests[QuestID].m_StepsQuestBot)
	{
		const QuestBotInfo& BotInfo = pStepBot.second.m_Bot;
		if(!BotInfo.m_HasAction)
			continue;

		const int HideID = (NUM_TAB_MENU + BotInfo.m_SubBotID);
		const vec2 Pos = BotInfo.m_Position / 32.0f;
		const CPlayerQuestStepDataInfo& rQuestStepDataInfo = pPlayerQuest.m_StepsQuestBot[pStepBot.first];
		const char* pSymbol = (((pPlayerQuest.GetState() == QuestState::ACCEPT && rQuestStepDataInfo.m_StepComplete) || pPlayerQuest.GetState() == QuestState::FINISHED) ? "✔ " : "\0");
		
		GS()->AVH(ClientID, HideID, "{STR}Step {INT}. {STR} {STR}(x{INT} y{INT})", pSymbol, BotInfo.m_Step, BotInfo.GetName(), Server()->GetWorldName(BotInfo.m_WorldID), (int)Pos.x, (int)Pos.y);

		// skipped non accepted task list
		if(pPlayerQuest.GetState() != QuestState::ACCEPT)
		{
			GS()->AVM(ClientID, "null", NOPE, HideID, "Quest been completed, or not accepted!");
			continue;
		}

		// need for bot
		bool NeedOnlyTalk = true;
		for(int i = 0; i < 2; i++)
		{
			const int NeedKillMobID = BotInfo.m_aNeedMob[i];
			const int KillNeed = BotInfo.m_aNeedMobValue[i];
			if(NeedKillMobID > 0 && KillNeed > 0 && DataBotInfo::ms_aDataBot.find(NeedKillMobID) != DataBotInfo::ms_aDataBot.end())
			{
				GS()->AVM(ClientID, "null", NOPE, HideID, "- Defeat {STR} [{INT}/{INT}]",
					DataBotInfo::ms_aDataBot[NeedKillMobID].m_aNameBot, rQuestStepDataInfo.m_MobProgress[i], KillNeed);
				NeedOnlyTalk = false;
			}

			const int NeedItemID = BotInfo.m_aItemSearch[i];
			const int NeedValue = BotInfo.m_aItemSearchValue[i];
			if(NeedItemID > 0 && NeedValue > 0)
			{
				CPlayerItem* pPlayerItem = pPlayer->GetItem(NeedItemID);
				int ClapmItem = clamp(pPlayerItem->GetValue(), 0, NeedValue);
				GS()->AVM(ClientID, "null", NOPE, HideID, "- Item {STR} [{VAL}/{VAL}]", pPlayerItem->Info()->GetName(), ClapmItem, NeedValue);
				NeedOnlyTalk = false;
			}
		}

		// reward from bot after can move to up
		for(int i = 0; i < 2; i++)
		{
			const int RewardItemID = BotInfo.m_aItemGives[i];
			const int RewardValue = BotInfo.m_aItemGivesValue[i];
			if(RewardItemID > 0 && RewardValue > 0)
			{
				CItemDescription* pRewardItemInfo = GS()->GetItemInfo(RewardItemID);
				GS()->AVM(ClientID, "null", NOPE, HideID, "- Receive {STR}x{VAL}", pRewardItemInfo->GetName(), RewardValue);
			}
		}

		if(NeedOnlyTalk)
			GS()->AVM(ClientID, "null", NOPE, HideID, "You just need to talk.");
	}
}

void QuestCore::QuestShowRequired(CPlayer* pPlayer, QuestBotInfo& pBot, const char* pBuffer)
{
	const int QuestID = pBot.m_QuestID;
	CQuestData& pPlayerQuest = pPlayer->GetQuest(QuestID);
	if(pPlayerQuest.m_StepsQuestBot.find(pBot.m_SubBotID) != pPlayerQuest.m_StepsQuestBot.end())
		pPlayerQuest.m_StepsQuestBot[pBot.m_SubBotID].ShowRequired(pPlayer, pBuffer);
}

bool QuestCore::InteractiveQuestNPC(CPlayer* pPlayer, QuestBotInfo& pBot, bool FinalStepTalking)
{
	const int QuestID = pBot.m_QuestID;
	CQuestData& pPlayerQuest = pPlayer->GetQuest(QuestID);
	if(pPlayerQuest.m_StepsQuestBot.find(pBot.m_SubBotID) != pPlayerQuest.m_StepsQuestBot.end())
		return pPlayerQuest.m_StepsQuestBot[pBot.m_SubBotID].Finish(pPlayer, FinalStepTalking);
	return false;
}

void QuestCore::DoStepDropTakeItems(CPlayer* pPlayer, QuestBotInfo& pBot)
{
	const int QuestID = pBot.m_QuestID;
	CQuestData& pPlayerQuest = pPlayer->GetQuest(QuestID);
	if(pPlayerQuest.m_StepsQuestBot.find(pBot.m_SubBotID) != pPlayerQuest.m_StepsQuestBot.end())
		pPlayerQuest.m_StepsQuestBot[pBot.m_SubBotID].CreateStepDropTakeItems(pPlayer);
}

void QuestCore::AddMobProgressQuests(CPlayer* pPlayer, int BotID)
{
	// TODO Optimize algoritm check complected steps
	const int ClientID = pPlayer->GetCID();
	for(auto& pPlayerQuest : CQuestData::ms_aPlayerQuests[ClientID])
	{
		if(pPlayerQuest.second.m_State != QuestState::ACCEPT)
			continue;

		for(auto& pStepBot : pPlayerQuest.second.m_StepsQuestBot)
		{
			if(pPlayerQuest.second.m_Step == pStepBot.second.m_Bot.m_Step)
				pStepBot.second.AddMobProgress(pPlayer, BotID);
		}
	}
}

void QuestCore::UpdateArrowStep(CPlayer *pPlayer)
{
	// TODO Optimize algoritm check complected steps
	const int ClientID = pPlayer->GetCID();
	for (auto& pPlayerQuest : CQuestData::ms_aPlayerQuests[ClientID])
	{
		if(pPlayerQuest.second.m_State != QuestState::ACCEPT)
			continue;

		for(auto& pStepBot : pPlayerQuest.second.m_StepsQuestBot)
			pStepBot.second.CreateStepArrow(ClientID);
	}
}

void QuestCore::AcceptNextStoryQuestStep(CPlayer *pPlayer, int CheckQuestID)
{
	const CQuestDataInfo CheckingQuest = CQuestDataInfo::ms_aDataQuests[CheckQuestID];
	for (auto pQuestData = CQuestDataInfo::ms_aDataQuests.find(CheckQuestID); pQuestData != CQuestDataInfo::ms_aDataQuests.end(); pQuestData++)
	{
		// search next quest story step
		if(str_comp_nocase(CheckingQuest.m_aStoryLine, pQuestData->second.m_aStoryLine) == 0)
		{
			// skip all if a quest story is found that is still active
			if(pPlayer->GetQuest(pQuestData->first).GetState() == QuestState::ACCEPT)
				break;

			// accept next quest step
			if(pPlayer->GetQuest(pQuestData->first).Accept())
				break;
		}
	}
}

void QuestCore::AcceptNextStoryQuestStep(CPlayer* pPlayer)
{
	// check first quest story step search active quests
	std::list < std::string /*stories was checked*/ > StoriesChecked;
	for(const auto& pPlayerQuest : CQuestData::ms_aPlayerQuests[pPlayer->GetCID()])
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
			AcceptNextStoryQuestStep(pPlayer, pPlayerQuest.first);
		}
	}
}

int QuestCore::GetUnfrozenItemValue(CPlayer *pPlayer, int ItemID) const
{
	const int ClientID = pPlayer->GetCID();
	int AvailableValue = pPlayer->GetItem(ItemID)->GetValue();
	for (const auto& pPlayerQuest : CQuestData::ms_aPlayerQuests[ClientID])
	{
		if(pPlayerQuest.second.m_State != QuestState::ACCEPT)
			continue;

		for(auto& pStepBot : pPlayerQuest.second.m_StepsQuestBot)
		{
			if(!pStepBot.second.m_StepComplete)
				AvailableValue -= pStepBot.second.GetValueBlockedItem(pPlayer, ItemID);
		}
	}
	return max(AvailableValue, 0);
}