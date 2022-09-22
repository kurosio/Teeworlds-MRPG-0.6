/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "BotCore.h"

#include <game/server/gamecontext.h>

#include <game/server/mmocore/Components/Quests/QuestCore.h>

static int GetIntegerEmoteValue(const char* JsonName, const char* JsonValue)
{
	if(str_comp(JsonName, "emote") == 0)
	{
		if(str_comp(JsonValue, "pain") == 0) return EMOTE_PAIN;
		if(str_comp(JsonValue, "happy") == 0) return EMOTE_HAPPY;
		if(str_comp(JsonValue, "surprise") == 0) return EMOTE_SURPRISE;
		if(str_comp(JsonValue, "blink") == 0) return EMOTE_BLINK;
		if(str_comp(JsonValue, "angry") == 0) return EMOTE_ANGRY;
		return EMOTE_NORMAL;
	}
	return 0;
}

static void InitInformationBots()
{
	ResultPtr pRes = Sqlpool.Execute<DB::SELECT>("*", "tw_bots_info");
	while(pRes->next())
	{
		const int BotID = pRes->getInt("ID");
		str_copy(DataBotInfo::ms_aDataBot[BotID].m_aNameBot, pRes->getString("Name").c_str(), sizeof(DataBotInfo::ms_aDataBot[BotID].m_aNameBot));
		DataBotInfo::ms_aDataBot[BotID].m_aEquipSlot[EQUIP_HAMMER] = pRes->getInt("SlotHammer");
		DataBotInfo::ms_aDataBot[BotID].m_aEquipSlot[EQUIP_GUN] = pRes->getInt("SlotGun");
		DataBotInfo::ms_aDataBot[BotID].m_aEquipSlot[EQUIP_SHOTGUN] = pRes->getInt("SlotShotgun");
		DataBotInfo::ms_aDataBot[BotID].m_aEquipSlot[EQUIP_GRENADE] = pRes->getInt("SlotGrenade");
		DataBotInfo::ms_aDataBot[BotID].m_aEquipSlot[EQUIP_RIFLE] = pRes->getInt("SlotRifle");
		DataBotInfo::ms_aDataBot[BotID].m_aEquipSlot[EQUIP_MINER] = 0;
		DataBotInfo::ms_aDataBot[BotID].m_aEquipSlot[EQUIP_WINGS] = pRes->getInt("SlotWings");

		// load teeinfo
		try
		{
			std::string JsonString = pRes->getString("JsonTeeInfo").c_str();
			nlohmann::json JsonData = nlohmann::json::parse(JsonString);

			CTeeInfo TeeInfo{};
			str_copy(TeeInfo.m_aSkinName, JsonData.value("skin", "default").c_str(), sizeof(TeeInfo.m_aSkinName));
			TeeInfo.m_UseCustomColor = JsonData.value("custom_color", 0);
			TeeInfo.m_ColorBody = JsonData.value("color_body", -1);
			TeeInfo.m_ColorFeet = JsonData.value("color_feet", -1);
			DataBotInfo::ms_aDataBot[BotID].m_TeeInfos = TeeInfo;
		}
		catch (nlohmann::json::exception& s)
		{
			dbg_msg("bot parse", "teeinfo [from bot id %d] (json %s)", BotID, s.what());
		}

		for(int i = 0; i < MAX_PLAYERS; i++)
			DataBotInfo::ms_aDataBot[BotID].m_aVisibleActive[i] = false;

	}
}

void CBotCore::OnInit()
{
	InitInformationBots();
}

void CBotCore::OnInitWorld(const char* pWhereLocalWorld)
{
	InitQuestBots(pWhereLocalWorld);
	InitNPCBots(pWhereLocalWorld);
	InitMobsBots(pWhereLocalWorld);
}

// Initialization of quest bots
void CBotCore::InitQuestBots(const char* pWhereLocalWorld)
{
	ResultPtr pRes = Sqlpool.Execute<DB::SELECT>("*", "tw_bots_quest", pWhereLocalWorld);
	while(pRes->next())
	{
		const int MobID = pRes->getInt("ID");
		const int QuestID = pRes->getInt("QuestID");
		std::string DialogJsonStr = pRes->getString("DialogData").c_str();

		QuestBotInfo::ms_aQuestBot[MobID].m_SubBotID = MobID;
		QuestBotInfo::ms_aQuestBot[MobID].m_QuestID = QuestID;
		QuestBotInfo::ms_aQuestBot[MobID].m_BotID = pRes->getInt("BotID");
		QuestBotInfo::ms_aQuestBot[MobID].m_Step = pRes->getInt("Step");
		QuestBotInfo::ms_aQuestBot[MobID].m_WorldID = pRes->getInt("WorldID");
		QuestBotInfo::ms_aQuestBot[MobID].m_Position = vec2(pRes->getInt("PosX"), pRes->getInt("PosY") + 1);
		QuestBotInfo::ms_aQuestBot[MobID].m_aItemSearch[0] = pRes->getInt("RequiredItemID1");
		QuestBotInfo::ms_aQuestBot[MobID].m_aItemSearch[1] = pRes->getInt("RequiredItemID2");
		QuestBotInfo::ms_aQuestBot[MobID].m_aItemGives[0] = pRes->getInt("RewardItemID1");
		QuestBotInfo::ms_aQuestBot[MobID].m_aItemGives[1] = pRes->getInt("RewardItemID2");
		QuestBotInfo::ms_aQuestBot[MobID].m_aNeedMob[0] = pRes->getInt("RequiredDefeatMobID1");
		QuestBotInfo::ms_aQuestBot[MobID].m_aNeedMob[1] = pRes->getInt("RequiredDefeatMobID2");
		QuestBotInfo::ms_aQuestBot[MobID].m_InteractiveType = pRes->getInt("InteractionType");
		QuestBotInfo::ms_aQuestBot[MobID].m_InteractiveTemp = pRes->getInt("InteractionTemp");
		QuestBotInfo::ms_aQuestBot[MobID].m_GenerateNick = pRes->getBoolean("GenerateSubName");
		sscanf(pRes->getString("Amount").c_str(), "|%d|%d|%d|%d|%d|%d|",
			&QuestBotInfo::ms_aQuestBot[MobID].m_aItemSearchValue[0], &QuestBotInfo::ms_aQuestBot[MobID].m_aItemSearchValue[1],
			&QuestBotInfo::ms_aQuestBot[MobID].m_aItemGivesValue[0], &QuestBotInfo::ms_aQuestBot[MobID].m_aItemGivesValue[1],
			&QuestBotInfo::ms_aQuestBot[MobID].m_aNeedMobValue[0], &QuestBotInfo::ms_aQuestBot[MobID].m_aNeedMobValue[1]);
		if(QuestID > 0)
			CQuestDataInfo::ms_aDataQuests[QuestID].m_StepsQuestBot[MobID].m_Bot = &QuestBotInfo::ms_aQuestBot[MobID];

		// load dialog
		try
		{
			if(DialogJsonStr.length() >= 10)
			{
				nlohmann::json JsonData = nlohmann::json::parse(DialogJsonStr.c_str());
				for(auto& pItem : JsonData)
				{
					DialogData LoadTalk;
					str_copy(LoadTalk.m_aText, pItem.value("text", "").c_str(), sizeof(LoadTalk.m_aText));
					LoadTalk.m_Emote = GetIntegerEmoteValue("emote", pItem.value("emote", "").c_str());
					LoadTalk.m_RequestAction = pItem.value("action_step", 0);
					LoadTalk.LoadFlags();

					QuestBotInfo::ms_aQuestBot[MobID].m_aDialog.push_back(LoadTalk);
				}
			}
		}
		catch(nlohmann::json::exception& s)
		{
			dbg_msg("dialog error", "dialog [quest bot id %d] (json %s)", pRes->getInt("ID"), s.what());
		}
	}
}

// Initialization of NPC bots
void CBotCore::InitNPCBots(const char* pWhereLocalWorld)
{
	ResultPtr pRes = Sqlpool.Execute<DB::SELECT>("*", "tw_bots_npc", pWhereLocalWorld);
	while(pRes->next())
	{
		const int MobID = pRes->getInt("ID");
		const int NumberOfNpc = pRes->getInt("Number");
		std::string DialogJsonStr = pRes->getString("DialogData").c_str();

		NpcBotInfo::ms_aNpcBot[MobID].m_WorldID = pRes->getInt("WorldID");
		NpcBotInfo::ms_aNpcBot[MobID].m_Static = pRes->getBoolean("Static");
		NpcBotInfo::ms_aNpcBot[MobID].m_Position = vec2(pRes->getInt("PosX"), pRes->getInt("PosY") + (NpcBotInfo::ms_aNpcBot[MobID].m_Static ? 1 : 0));
		NpcBotInfo::ms_aNpcBot[MobID].m_Emote = pRes->getInt("Emote");
		NpcBotInfo::ms_aNpcBot[MobID].m_BotID = pRes->getInt("BotID");
		NpcBotInfo::ms_aNpcBot[MobID].m_Function = pRes->getInt("Function");
		NpcBotInfo::ms_aNpcBot[MobID].m_GivesQuestID = pRes->getInt("GivesQuestID");
		if(NpcBotInfo::ms_aNpcBot[MobID].m_GivesQuestID > 0)
			NpcBotInfo::ms_aNpcBot[MobID].m_Function = FUNCTION_NPC_GIVE_QUEST;
		for(int c = 0; c < NumberOfNpc; c++)
			GS()->CreateBot(TYPE_BOT_NPC, NpcBotInfo::ms_aNpcBot[MobID].m_BotID, MobID);

		// load dialog
		try
		{
			if(DialogJsonStr.length() >= 10)
			{
				DialogData LoadTalk;
				nlohmann::json JsonData = nlohmann::json::parse(DialogJsonStr.c_str());
				for(auto& pItem : JsonData)
				{
					str_copy(LoadTalk.m_aText, pItem.value("text", "").c_str(), sizeof(LoadTalk.m_aText));
					LoadTalk.m_Emote = GetIntegerEmoteValue("emote", pItem.value("emote", "normal").c_str());
					LoadTalk.LoadFlags();

					NpcBotInfo::ms_aNpcBot[MobID].m_aDialog.push_back(LoadTalk);
				}
			}
		}
		catch(nlohmann::json::exception& s)
		{
			dbg_msg("dialog error", "dialog [npc bot id %d] (json %s)", pRes->getInt("ID"), s.what());
		}
	}
}

// Initialization of mobs bots
void CBotCore::InitMobsBots(const char* pWhereLocalWorld)
{
	ResultPtr pRes = Sqlpool.Execute<DB::SELECT>("*", "tw_bots_mobs", pWhereLocalWorld);
	while(pRes->next())
	{
		const int MobID = pRes->getInt("ID");
		const int BotID = pRes->getInt("BotID");
		const int NumberOfMobs = pRes->getInt("Number");
		
		MobBotInfo::ms_aMobBot[MobID].m_WorldID = pRes->getInt("WorldID");
		MobBotInfo::ms_aMobBot[MobID].m_Position = vec2(pRes->getInt("PositionX"), pRes->getInt("PositionY"));
		MobBotInfo::ms_aMobBot[MobID].m_Power = pRes->getInt("Power");
		MobBotInfo::ms_aMobBot[MobID].m_Spread = pRes->getInt("Spread");
		MobBotInfo::ms_aMobBot[MobID].m_Boss = pRes->getBoolean("Boss");
		MobBotInfo::ms_aMobBot[MobID].m_Level = pRes->getInt("Level");
		MobBotInfo::ms_aMobBot[MobID].m_RespawnTick = pRes->getInt("Respawn");
		MobBotInfo::ms_aMobBot[MobID].m_BotID = BotID;
		str_copy(MobBotInfo::ms_aMobBot[MobID].m_aEffect, pRes->getString("Effect").c_str(), sizeof(MobBotInfo::ms_aMobBot[MobID].m_aEffect));
		str_copy(MobBotInfo::ms_aMobBot[MobID].m_aBehavior, pRes->getString("Behavior").c_str(), sizeof(MobBotInfo::ms_aMobBot[MobID].m_aBehavior));

		char aBuf[32];
		for(int i = 0; i < MAX_DROPPED_FROM_MOBS; i++)
		{
			str_format(aBuf, sizeof(aBuf), "it_drop_%d", i);
			MobBotInfo::ms_aMobBot[MobID].m_aDropItem[i] = pRes->getInt(aBuf);
		}
		sscanf(pRes->getString("it_drop_count").c_str(), "|%d|%d|%d|%d|%d|",
			&MobBotInfo::ms_aMobBot[MobID].m_aValueItem[0], &MobBotInfo::ms_aMobBot[MobID].m_aValueItem[1], &MobBotInfo::ms_aMobBot[MobID].m_aValueItem[2],
			&MobBotInfo::ms_aMobBot[MobID].m_aValueItem[3], &MobBotInfo::ms_aMobBot[MobID].m_aValueItem[4]);
		sscanf(pRes->getString("it_drop_chance").c_str(), "|%f|%f|%f|%f|%f|",
			&MobBotInfo::ms_aMobBot[MobID].m_aRandomItem[0], &MobBotInfo::ms_aMobBot[MobID].m_aRandomItem[1], &MobBotInfo::ms_aMobBot[MobID].m_aRandomItem[2],
			&MobBotInfo::ms_aMobBot[MobID].m_aRandomItem[3], &MobBotInfo::ms_aMobBot[MobID].m_aRandomItem[4]);
		for(int c = 0; c < NumberOfMobs; c++)
			GS()->CreateBot(TYPE_BOT_MOB, BotID, MobID);
	}
}

void CBotCore::ProcessingTalkingNPC(int OwnID, int TalkingID, const char *Message, int Emote, int TalkedFlag) const
{
	GS()->Motd(OwnID, Message);
}

void CBotCore::DialogBotStepNPC(CPlayer* pPlayer, int MobID, int Progress, int TalkedID, const char *pText)
{
	const int SizeTalking = NpcBotInfo::ms_aNpcBot[MobID].m_aDialog.size();
	if(!NpcBotInfo::IsNpcBotValid(MobID) || Progress >= SizeTalking)
	{
		pPlayer->ClearTalking();
		return;
	}

	const int ClientID = pPlayer->GetCID();
	GS()->Broadcast(ClientID, BroadcastPriority::GAME_PRIORITY, 100, "Press 'F4' to continue the dialog!");

	char reformTalkedText[512];
	const int BotID = NpcBotInfo::ms_aNpcBot[MobID].m_BotID;
	if (str_comp_nocase(pText, "empty") != 0)
	{
		pPlayer->FormatDialogText(BotID, pText);

		str_format(reformTalkedText, sizeof(reformTalkedText), "( 1 of 1 ) %s:\n- %s", NpcBotInfo::ms_aNpcBot[MobID].GetName(), pPlayer->GetDialogText());
		GS()->Broadcast(ClientID, BroadcastPriority::GAME_PRIORITY, 100, "Press 'F4' to continue the dialog!");

		pPlayer->ClearDialogText();
		GS()->Mmo()->BotsData()->ProcessingTalkingNPC(ClientID, TalkedID, reformTalkedText, EMOTE_BLINK, TALKED_FLAG_FULL|TALKED_FLAG_SAYS_BOT);
		return;
	}

	const int DialogFlag = NpcBotInfo::ms_aNpcBot[MobID].m_aDialog[Progress].m_Flag;
	pPlayer->FormatDialogText(BotID, NpcBotInfo::ms_aNpcBot[MobID].m_aDialog[Progress].m_aText);

	const char* TalkedNick = "\0";
	if(DialogFlag & TALKED_FLAG_SAYS_PLAYER)
		TalkedNick = Server()->ClientName(ClientID);
	else if(DialogFlag & TALKED_FLAG_BOT)
		TalkedNick = NpcBotInfo::ms_aNpcBot[MobID].GetName();

	str_format(reformTalkedText, sizeof(reformTalkedText), "( %d of %d ) %s:\n- %s", (1 + Progress), SizeTalking, TalkedNick, pPlayer->GetDialogText());
	GS()->Broadcast(ClientID, BroadcastPriority::GAME_PRIORITY, 100, "Press 'F4' to continue the dialog!");

	pPlayer->ClearDialogText();
	GS()->Mmo()->BotsData()->ProcessingTalkingNPC(ClientID, TalkedID, reformTalkedText, NpcBotInfo::ms_aNpcBot[MobID].m_aDialog[Progress].m_Emote, DialogFlag);
}

void CBotCore::DialogBotStepQuest(CPlayer* pPlayer, int MobID, int Progress, int TalkedID)
{
	const int SizeTalking = QuestBotInfo::ms_aQuestBot[MobID].m_aDialog.size();
	if(!QuestBotInfo::IsQuestBotValid(MobID) || Progress >= SizeTalking)
	{
		pPlayer->ClearTalking();
		return;
	}

	const int ClientID = pPlayer->GetCID();
	char reformTalkedText[512];
	const int BotID = QuestBotInfo::ms_aQuestBot[MobID].m_BotID;
	const int DialogFlag = QuestBotInfo::ms_aQuestBot[MobID].m_aDialog[Progress].m_Flag;
	pPlayer->FormatDialogText(BotID, QuestBotInfo::ms_aQuestBot[MobID].m_aDialog[Progress].m_aText);

	const char* TalkedNick = "\0";
	const int QuestID = QuestBotInfo::ms_aQuestBot[MobID].m_QuestID;
	if(DialogFlag & TALKED_FLAG_SAYS_PLAYER)
		TalkedNick = Server()->ClientName(ClientID);
	else if(DialogFlag & TALKED_FLAG_BOT)
		TalkedNick = QuestBotInfo::ms_aQuestBot[MobID].GetName();
	str_format(reformTalkedText, sizeof(reformTalkedText), "%s\n=========\n\n( %d of %d ) %s:\n- %s",
		GS()->GetQuestInfo(QuestID).GetName(), (1 + Progress), SizeTalking, TalkedNick, pPlayer->GetDialogText());
	GS()->Broadcast(ClientID, BroadcastPriority::GAME_PRIORITY, 100, "Press 'F4' to continue the dialog!");

	pPlayer->ClearDialogText();
	GS()->Mmo()->BotsData()->ProcessingTalkingNPC(ClientID, TalkedID, reformTalkedText, QuestBotInfo::ms_aQuestBot[MobID].m_aDialog[Progress].m_Emote, DialogFlag);
}

void CBotCore::ShowBotQuestTaskInfo(CPlayer* pPlayer, int MobID, int Progress)
{
	const int ClientID = pPlayer->GetCID();
	const int SizeTalking = QuestBotInfo::ms_aQuestBot[MobID].m_aDialog.size();
	if (!QuestBotInfo::IsQuestBotValid(MobID) || Progress >= SizeTalking)
	{
		pPlayer->ClearTalking();
		return;
	}

	// vanila clients
	const int BotID = QuestBotInfo::ms_aQuestBot[MobID].m_BotID;
	const int QuestID = QuestBotInfo::ms_aQuestBot[MobID].m_QuestID;
	const int DialogFlag = QuestBotInfo::ms_aQuestBot[MobID].m_aDialog[Progress].m_Flag;
	const char* TalkedNick = "\0";
	if(DialogFlag & TALKED_FLAG_SAYS_PLAYER)
		TalkedNick = Server()->ClientName(ClientID);
	else if(DialogFlag & TALKED_FLAG_BOT)
		TalkedNick = QuestBotInfo::ms_aQuestBot[MobID].GetName();

	char reformTalkedText[512];
	pPlayer->FormatDialogText(BotID, QuestBotInfo::ms_aQuestBot[MobID].m_aDialog[Progress].m_aText);
	str_format(reformTalkedText, sizeof(reformTalkedText), "%s\n=========\n\n( %d of %d ) %s:\n- %s",
		GS()->GetQuestInfo(QuestID).GetName(), (1 + Progress), SizeTalking, TalkedNick, pPlayer->GetDialogText());
	pPlayer->ClearDialogText();

	GS()->Mmo()->Quest()->QuestShowRequired(pPlayer, QuestBotInfo::ms_aQuestBot[MobID], reformTalkedText);
}

int CBotCore::GetQuestNPC(int MobID)
{
	if (!NpcBotInfo::IsNpcBotValid(MobID))
		return -1;

	return NpcBotInfo::ms_aNpcBot[MobID].m_GivesQuestID;
}


const char* CBotCore::GetMeaninglessDialog()
{
	const char* pTalking[3] =
	{
		"[Player], do you have any questions? I'm sorry, can't help you.",
		"What a beautiful [Time]. I don't have anything for you [Player].",
		"[Player] are you interested something? I'm sorry, don't want to talk right now."
	};
	return pTalking[random_int()%3];
}

// add a new bot
void CBotCore::ConAddCharacterBot(int ClientID, const char* pCharacter)
{
	CPlayer* pPlayer = GS()->GetPlayer(ClientID);
	if(!pPlayer)
		return;

	nlohmann::json JsonTeeInfo;
	JsonTeeInfo["skin"] = pPlayer->m_TeeInfos.m_aSkinName;
	JsonTeeInfo["custom_color"] = pPlayer->m_TeeInfos.m_UseCustomColor;
	JsonTeeInfo["color_body"] = pPlayer->m_TeeInfos.m_ColorBody;
	JsonTeeInfo["color_feet"] = pPlayer->m_TeeInfos.m_ColorFeet;

	// check the nick
	CSqlString<16> cNick = CSqlString<16>(pCharacter);
	ResultPtr pRes = Sqlpool.Execute<DB::SELECT>("*", "tw_bots_info", "WHERE Name = '%s'", cNick.cstr());
	if(pRes->next())
	{
		// if the nickname is not in the database
		Sqlpool.Execute<DB::UPDATE>("tw_bots_info", "JsonTeeInfo = '%s' WHERE ID = '%d'", JsonTeeInfo.dump().c_str());
		GS()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "parseskin", "Updated character bot!");
		return;
	}

	// add a new bot
	Sqlpool.Execute<DB::INSERT>("tw_bots_info", "(Name, JsonTeeInfo) VALUES ('%s', '%s')", cNick.cstr(), JsonTeeInfo.dump().c_str());
	GS()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "parseskin", "Added new character bot!");
}
