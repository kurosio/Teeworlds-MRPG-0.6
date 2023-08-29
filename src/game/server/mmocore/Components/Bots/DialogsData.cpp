/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "DialogsData.h"

#include <game/server/gamecontext.h>
#include <game/server/mmocore/Components/Quests/QuestManager.h>

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

// TODO: Replace not optimized for search
int CDialogElem::GetClientIDByBotID(CGS* pGS, int CheckVisibleForCID, int BotID) const
{
	int CurrentPosCID = -1;
	float LastDistance = 1e10f;
	for(int i = MAX_PLAYERS; i < MAX_CLIENTS; i++)
	{
		if(!pGS->m_apPlayers[i] || !pGS->m_apPlayers[i]->GetCharacter())
			continue;

		if(const CPlayerBot* pPlayerBot = dynamic_cast<CPlayerBot*>(pGS->m_apPlayers[i]); 
			pPlayerBot->GetBotID() == BotID && pPlayerBot->IsVisibleForClient(CheckVisibleForCID))
		{
			const CPlayer* pPlayer = pGS->m_apPlayers[CheckVisibleForCID];
			if(const float Distance = distance(pPlayerBot->GetCharacter()->GetPos(), pPlayer->m_ViewPos); 
				Distance < LastDistance)
			{
				LastDistance = Distance;
				CurrentPosCID = pPlayerBot->GetCID();
			}
		}
	}

	return CurrentPosCID;
}

void CDialogElem::Show(CGS* pGS, int ClientID)
{
	CPlayer* pPlayer = pGS->GetPlayer(ClientID, true);
	if(!pPlayer)
		return;

	int LeftSideClientID = -1;
	int RightSideClientID = -1;
	const char* pLeftNickname = nullptr;
	const char* pRightNickname = nullptr;

	// checking flags
	if(m_Flags & DIALOGFLAG_SPEAK_AUTHOR)
	{
		pLeftNickname = "...";
	}
	else
	{
		// left sides flags
		if(m_Flags & DIALOGFLAG_LEFT_PLAYER)
		{
			LeftSideClientID = ClientID;
			pLeftNickname = pGS->Server()->ClientName(LeftSideClientID);

		}
		else if(m_Flags & DIALOGFLAG_LEFT_BOT)
		{
			// search clientid by bot id or dissable flag what left side it's bot
			if(LeftSideClientID = GetClientIDByBotID(pGS, ClientID, m_LeftSide); LeftSideClientID == -1)
			{
				m_Flags ^= DIALOGFLAG_LEFT_BOT;
				m_Flags |= DIALOGFLAG_LEFT_EMPTY;
			}
			else
			{
				pLeftNickname = DataBotInfo::ms_aDataBot[m_LeftSide].m_aNameBot;
			}
		}

		// right sides flags
		if(m_Flags & DIALOGFLAG_RIGHT_BOT)
		{
			// search clientid by bot id or dissable flag what right side it's bot
			if(RightSideClientID = GetClientIDByBotID(pGS, ClientID, m_RightSide); RightSideClientID == -1)
			{
				m_Flags ^= DIALOGFLAG_RIGHT_BOT;
				m_Flags |= DIALOGFLAG_RIGHT_EMPTY;
			}
			else
			{
				pRightNickname = DataBotInfo::ms_aDataBot[m_RightSide].m_aNameBot;
			}
		}
	}

	// show dialog
	pPlayer->m_Dialog.FormatText(this, pLeftNickname, pRightNickname);
	if(pGS->IsClientMRPG(ClientID))
	{
		CNetMsg_Sv_Dialog Msg;
		Msg.m_LeftClientID = LeftSideClientID;
		Msg.m_RightClientID = RightSideClientID;
		Msg.m_pText = pPlayer->m_Dialog.GetCurrentText();
		Msg.m_Flag = m_Flags;
		pGS->Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
	}
	else
	{
		pGS->Motd(ClientID, pPlayer->m_Dialog.GetCurrentText());
	}
}

CDialogElem* CPlayerDialog::GetCurrent() const
{
	std::vector <CDialogElem>* pDialogsVector = m_BotType == TYPE_BOT_QUEST ? 
		&QuestBotInfo::ms_aQuestBot[m_MobID].m_aDialogs : &NpcBotInfo::ms_aNpcBot[m_MobID].m_aDialogs;

	if(m_Step < 0 || m_Step >= static_cast<int>(pDialogsVector->size()))
		return nullptr;

	return &(*pDialogsVector)[m_Step];
}

CGS* CPlayerDialog::GS() const { return m_pPlayer->GS(); }
void CPlayerDialog::Start(CPlayer* pPlayer, int BotCID)
{
	m_pPlayer = pPlayer;
	if(!pPlayer || !GS()->GetPlayer(BotCID))
		return;

	Clear();

	const CPlayerBot* pPlayerBot = dynamic_cast<CPlayerBot*>(GS()->m_apPlayers[BotCID]);

	m_Step = 0;
	m_BotType = pPlayerBot->GetBotType();
	m_BotCID = BotCID;
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
		MeaninglessDialog.Init(pPlayerBot->GetBotID(), pTalking[random_int() % 3], false);
		MeaninglessDialog.Show(GS(), pPlayer->GetCID());
	}
	else
	{
		pDialog->Show(GS(), pPlayer->GetCID());
	}
}

void CPlayerDialog::TickUpdate()
{
	if(!m_pPlayer || !m_pPlayer->GetCharacter() || m_BotCID < MAX_PLAYERS || !GS()->m_apPlayers[m_BotCID]  || !GS()->m_apPlayers[m_BotCID]->GetCharacter()
		|| distance(m_pPlayer->m_ViewPos, GS()->m_apPlayers[m_BotCID]->GetCharacter()->GetPos()) > 180.0f)
		Clear();
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
	char aBufInformation[128]{};
	if(IsVanillaClient)
	{
		str_copy(aBufInformation, "F4 (vote no) - continue dialog\n\n\n", sizeof(aBufInformation));
	}

	/*
	 * Title format
	 */
	char aBufTittle[128]{};
	if(IsVanillaClient && m_BotType == TYPE_BOT_QUEST)
	{
		int QuestID = QuestBotInfo::ms_aQuestBot[m_MobID].m_QuestID;
		str_format(aBufTittle, sizeof(aBufTittle), "### %s\n\n", GS()->GetQuestInfo(QuestID).GetName());
	}

	/*
	 * Nickname format
	 */
	char aBufNickname[128] {};
	if(IsVanillaClient && !IsSpeakAuthor)
	{
		if(pLeftNickname && pRightNickname)
			str_format(aBufNickname, sizeof(aBufNickname), "%s and %s:\n\n", pLeftNickname, pRightNickname);
		else if(pRightNickname)
			str_format(aBufNickname, sizeof(aBufNickname), "%s:\n\n", pRightNickname);
		else if(pLeftNickname)
			str_format(aBufNickname, sizeof(aBufNickname), "%s:\n\n", pLeftNickname);
	}

	char aBufPosition[128];
	{
		int PageNum = m_Step;
		if(m_BotType == TYPE_BOT_QUEST)
			PageNum = static_cast<int>(QuestBotInfo::ms_aQuestBot[m_MobID].m_aDialogs.size());
		else if(m_BotType == TYPE_BOT_NPC)
			PageNum = static_cast<int>(NpcBotInfo::ms_aNpcBot[m_MobID].m_aDialogs.size());

		const char* pNicknameTalked = IsSpeakAuthor ? "..." : (pDialog->GetFlag() & DIALOGFLAG_SPEAK_LEFT ? pLeftNickname : pRightNickname);
		str_format(aBufPosition, sizeof(aBufPosition), "--------- ( %d of %d ) %s ---------\n", (m_Step + 1), max(1, PageNum), pNicknameTalked);
	}

	/*
	 * Dialog format
	 */
	char aBufText[1024]{};
	{
		str_copy(aBufText, GS()->Server()->Localization()->Localize(m_pPlayer->GetLanguage(), pDialog->GetText()), sizeof(aBufText));

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
		str_replace(aBufText, "<time>", GS()->Server()->GetStringTypeDay());
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
		GS()->Mmo()->Quest()->QuestShowRequired(m_pPlayer, QuestBotInfo::ms_aQuestBot[m_MobID], aBufQuestTask, sizeof(aBufQuestTask));
	}

	// copy all formated data
	str_format(m_aFormatedText, sizeof(m_aFormatedText), "%s%s%s%s%s%s", aBufInformation, aBufTittle, aBufNickname, aBufPosition, aBufText, aBufQuestTask);
}

void CPlayerDialog::ClearText()
{
	mem_zero(m_aFormatedText, sizeof(m_aFormatedText));
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
			CQuestData* pQuest = m_pPlayer->GetQuest(QuestID);

			if(!pQuest->GetMobStep(m_MobID)->IsComplete(m_pPlayer))
			{
				GS()->Chat(ClientID, "Task has not been completed yet!");
				pDialog->Show(GS(), ClientID);
				return;
			}

			GS()->CreatePlayerSound(m_pPlayer->GetCID(), SOUND_CTF_RETURN);
		}

		// run event
		DialogEvents();
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
		bool RunEndDialogEvent = true;

		// finish quest on last dialogue
		if(m_BotType == TYPE_BOT_QUEST)
		{
			int QuestID = QuestBotInfo::ms_aQuestBot[m_MobID].m_QuestID;
			RunEndDialogEvent = m_pPlayer->GetQuest(QuestID)->GetMobStep(m_MobID)->Finish(m_pPlayer);
		}

		// clear and run post events
		if(RunEndDialogEvent)
		{
			EndDialogEvents();
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
			CQuestData* pQuest = m_pPlayer->GetQuest(QuestID);
			pQuest->GetMobStep(m_MobID)->CreateStepDropTakeItems(m_pPlayer);
		}
	}

	// show next dialog
	pCurrent->Show(GS(), m_pPlayer->GetCID());
}

void CPlayerDialog::Clear()
{
	// clear var
	m_Step = 0;
	m_BotCID = -1;
	m_BotType = -1;
	m_MobID = -1;
	ClearText();

	// send information packet about clear
	if(m_pPlayer)
	{
		int ClientID = m_pPlayer->GetCID();
		if(GS()->IsClientMRPG(ClientID))
		{
			CNetMsg_Sv_ClearDialog Msg;
			GS()->Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);

			GS()->Motd(ClientID, "\0");
		}
		else
			GS()->Motd(ClientID, "\0");
	}
}

void CPlayerDialog::DialogEvents() const
{
	std::string EventData {};
	if(m_BotType == TYPE_BOT_QUEST)
		EventData = QuestBotInfo::ms_aQuestBot[m_MobID].m_EventJsonData;

	JsonTools::parseFromString(EventData, [this](nlohmann::json& pJson)
	{
		// Information event
		int ClientID = m_pPlayer->GetCID();
		if(pJson.find("chat") != pJson.end())
		{
			for(auto& p : pJson["chat"])
			{
				std::string Text = p.value("text", "\0");
				if(p.find("broadcast") != p.end() && p.value("broadcast", 0))
					GS()->Broadcast(ClientID, BroadcastPriority::MAIN_INFORMATION, 300, Text.c_str());
				else
					GS()->Chat(ClientID, Text.c_str());
			}
		}

		// Effect event
		if(pJson.find("effect") != pJson.end())
		{
			auto pEffect = pJson["effect"];
			std::string Effect = pEffect.value("name", "\0");
			int Seconds = pEffect.value("seconds", 0);

			if(!Effect.empty())
				m_pPlayer->GiveEffect(Effect.c_str(), Seconds);
		}
	});
}

void CPlayerDialog::EndDialogEvents() const
{
	std::string EventData {};
	if(m_BotType == TYPE_BOT_QUEST)
		EventData = QuestBotInfo::ms_aQuestBot[m_MobID].m_EventJsonData;

	// post event
	JsonTools::parseFromString(EventData, [this](nlohmann::json& pJson)
	{
		// Teleport event
		if(pJson.find("teleport") != pJson.end())
		{
			auto pTeleport = pJson["teleport"];
			vec2 Position(pTeleport.value("x", -1.0f), pTeleport.value("y", -1.0f));

			// change world
			if(pTeleport.find("world_id") != pTeleport.end() && m_pPlayer->GetPlayerWorldID() != pTeleport.value("world_id", MAIN_WORLD_ID))
			{
				m_pPlayer->GetTempData().m_TempTeleportPos = Position;
				m_pPlayer->ChangeWorld(pTeleport.value("world_id", MAIN_WORLD_ID));
				return;
			}

			// or change position only
			m_pPlayer->GetCharacter()->ChangePosition(Position);
		}
	});
}