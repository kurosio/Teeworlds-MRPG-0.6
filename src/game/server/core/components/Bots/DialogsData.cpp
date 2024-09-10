/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "DialogsData.h"

#include <game/server/gamecontext.h>
#include <game/server/core/components/quests/quest_manager.h>

void CDialogElem::Init(int BotID, const nlohmann::json& JsonDialog)
{
	// initialize variables
	const std::string Side = JsonDialog.value("side", "left");
	const int LeftSpeakerID = JsonDialog.value("left_speaker_id", 0);
	const int RightSpeakerID = JsonDialog.value("right_speaker_id", 0);
	m_Text = JsonDialog.value("text", "");
	m_Request = JsonDialog.value("action", false);

	// initialize author
	if(Side == "author")
	{
		m_Flags |= DIALOGFLAG_SPEAK_AUTHOR;
		return;
	}

	// initialize thoughts
	if(Side == "thoughts")
	{
		if(LeftSpeakerID == -1)
		{
			m_Flags |= DIALOGFLAG_LEFT_PLAYER;
		}
		else if(LeftSpeakerID == 0)
		{
			m_Flags |= DIALOGFLAG_LEFT_BOT;
			m_LeftSide = BotID;
		}
		else
		{
			m_LeftSide = LeftSpeakerID;
			m_Flags |= DIALOGFLAG_LEFT_BOT;
		}

		m_Flags |= DIALOGFLAG_SPEAK_THOUGHTS;
		return;
	}

	// initialize left-side
	if(LeftSpeakerID == -1)
	{
		m_Flags |= DIALOGFLAG_LEFT_PLAYER;
	}
	else if(LeftSpeakerID == 0)
	{
		m_Flags |= DIALOGFLAG_LEFT_BOT;
		m_LeftSide = BotID;
	}
	else
	{
		m_LeftSide = LeftSpeakerID;
		m_Flags |= DIALOGFLAG_LEFT_BOT;
	}

	// initialize right-side
	if(RightSpeakerID == -1)
	{
		m_Flags |= DIALOGFLAG_RIGHT_PLAYER;
	}
	else if(RightSpeakerID == 0)
	{
		m_Flags |= DIALOGFLAG_RIGHT_BOT;
		m_RightSide = BotID;
	}
	else
	{
		m_Flags |= DIALOGFLAG_RIGHT_BOT;
		m_RightSide = RightSpeakerID;
	}
}

void CDialogElem::Show(CGS* pGS, int ClientID) const
{
	CPlayer* pPlayer = pGS->GetPlayer(ClientID, true);
	if(!pPlayer) return;

	// Determine nicknames based on flags
	const char* pLeftNickname = nullptr;
	const char* pRightNickname = nullptr;

	if(m_Flags & DIALOGFLAG_SPEAK_AUTHOR)
	{
		pLeftNickname = "...";
	}
	else
	{
		if(m_Flags & DIALOGFLAG_LEFT_PLAYER)
			pLeftNickname = pGS->Server()->ClientName(ClientID);
		else if(m_Flags & DIALOGFLAG_LEFT_BOT)
			pLeftNickname = DataBotInfo::ms_aDataBot[m_LeftSide].m_aNameBot;

		if(m_Flags & DIALOGFLAG_RIGHT_PLAYER)
			pRightNickname = pGS->Server()->ClientName(ClientID);
		else if(m_Flags & DIALOGFLAG_RIGHT_BOT)
			pRightNickname = DataBotInfo::ms_aDataBot[m_RightSide].m_aNameBot;
	}

	// Show dialog
	pPlayer->m_Dialog.FormatText(this, pLeftNickname, pRightNickname);
	pGS->Motd(ClientID, pPlayer->m_Dialog.GetCurrentText());
}

CDialogElem* CPlayerDialog::GetCurrent() const
{
	std::vector<CDialogElem>* pDialogsVector;
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
	// Assert player
	dbg_assert(m_pPlayer != nullptr, "Player is not initialized on player dialog");

	// Check if bot is valid
	const auto pPlayerBot = dynamic_cast<CPlayerBot*>(GS()->GetPlayer(BotCID));
	if(!pPlayerBot) return;

	// Initialize variables
	Clear();
	m_Step = 0;
	m_BotCID = BotCID;
	m_BotType = pPlayerBot->GetBotType();
	m_MobID = pPlayerBot->GetBotMobID();

	// Show current dialog or meaningless dialog
	CDialogElem* pDialog = GetCurrent();
	if(!pDialog)
	{
		CDialogElem MeaninglessDialog;
		const char* pTalking[] = {
			"<player>, do you have any questions? I'm sorry, can't help you.",
			"What a beautiful <time>. I don't have anything for you <player>.",
			"<player>, are you interested in something? I'm sorry, don't want to talk right now."
		};

		// create structure
		nlohmann::json JsonDialog;
		JsonDialog["text"] = pTalking[rand() % 3];
		JsonDialog["action"] = false;
		JsonDialog["side"] = "right";
		JsonDialog["left_speaker_id"] = 0;
		JsonDialog["right_speaker_id"] = pPlayerBot->GetBotID();

		// initialize and show dialogue
		MeaninglessDialog.Init(pPlayerBot->GetBotID(), JsonDialog);
		MeaninglessDialog.Show(GS(), m_pPlayer->GetCID());
		return;
	}

	ShowCurrentDialog();
}

void CPlayerDialog::Tick()
{
	// Check if dialog is active
	if(!IsActive()) return;

	// Validate player
	if(!m_pPlayer || !m_pPlayer->GetCharacter())
	{
		Clear();
		return;
	}

	// Validate bot
	const auto pPlayerBot = dynamic_cast<CPlayerBot*>(GS()->GetPlayer(m_BotCID));
	if(!pPlayerBot || !pPlayerBot->GetCharacter())
	{
		Clear();
		return;
	}

	// Check distance between player and bot
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
			str_format(aBufNickname, sizeof(aBufNickname), "* %s says to %s:\n", pLeftNickname, pRightNickname);
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

		const char* pNicknameTalked = IsSpeakAuthor ? "..." : pLeftNickname;
		str_format(aBufPosition, sizeof(aBufPosition), "\u2500\u2500\u2500\u2500 | %d of %d | %s.\n", (m_Step + 1), maximum(1, PageNum), pNicknameTalked);
	}

	/*
	 * Dialog format
	 */
	char aBufText[1024] {};
	{
		str_copy(aBufText, Instance::Localize(m_pPlayer->GetCID(), pDialog->GetText()), sizeof(aBufText));

		// Arrays replacing dialogs
		auto replace_placeholders = [&](const std::string& prefix, auto get_replacement)
		{
			const char* pSearch = str_find(aBufText, prefix.c_str());
			while(pSearch != nullptr)
			{
				int id = 0;
				pSearch += prefix.length();
				if(sscanf(pSearch, "%d>", &id) == 1)
				{
					char aBufSearch[16];
					str_format(aBufSearch, sizeof(aBufSearch), "%s%d>", prefix.c_str(), id);
					str_replace(aBufText, aBufSearch, get_replacement(id));
				}

				pSearch = str_find(aBufText, prefix.c_str());
			}
		};

		replace_placeholders("<bot_", [](int id) { return DataBotInfo::ms_aDataBot[id].m_aNameBot; });
		replace_placeholders("<world_", [&](int id) { return GS()->Server()->GetWorldName(id); });
		replace_placeholders("<item_", [&](int id) { return GS()->GetItemInfo(id)->GetName(); });

		// Based replacing dialogs
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
		GS()->Core()->QuestManager()->PrepareRequiredBuffer(m_pPlayer, QuestBotInfo::ms_aQuestBot[m_MobID], aBufQuestTask, sizeof(aBufQuestTask));
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

	// Request action handling
	if(pDialog->IsRequestAction())
	{
		// Handle NPC bot type
		if(m_BotType == TYPE_BOT_NPC)
		{
			int QuestID = NpcBotInfo::ms_aNpcBot[m_MobID].m_GiveQuestID;
			m_pPlayer->GetQuest(QuestID)->Accept();
		}
		// Handle Quest bot type
		else if(m_BotType == TYPE_BOT_QUEST)
		{
			int ClientID = m_pPlayer->GetCID();
			int QuestID = QuestBotInfo::ms_aQuestBot[m_MobID].m_QuestID;

			CPlayerQuest* pQuest = m_pPlayer->GetQuest(QuestID);
			CQuestStep* pStep = pQuest->GetStepByMob(m_MobID);
			if(!pStep) return;

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

	// Move to next step
	m_Step++;
	ClearText();
	PostNext();
}

void CPlayerDialog::PostNext()
{
	CDialogElem* pCurrent = GetCurrent();

	// Check if last dialog
	if(!pCurrent)
	{
		// Handle end of quest
		if(m_BotType == TYPE_BOT_QUEST)
		{
			int QuestID = QuestBotInfo::ms_aQuestBot[m_MobID].m_QuestID;
			bool RunEndDialogEvent = m_pPlayer->GetQuest(QuestID)->GetStepByMob(m_MobID)->Finish();

			if(RunEndDialogEvent)
			{
				DialogEvents(DIALOGEVENTCUR::ON_END);
			}
		}

		Clear();
		return;
	}

	// Handle dialog actions
	if(pCurrent->IsRequestAction())
	{
		if(m_BotType == TYPE_BOT_QUEST)
		{
			int QuestID = QuestBotInfo::ms_aQuestBot[m_MobID].m_QuestID;
			CPlayerQuest* pQuest = m_pPlayer->GetQuest(QuestID);
			pQuest->GetStepByMob(m_MobID)->CreateVarietyTypesRequiredItems();

			CQuestStep* pStep = pQuest->GetStepByMob(m_MobID);
			if(!pStep->m_TaskListReceived)
			{
				pStep->m_TaskListReceived = true;
				pStep->UpdateTaskMoveTo();
			}
		}
	}

	// Show next dialog
	ShowCurrentDialog();
}

void CPlayerDialog::Clear()
{
	if(IsActive())
	{
		dbg_assert(m_pPlayer != nullptr, "Player is not initialized on player dialog");
		GS()->Motd(m_pPlayer->GetCID(), "\0");
	}

	m_Step = 0;
	m_BotCID = -1;
	m_BotType = -1;
	m_MobID = -1;
	ClearText();
}

void CPlayerDialog::DialogEvents(DIALOGEVENTCUR Pos) const
{
	std::string EventData;
	if(m_BotType == TYPE_BOT_QUEST)
	{
		EventData = QuestBotInfo::ms_aQuestBot[m_MobID].m_EventJsonData;
	}

	mystd::json::parse(EventData, [Pos, this](nlohmann::json& pJson) 
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

		// Handle messages
		if(pEventObjectJson.contains("messages"))
		{
			const auto& pChatArrayJson = pEventObjectJson["messages"];

			for(const auto& p : pChatArrayJson)
			{
				std::string Text = p.value("text", "\0");

				if(p.contains("broadcast") && p.value("broadcast", false))
				{
					GS()->Broadcast(ClientID, BroadcastPriority::MAIN_INFORMATION, 300, Text.c_str());
				}
				else
				{
					GS()->Chat(ClientID, Text.c_str());
				}
			}
		}

		// Handle effects
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

		// Handle teleport
		if(Pos == DIALOGEVENTCUR::ON_END && pEventObjectJson.contains("teleport"))
		{
			const auto& pTeleport = pEventObjectJson["teleport"];
			vec2 Position(pTeleport.value("x", -1.0f), pTeleport.value("y", -1.0f));

			if(pTeleport.find("world_id") != pTeleport.end() && m_pPlayer->GetPlayerWorldID() != pTeleport.value("world_id", MAIN_WORLD_ID))
			{
				m_pPlayer->GetTempData().SetTeleportPosition(Position);
				m_pPlayer->ChangeWorld(pTeleport.value("world_id", MAIN_WORLD_ID));
				return;
			}

			m_pPlayer->GetCharacter()->ChangePosition(Position);
		}
	});
}

void CPlayerDialog::ShowCurrentDialog() const
{
	CDialogElem* pCurrent = GetCurrent();

	if(m_BotType == TYPE_BOT_QUEST && pCurrent->IsRequestAction())
	{
		int QuestID = QuestBotInfo::ms_aQuestBot[m_MobID].m_QuestID;
		CPlayerQuest* pQuest = m_pPlayer->GetQuest(QuestID);
		CQuestStep* pStep = pQuest->GetStepByMob(m_MobID);

		if(!pStep->m_TaskListReceived)
		{
			pStep->m_TaskListReceived = true;
			pStep->UpdateTaskMoveTo();

			DialogEvents(DIALOGEVENTCUR::ON_RECIEVE_TASK);
		}
	}

	pCurrent->Show(GS(), m_pPlayer->GetCID());
}
