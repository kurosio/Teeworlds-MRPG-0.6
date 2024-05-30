/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "DialogsData.h"

#include <game/server/gamecontext.h>
#include <game/server/core/components/Quests/QuestManager.h>

// TODO: add SPEAK side / add support client 

/*
 * Information about formatting:
 * [l] - speak left side / not set right side if all sets to empty world side
 * [ls_ID] - left side bot speak with ID if not set speak Player
 * [le] - left side is empty
 * [rs_ID] - right side bot speak with ID if not set speak Bot from dialog
 * [re] - right side empty
 */

void CDialogElem::Init(int BotID, std::string Text, bool Action)
{
	// left side
	const char* pBot = str_find_nocase(Text.c_str(), "[ls_");
	if(int LeftDataBotID = 0; pBot != nullptr && sscanf(pBot, "[ls_%d]", &LeftDataBotID) && DataBotInfo::IsDataBotValid(LeftDataBotID))
	{
		char aBufSearch[16];
		str_format(aBufSearch, sizeof(aBufSearch), "[ls_%d]", LeftDataBotID);
		Text.erase(Text.find(aBufSearch), str_length(aBufSearch));
		m_LeftSide = LeftDataBotID;
		m_Flags |= DIALOGFLAG_LEFT_BOT;
	}
	else if(str_find_nocase(Text.c_str(), "[le]") != nullptr)
	{
		Text.erase(Text.find("[le]"), 4);
		m_Flags |= DIALOGFLAG_LEFT_EMPTY;
	}
	else
	{
		m_Flags |= DIALOGFLAG_LEFT_PLAYER;
	}

	// right side
	pBot = str_find_nocase(Text.c_str(), "[rs_");
	if(int RightDataBotID = 0; pBot != nullptr && sscanf(pBot, "[rs_%d]", &RightDataBotID) && DataBotInfo::IsDataBotValid(RightDataBotID))
	{
		char aBufSearch[16];
		str_format(aBufSearch, sizeof(aBufSearch), "[rs_%d]", RightDataBotID);
		Text.erase(Text.find(aBufSearch), str_length(aBufSearch));
		m_RightSide = RightDataBotID;
		m_Flags |= DIALOGFLAG_RIGHT_BOT;
	}
	else if(str_find_nocase(Text.c_str(), "[re]") != nullptr)
	{
		Text.erase(Text.find("[re]"), 4);
		m_Flags |= DIALOGFLAG_RIGHT_EMPTY;
	}
	else
	{
		m_RightSide = BotID;
		m_Flags |= DIALOGFLAG_RIGHT_BOT;
	}

	// speak left or right or author
	if(m_Flags & DIALOGFLAG_LEFT_EMPTY && m_Flags & DIALOGFLAG_RIGHT_EMPTY)
	{
		m_Flags |= DIALOGFLAG_SPEAK_AUTHOR;
	}
	else if(str_find_nocase(Text.c_str(), "[l]") != nullptr)
	{
		Text.erase(Text.find("[l]"), 3);
		m_Flags |= DIALOGFLAG_SPEAK_LEFT;
	}
	else
	{
		m_Flags |= DIALOGFLAG_SPEAK_RIGHT;
	}

	// initilize var
	m_Text = Text;
	m_Request = Action;
}

void CDialogElem::Show(CGS* pGS, int ClientID) const
{
	CPlayer* pPlayer = pGS->GetPlayer(ClientID, true);
	if(!pPlayer)
		return;

	// checking flags
	const char* pLeftNickname = nullptr;
	const char* pRightNickname = nullptr;
	if(m_Flags & DIALOGFLAG_SPEAK_AUTHOR)
	{
		pLeftNickname = "...";
	}
	else
	{
		// left sides flags
		if(m_Flags & DIALOGFLAG_LEFT_PLAYER)
			pLeftNickname = pGS->Server()->ClientName(ClientID);
		else if(m_Flags & DIALOGFLAG_LEFT_BOT)
			pLeftNickname = DataBotInfo::ms_aDataBot[m_LeftSide].m_aNameBot;

		// right sides flags
		if(m_Flags & DIALOGFLAG_RIGHT_BOT)
			pRightNickname = DataBotInfo::ms_aDataBot[m_RightSide].m_aNameBot;
	}

	// show dialog
	pPlayer->m_Dialog.FormatText(this, pLeftNickname, pRightNickname);
	pGS->Motd(ClientID, pPlayer->m_Dialog.GetCurrentText());
}

CDialogElem* CPlayerDialog::GetCurrent() const
{
	std::vector <CDialogElem>* pDialogsVector;
	if(m_BotType == TYPE_BOT_QUEST)
		pDialogsVector = &QuestBotInfo::ms_aQuestBot[m_MobID].m_aDialogs;
	else
		pDialogsVector = &NpcBotInfo::ms_aNpcBot[m_MobID].m_aDialogs;

	if(m_Step < 0 || m_Step >= static_cast<int>(pDialogsVector->size()))
		return nullptr;

	return &(*pDialogsVector)[m_Step];
}

CGS* CPlayerDialog::GS() const { return m_pPlayer->GS(); }
void CPlayerDialog::Start(int BotCID)
{
	// assert player
	dbg_assert(m_pPlayer != nullptr, "Player is not initialized on player dialog");

	// check valid bot
	const auto pPlayerBot = dynamic_cast<CPlayerBot*>(GS()->GetPlayer(BotCID));
	if(!pPlayerBot)
		return;

	// initialize variables
	Clear();
	m_Step = 0;
	m_BotCID = BotCID;
	m_BotType = pPlayerBot->GetBotType();
	m_MobID = pPlayerBot->GetBotMobID();

	// show step dialog or meaningless
	CDialogElem* pDialog = GetCurrent();
	if(!pDialog)
	{
		CDialogElem MeaninglessDialog;
		const char* pTalking[3] =
		{
			"<player>, do you have any questions? I'm sorry, can't help you.",
			"What a beautiful <time>. I don't have anything for you <player>.",
			"<player> are you interested something? I'm sorry, don't want to talk right now."
		};
		MeaninglessDialog.Init(pPlayerBot->GetBotID(), pTalking[rand() % 3], false);
		MeaninglessDialog.Show(GS(), m_pPlayer->GetCID());
		return;
	}

	// default dialogue
	ShowCurrentDialog();
}

void CPlayerDialog::TickUpdate()
{
	// is non-active
	if(!IsActive())
		return;

	// check player valid
	if(!m_pPlayer || !m_pPlayer->GetCharacter())
	{
		Clear();
		return;
	}

	// check bot valid
	const auto pPlayerBot = dynamic_cast<CPlayerBot*>(GS()->GetPlayer(m_BotCID));
	if(!pPlayerBot || !pPlayerBot->GetCharacter())
	{
		Clear();
		return;
	}

	// check distance between player and bot
	if(distance(m_pPlayer->m_ViewPos, pPlayerBot->GetCharacter()->GetPos()) > 180.0f)
	{
		Clear();
		return;
	}
}

void CPlayerDialog::FormatText(const CDialogElem* pDialog, const char* pLeftNickname, const char* pRightNickname)
{
	if(!pDialog || !m_pPlayer || m_aFormatedText[0] != '\0')
		return;

	const int ClientID = m_pPlayer->GetCID();
	const bool IsVanillaClient = !GS()->IsClientMRPG(ClientID);
	const bool IsSpeakAuthor = pDialog->GetFlag() & DIALOGFLAG_SPEAK_AUTHOR;

	/*
	 * Information format
	 */
	char aBufInformation[128] {};
	if(IsVanillaClient)
	{
		str_copy(aBufInformation, "F4 (vote no) - continue dialog\n\n\n", sizeof(aBufInformation));
	}

	/*
	 * Title format
	 */
	char aBufTittle[128] {};
	if(IsVanillaClient && m_BotType == TYPE_BOT_QUEST)
	{
		int QuestID = QuestBotInfo::ms_aQuestBot[m_MobID].m_QuestID;
		str_format(aBufTittle, sizeof(aBufTittle), "\u2766 %s\n\n", GS()->GetQuestInfo(QuestID)->GetName());
	}

	/*
	 * Nickname format
	 */
	char aBufNickname[128] {};
	if(IsVanillaClient && !IsSpeakAuthor)
	{
		if(pLeftNickname && pRightNickname)
			str_format(aBufNickname, sizeof(aBufNickname), "* %s and %s:\n", pLeftNickname, pRightNickname);
		else if(pRightNickname)
			str_format(aBufNickname, sizeof(aBufNickname), "* %s:\n", pRightNickname);
		else if(pLeftNickname)
			str_format(aBufNickname, sizeof(aBufNickname), "* %s:\n", pLeftNickname);
	}

	char aBufPosition[128];
	{
		int PageNum = m_Step;
		if(m_BotType == TYPE_BOT_QUEST)
			PageNum = static_cast<int>(QuestBotInfo::ms_aQuestBot[m_MobID].m_aDialogs.size());
		else if(m_BotType == TYPE_BOT_NPC)
			PageNum = static_cast<int>(NpcBotInfo::ms_aNpcBot[m_MobID].m_aDialogs.size());

		const char* pNicknameTalked = IsSpeakAuthor ? "..." : (pDialog->GetFlag() & DIALOGFLAG_SPEAK_LEFT ? pLeftNickname : pRightNickname);
		str_format(aBufPosition, sizeof(aBufPosition), "\u2500\u2500\u2500\u2500 | %d of %d | %s.\n", (m_Step + 1), maximum(1, PageNum), pNicknameTalked);
	}

	/*
	 * Dialog format
	 */
	char aBufText[1024] {};
	{
		str_copy(aBufText, Instance::Localize(m_pPlayer->GetCID(), pDialog->GetText()), sizeof(aBufText));

		// arrays replacing dialogs
		char aBufSearch[16];
		const char* pSearch = str_find(aBufText, "<bot_");
		while(pSearch != nullptr)
		{
			int SearchBotID = 0;
			if(sscanf(pSearch, "<bot_%d>", &SearchBotID) && DataBotInfo::IsDataBotValid(SearchBotID))
			{
				str_format(aBufSearch, sizeof(aBufSearch), "<bot_%d>", SearchBotID);
				str_replace(aBufText, aBufSearch, DataBotInfo::ms_aDataBot[SearchBotID].m_aNameBot);
			}
			pSearch = str_find(aBufText, "<bot_");
		}

		pSearch = str_find(aBufText, "<world_");
		while(pSearch != nullptr)
		{
			int WorldID = 0;
			if(sscanf(pSearch, "<world_%d>", &WorldID))
			{
				str_format(aBufSearch, sizeof(aBufSearch), "<world_%d>", WorldID);
				str_replace(aBufText, aBufSearch, GS()->Server()->GetWorldName(WorldID));
			}
			pSearch = str_find(aBufText, "<world_");
		}

		pSearch = str_find(aBufText, "<item_");
		while(pSearch != nullptr)
		{
			int ItemID = 0;
			if(sscanf(pSearch, "<item_%d>", &ItemID) && (CItemDescription::Data().find(ItemID) != CItemDescription::Data().end()))
			{
				str_format(aBufSearch, sizeof(aBufSearch), "<item_%d>", ItemID);
				str_replace(aBufText, aBufSearch, GS()->GetItemInfo(ItemID)->GetName());
			}
			pSearch = str_find(aBufText, "<item_");
		}

		// based replacing dialogs
		str_replace(aBufText, "<player>", GS()->Server()->ClientName(ClientID));
		str_replace(aBufText, "<time>", GS()->Server()->GetStringTypeday());
		str_replace(aBufText, "<here>", GS()->Server()->GetWorldName(GS()->GetWorldID()));
		str_replace(aBufText, "<eidolon>", m_pPlayer->GetEidolon() ? DataBotInfo::ms_aDataBot[m_pPlayer->GetEidolon()->GetBotID()].m_aNameBot : "Eidolon");
	}

	/*
	 * Quest task format
	 */
	char aBufQuestTask[256] {};
	if(m_BotType == TYPE_BOT_QUEST && pDialog->IsRequestAction())
	{
		// check for client and send quest tables
		GS()->Core()->QuestManager()->QuestShowRequired(m_pPlayer, QuestBotInfo::ms_aQuestBot[m_MobID], aBufQuestTask, sizeof(aBufQuestTask));
	}

	// copy all formated data
	str_format(m_aFormatedText, sizeof(m_aFormatedText), "%s%s%s%s\u00ab%s\u00bb%s", aBufNickname, aBufInformation, aBufTittle, aBufPosition, aBufText, aBufQuestTask);
}

void CPlayerDialog::ClearText()
{
	mem_zero(m_aFormatedText, sizeof(m_aFormatedText));
}

void CPlayerDialog::Init(CPlayer* pPlayer)
{
	m_pPlayer = pPlayer;
}

void CPlayerDialog::Next()
{
	CDialogElem* pDialog = GetCurrent();
	if(!pDialog || !m_pPlayer)
	{
		Clear();
		return;
	}

	// request action
	if(pDialog->IsRequestAction())
	{
		// bot type NPC (who giving Quests)
		if(m_BotType == TYPE_BOT_NPC)
		{
			int QuestID = NpcBotInfo::ms_aNpcBot[m_MobID].m_GiveQuestID;
			m_pPlayer->GetQuest(QuestID)->Accept();
		}

		// bot type Quest (who requred tasks)
		else if(m_BotType == TYPE_BOT_QUEST)
		{
			int ClientID = m_pPlayer->GetCID();
			int QuestID = QuestBotInfo::ms_aQuestBot[m_MobID].m_QuestID;

			CPlayerQuest* pQuest = m_pPlayer->GetQuest(QuestID);
			CQuestStep* pStep = pQuest->GetStepByMob(m_MobID);
			if(!pStep)
				return;

			if(!pStep->IsComplete())
			{
				GS()->Chat(ClientID, "The tasks haven't been completed yet!");
				ShowCurrentDialog();
				return;
			}

			GS()->CreatePlayerSound(m_pPlayer->GetCID(), SOUND_CTF_RETURN);
			DialogEvents(DIALOGEVENTCUR::ON_COMPLETE_TASK);
		}
	}

	// next step
	m_Step++;

	// clear text buffer and allow update
	ClearText();

	// post next
	PostNext();
}

void CPlayerDialog::PostNext()
{
	CDialogElem* pCurrent = GetCurrent();

	// is last dialog
	if(!pCurrent)
	{
		// finish quest on last dialogue
		if(m_BotType == TYPE_BOT_QUEST)
		{
			int QuestID = QuestBotInfo::ms_aQuestBot[m_MobID].m_QuestID;
			bool RunEndDialogEvent = m_pPlayer->GetQuest(QuestID)->GetStepByMob(m_MobID)->Finish();

			// clear and run end event
			if(RunEndDialogEvent)
			{
				DialogEvents(DIALOGEVENTCUR::ON_END);
			}
		}

		Clear();
		return;
	}

	// post dialog action
	if(pCurrent->IsRequestAction())
	{
		if(m_BotType == TYPE_BOT_QUEST)
		{
			int QuestID = QuestBotInfo::ms_aQuestBot[m_MobID].m_QuestID;
			CPlayerQuest* pQuest = m_pPlayer->GetQuest(QuestID);
			pQuest->GetStepByMob(m_MobID)->CreateVarietyTypesRequiredItems();

			// tasks receive
			CQuestStep* pStep = pQuest->GetStepByMob(m_MobID);
			if(!pStep->m_TaskListReceived)
			{
				pStep->m_TaskListReceived = true;
				pStep->UpdateTaskMoveTo();
			}
		}
	}

	// show next dialog
	ShowCurrentDialog();
}

void CPlayerDialog::Clear()
{
	dbg_assert(m_pPlayer != nullptr, "Player is not initialized on player dialog");

	// send empty motd for clear it's
	if(IsActive())
	{
		GS()->Motd(m_pPlayer->GetCID(), "\0");
	}

	// clear var
	m_Step = 0;
	m_BotCID = -1;
	m_BotType = -1;
	m_MobID = -1;
	ClearText();
}

void CPlayerDialog::DialogEvents(DIALOGEVENTCUR Pos) const
{
	std::string EventData {};
	if(m_BotType == TYPE_BOT_QUEST)
	{
		EventData = QuestBotInfo::ms_aQuestBot[m_MobID].m_EventJsonData;
	}

	Tools::Json::parseFromString(EventData, [Pos, this](nlohmann::json& pJson)
	{
		const char* pElem = nullptr;
		switch(Pos)
		{
			case DIALOGEVENTCUR::ON_RECIEVE_TASK: pElem = "on_recieve_task"; break;
			case DIALOGEVENTCUR::ON_COMPLETE_TASK: pElem = "on_complete_task"; break;
			default: pElem = "on_end"; break;
		}

		const auto& pEventObjectJson = pJson[pElem];
		const int ClientID = m_pPlayer->GetCID();

		// messages array
		if(pEventObjectJson.contains("messages"))
		{
			const auto& pChatArrayJson = pEventObjectJson["messages"];

			std::string Text;
			for(const auto& p : pChatArrayJson)
			{
				Text = p.value("text", "\0");

				// broadcast type
				if(p.contains("broadcast") && p.value("broadcast", false))
				{
					GS()->Broadcast(ClientID, BroadcastPriority::MAIN_INFORMATION, 300, Text.c_str());
				}
				else // chat type
				{
					GS()->Chat(ClientID, Text.c_str());
				}
			}
		}

		// effect object
		if(pEventObjectJson.contains("effect"))
		{
			const auto& pEffectObjectJson = pEventObjectJson["effect"];

			std::string Effect = pEffectObjectJson.value("name", "\0");
			int Seconds = pEffectObjectJson.value("seconds", 0);
			if(!Effect.empty())
			{
				m_pPlayer->GiveEffect(Effect.c_str(), Seconds);
			}
		}

		// only by end
		if(Pos == DIALOGEVENTCUR::ON_END)
		{
			// teleport object
			if(pEventObjectJson.contains("teleport"))
			{
				const auto& pTeleport = pEventObjectJson["teleport"];
				vec2 Position(pTeleport.value("x", -1.0f), pTeleport.value("y", -1.0f));

				// change world
				if(pTeleport.find("world_id") != pTeleport.end() && m_pPlayer->GetPlayerWorldID() != pTeleport.value("world_id", MAIN_WORLD_ID))
				{
					m_pPlayer->GetTempData().SetTeleportPosition(Position);
					m_pPlayer->ChangeWorld(pTeleport.value("world_id", MAIN_WORLD_ID));
					return;
				}

				// or change position only
				m_pPlayer->GetCharacter()->ChangePosition(Position);
			}
		}
	});
}

void CPlayerDialog::ShowCurrentDialog() const
{
	CDialogElem* pCurrent = GetCurrent();

	// tasks receive
	if(m_BotType == TYPE_BOT_QUEST)
	{
		if(pCurrent->IsRequestAction())
		{
			int QuestID = QuestBotInfo::ms_aQuestBot[m_MobID].m_QuestID;
			CPlayerQuest* pQuest = m_pPlayer->GetQuest(QuestID);
			CQuestStep* pStep = pQuest->GetStepByMob(m_MobID);

			if(!pStep->m_TaskListReceived)
			{
				pStep->m_TaskListReceived = true;
				pStep->UpdateTaskMoveTo();

				// run event recieve task
				DialogEvents(DIALOGEVENTCUR::ON_RECIEVE_TASK);
			}
		}
	}

	// show dialog
	pCurrent->Show(GS(), m_pPlayer->GetCID());
}
