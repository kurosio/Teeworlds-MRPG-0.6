/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "QuestCore.h"

#include <game/server/gamecontext.h>

void QuestCore::OnInit()
{
	ResultPtr pRes = SJK.SD("*", "tw_quests_list");
	while(pRes->next())
	{
		const int QUID = pRes->getInt("ID");
		CQuestDataInfo::ms_aDataQuests[QUID].m_QuestID = QUID;
		str_copy(CQuestDataInfo::ms_aDataQuests[QUID].m_aName, pRes->getString("Name").c_str(), sizeof(CQuestDataInfo::ms_aDataQuests[QUID].m_aName));
		str_copy(CQuestDataInfo::ms_aDataQuests[QUID].m_aStoryLine, pRes->getString("StoryLine").c_str(), sizeof(CQuestDataInfo::ms_aDataQuests[QUID].m_aStoryLine));
		CQuestDataInfo::ms_aDataQuests[QUID].m_Gold = (int)pRes->getInt("Money");
		CQuestDataInfo::ms_aDataQuests[QUID].m_Exp = (int)pRes->getInt("Exp");
	}
}

void QuestCore::OnInitAccount(CPlayer* pPlayer)
{
	const int ClientID = pPlayer->GetCID();
	ResultPtr pRes = SJK.SD("*", "tw_accounts_quests", "WHERE UserID = '%d'", pPlayer->Acc().m_UserID);
	while(pRes->next())
	{
		const int QuestID = pRes->getInt("QuestID");
		CQuestData::ms_aPlayerQuests[ClientID][QuestID].m_pPlayer = pPlayer;
		CQuestData::ms_aPlayerQuests[ClientID][QuestID].m_QuestID = QuestID;
		CQuestData::ms_aPlayerQuests[ClientID][QuestID].m_State = (int)pRes->getInt("Type");
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
			pStepBot.second.UpdateBot(GS());
		}
	}
	CQuestData::ms_aPlayerQuests.erase(ClientID);
}

void QuestCore::OnMessage(int MsgID, void* pRawMsg, int ClientID)
{
	CPlayer* pPlayer = GS()->m_apPlayers[ClientID];
	if(MsgID == NETMSGTYPE_CL_TALKINTERACTIVE)
	{
		if(pPlayer->m_aPlayerTick[LastDialog] && pPlayer->m_aPlayerTick[LastDialog] > Server()->Tick())
			return;

		pPlayer->m_aPlayerTick[LastDialog] = Server()->Tick() + (Server()->TickSpeed() / 4);
		pPlayer->SetTalking(pPlayer->GetTalkedID(), false);
	}
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

	if(Menulist == MENU_JOURNAL_FINISHED)
	{
		pPlayer->m_LastVoteMenu = MENU_JOURNAL_MAIN;
		ShowQuestsTabList(pPlayer, QUEST_FINISHED);
		GS()->AddVotesBackpage(ClientID);
		return true;
	}

	return false;
}

bool QuestCore::OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, const int VoteID, const int VoteID2, int Get, const char* GetText)
{
	return false;
}

static const char* GetStateName(int Type)
{
	switch(Type)
	{
		case QUEST_ACCEPT: return "Active";
		case QUEST_FINISHED: return "Finished";
		default: return "Not active";
	}
}

void QuestCore::ShowQuestsMainList(CPlayer* pPlayer)
{
	ShowQuestsTabList(pPlayer, QUEST_ACCEPT);
	ShowQuestsTabList(pPlayer, QUEST_NO_ACCEPT);

	// show the completed menu
	pPlayer->m_VoteColored = BLUE_COLOR;
	GS()->AVM(pPlayer->GetCID(), "MENU", MENU_JOURNAL_FINISHED, NOPE, "List of completed quests");
}

void QuestCore::ShowQuestsTabList(CPlayer* pPlayer, int StateQuest)
{
	const int ClientID = pPlayer->GetCID();
	pPlayer->m_VoteColored = GOLDEN_COLOR;
	GS()->AVL(ClientID, "null", "{STR} quests", GetStateName(StateQuest));

	// check first quest story step
	bool IsEmptyList = true;
	pPlayer->m_VoteColored = LIGHT_GOLDEN_COLOR;
	std::list < std::string /*stories was checked*/ > StoriesChecked;
	for(const auto& pDataQuest : CQuestDataInfo::ms_aDataQuests)
	{
		if(pPlayer->GetQuest(pDataQuest.first).GetState() != StateQuest)
			continue;

		if(StateQuest == QUEST_FINISHED)
		{
			ShowQuestID(pPlayer, pDataQuest.first);
			IsEmptyList = false;
			continue;
		}

		const auto& IsAlreadyChecked = std::find_if(StoriesChecked.begin(), StoriesChecked.end(), [=](const std::string& stories)
		{ return (str_comp_nocase(CQuestDataInfo::ms_aDataQuests[pDataQuest.first].m_aStoryLine, stories.c_str()) == 0); });
		if(IsAlreadyChecked == StoriesChecked.end())
		{
			StoriesChecked.emplace_back(CQuestDataInfo::ms_aDataQuests[pDataQuest.first].m_aStoryLine);
			ShowQuestID(pPlayer, pDataQuest.first);
			IsEmptyList = false;
		}
	}

	// if the quest list is empty
	if(IsEmptyList)
	{
		pPlayer->m_VoteColored = LIGHT_GOLDEN_COLOR;
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

	// TODO: REMOVE IT
	GS()->AVCALLBACK(ClientID, "MENU", "\0", QuestID, NOPE, NOPE, [](CVoteOptionsCallback Callback)
	{
		CPlayer* pPlayer = Callback.pPlayer;
		const int ClientID = pPlayer->GetCID();
		const int QuestID = Callback.VoteID;
		CQuestDataInfo pData = pPlayer->GetQuest(QuestID).Info();

		pPlayer->GS()->ClearVotes(ClientID);
		pPlayer->GS()->Mmo()->Quest()->ShowQuestsActiveNPC(pPlayer, QuestID);
		pPlayer->GS()->AV(ClientID, "null");

		pPlayer->m_VoteColored = GOLDEN_COLOR;
		pPlayer->GS()->AVL(ClientID, "null", "{STR} : Reward", pData.GetName());
		pPlayer->m_VoteColored = LIGHT_GOLDEN_COLOR;
		pPlayer->GS()->AVL(ClientID, "null", "Gold: {INT} Exp: {INT}", pData.m_Gold, pData.m_Exp);

		pPlayer->m_LastVoteMenu = MENU_JOURNAL_MAIN;
		pPlayer->GS()->AddVotesBackpage(ClientID);
	}, "{INT}/{INT} {STR}: {STR}", QuestPosition, QuestsSize, pData.GetStory(), pData.GetName());
}

// active npc information display
void QuestCore::ShowQuestsActiveNPC(CPlayer* pPlayer, int QuestID)
{
	CQuestData& pPlayerQuest = pPlayer->GetQuest(QuestID);
	const int ClientID = pPlayer->GetCID();
	pPlayer->m_VoteColored = BLUE_COLOR;
	GS()->AVM(ClientID, "null", NOPE, NOPE, "Active NPC for current quests");

	for(auto& pStepBot : CQuestDataInfo::ms_aDataQuests[QuestID].m_StepsQuestBot)
	{
		// header
		QuestBotInfo* pBotInfo = pStepBot.second.m_Bot;
		const int HideID = (NUM_TAB_MENU + pBotInfo->m_SubBotID);
		const vec2 Pos = pBotInfo->m_Position / 32.0f;
		const char* pSymbol = (((pPlayerQuest.GetState() == QUEST_ACCEPT && pPlayerQuest.m_StepsQuestBot[pStepBot.first].m_StepComplete) || pPlayerQuest.GetState() ==
			                       QUEST_FINISHED) ? "✔ " : "\0");
		GS()->AVH(ClientID, HideID, LIGHT_BLUE_COLOR, "{STR}Step {INT}. {STR} {STR}(x{INT} y{INT})", pSymbol, pBotInfo->m_Step, pBotInfo->GetName(), Server()->GetWorldName(pBotInfo->m_WorldID), (int)Pos.x, (int)Pos.y);

		// skipped non accepted task list
		if(pPlayerQuest.GetState() != QUEST_ACCEPT)
		{
			GS()->AVM(ClientID, "null", NOPE, HideID, "Quest been completed, or not accepted!");
			continue;
		}

		// need for bot
		bool NeedOnlyTalk = true;
		for(int i = 0; i < 2; i++)
		{
			const int NeedKillMobID = pBotInfo->m_aNeedMob[i];
			const int KillNeed = pBotInfo->m_aNeedMobValue[i];
			if(NeedKillMobID > 0 && KillNeed > 0 && DataBotInfo::ms_aDataBot.find(NeedKillMobID) != DataBotInfo::ms_aDataBot.end())
			{
				GS()->AVMI(ClientID, "broken_h", "null", NOPE, HideID, "- Defeat {STR} [{INT}/{INT}]",
					DataBotInfo::ms_aDataBot[NeedKillMobID].m_aNameBot, pPlayerQuest.m_StepsQuestBot[pStepBot.first].m_MobProgress[i], KillNeed);
				NeedOnlyTalk = false;
			}

			const int NeedItemID = pBotInfo->m_aItemSearch[i];
			const int NeedValue = pBotInfo->m_aItemSearchValue[i];
			if(NeedItemID > 0 && NeedValue > 0)
			{
				CItemData PlayerItem = pPlayer->GetItem(NeedItemID);
				int ClapmItem = clamp(PlayerItem.m_Value, 0, NeedValue);
				GS()->AVMI(ClientID, PlayerItem.Info().GetIcon(), "null", NOPE, HideID, "- Item {STR} [{INT}/{INT}]", PlayerItem.Info().GetName(), ClapmItem, NeedValue);
				NeedOnlyTalk = false;
			}
		}

		// reward from bot after can move to up
		for(int i = 0; i < 2; i++)
		{
			const int RewardItemID = pBotInfo->m_aItemGives[i];
			const int RewardValue = pBotInfo->m_aItemGivesValue[i];
			if(RewardItemID > 0 && RewardValue > 0)
			{
				CItemDataInfo &RewardItem = GS()->GetItemInfo(RewardItemID);
				GS()->AVMI(ClientID, RewardItem.GetIcon(), "null", NOPE, HideID, "- Receive {STR}x{INT}", RewardItem.GetName(), RewardValue);
			}
		}

		if(NeedOnlyTalk)
			GS()->AVM(ClientID, "null", NOPE, HideID, "You just need to talk.");
	}
}

void QuestCore::QuestShowRequired(CPlayer* pPlayer, QuestBotInfo& pBot, const char* TextTalk)
{
	const int QuestID = pBot.m_QuestID;
	CQuestData& pPlayerQuest = pPlayer->GetQuest(QuestID);
	if(pPlayerQuest.m_StepsQuestBot.find(pBot.m_SubBotID) != pPlayerQuest.m_StepsQuestBot.end())
		pPlayerQuest.m_StepsQuestBot[pBot.m_SubBotID].ShowRequired(pPlayer, TextTalk);
}

void QuestCore::QuestTableAddInfo(int ClientID, const char* pText, int Requires, int Have)
{
	if(ClientID < 0 || ClientID >= MAX_PLAYERS || !GS()->IsMmoClient(ClientID))
		return;

	CNetMsg_Sv_AddQuestingProcessing Msg;
	Msg.m_pText = pText;
	Msg.m_pRequiresNum = Requires;
	Msg.m_pHaveNum = clamp(Have, 0, Requires);
	Msg.m_pGivingTable = false;
	StrToInts(Msg.m_pIcon, 4, "hammer");
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void QuestCore::QuestTableClear(int ClientID)
{
	if(ClientID < 0 || ClientID >= MAX_PLAYERS || !GS()->IsMmoClient(ClientID))
		return;

	CNetMsg_Sv_ClearQuestingProcessing Msg;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
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
		if(pPlayerQuest.second.m_State != QUEST_ACCEPT)
			continue;

		for(auto& pStepBot : pPlayerQuest.second.m_StepsQuestBot)
		{
			if(pPlayerQuest.second.m_Step == pStepBot.second.m_Bot->m_Step)
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
		if(pPlayerQuest.second.m_State != QUEST_ACCEPT)
			continue;

		for(auto& pStepBot : pPlayerQuest.second.m_StepsQuestBot)
			pStepBot.second.CreateStepArrow(pPlayer);
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
			if(pPlayer->GetQuest(pQuestData->first).GetState() == QUEST_ACCEPT)
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
		if(pPlayerQuest.second.GetState() != QUEST_FINISHED)
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
void QuestCore::QuestTableAddItem(int ClientID, const char* pText, int Requires, int ItemID, bool GivingTable)
{
	CPlayer* pPlayer = GS()->GetPlayer(ClientID, true);
	if (!pPlayer || ItemID < itGold || !GS()->IsMmoClient(ClientID))
		return;

	const CItemData PlayerSelectedItem = pPlayer->GetItem(ItemID);

	CNetMsg_Sv_AddQuestingProcessing Msg;
	Msg.m_pText = pText;
	Msg.m_pRequiresNum = Requires;
	Msg.m_pHaveNum = clamp(PlayerSelectedItem.m_Value, 0, Requires);
	Msg.m_pGivingTable = GivingTable;
	StrToInts(Msg.m_pIcon, 4, PlayerSelectedItem.Info().GetIcon());
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

int QuestCore::GetUnfrozenItemValue(CPlayer *pPlayer, int ItemID) const
{
	const int ClientID = pPlayer->GetCID();
	int AvailableValue = pPlayer->GetItem(ItemID).m_Value;
	for (const auto& pPlayerQuest : CQuestData::ms_aPlayerQuests[ClientID])
	{
		if(pPlayerQuest.second.m_State != QUEST_ACCEPT)
			continue;

		for(auto& pStepBot : pPlayerQuest.second.m_StepsQuestBot)
		{
			if(!pStepBot.second.m_StepComplete)
				AvailableValue -= pStepBot.second.GetValueBlockedItem(pPlayer, ItemID);
		}
	}
	return max(AvailableValue, 0);
}
