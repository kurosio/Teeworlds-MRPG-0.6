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
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_bots_info");
	while(pRes->next())
	{
		const int BotID = pRes->getInt("ID");

		DataBotInfo BotInfo;
		str_copy(BotInfo.m_aNameBot, pRes->getString("Name").c_str(), sizeof(BotInfo.m_aNameBot));
		BotInfo.m_aEquipSlot[EQUIP_HAMMER] = pRes->getInt("SlotHammer");
		BotInfo.m_aEquipSlot[EQUIP_GUN] = pRes->getInt("SlotGun");
		BotInfo.m_aEquipSlot[EQUIP_SHOTGUN] = pRes->getInt("SlotShotgun");
		BotInfo.m_aEquipSlot[EQUIP_GRENADE] = pRes->getInt("SlotGrenade");
		BotInfo.m_aEquipSlot[EQUIP_LASER] = pRes->getInt("SlotRifle");
		BotInfo.m_aEquipSlot[EQUIP_ARMOR] = pRes->getInt("SlotArmor");
		BotInfo.m_aEquipSlot[EQUIP_PICKAXE] = 0;
		BotInfo.m_aEquipSlot[EQUIP_RAKE] = 0;
		BotInfo.m_aEquipSlot[EQUIP_EIDOLON] = 0;

		// load teeinfo
		std::string JsonString = pRes->getString("JsonTeeInfo").c_str();
		JsonTools::parseFromString(JsonString, [&](nlohmann::json& pJson)
		{
			str_copy(BotInfo.m_TeeInfos.m_aSkinName, pJson.value("skin", "default").c_str(), sizeof(BotInfo.m_TeeInfos.m_aSkinName));
			BotInfo.m_TeeInfos.m_UseCustomColor = pJson.value("custom_color", 0);
			BotInfo.m_TeeInfos.m_ColorBody = pJson.value("color_body", -1);
			BotInfo.m_TeeInfos.m_ColorFeet = pJson.value("color_feet", -1);
		});

		for(int i = 0; i < MAX_PLAYERS; i++)
			BotInfo.m_aVisibleActive[i] = false;

		DataBotInfo::ms_aDataBot[BotID] = BotInfo;
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
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_bots_quest", pWhereLocalWorld);
	while(pRes->next())
	{
		const int MobID = pRes->getInt("ID");
		const int QuestID = pRes->getInt("QuestID");

		// load from database
		QuestBotInfo QuestBot;
		QuestBot.m_SubBotID = MobID;
		QuestBot.m_QuestID = QuestID;
		QuestBot.m_BotID = pRes->getInt("BotID");
		QuestBot.m_Step = pRes->getInt("Step");
		QuestBot.m_WorldID = pRes->getInt("WorldID");
		QuestBot.m_Position = vec2(pRes->getInt("PosX"), pRes->getInt("PosY") + 1);
		QuestBot.m_aItemSearch[0] = pRes->getInt("RequiredItemID1");
		QuestBot.m_aItemSearch[1] = pRes->getInt("RequiredItemID2");
		QuestBot.m_aItemGives[0] = pRes->getInt("RewardItemID1");
		QuestBot.m_aItemGives[1] = pRes->getInt("RewardItemID2");
		QuestBot.m_aNeedMob[0] = pRes->getInt("RequiredDefeatMobID1");
		QuestBot.m_aNeedMob[1] = pRes->getInt("RequiredDefeatMobID2");
		QuestBot.m_InteractiveType = pRes->getInt("InteractionType");
		QuestBot.m_InteractiveTemp = pRes->getInt("InteractionTemp");
		//QuestBot.m_GenerateNick = pRes->getBoolean("GenerateSubName");
		QuestBot.m_EventJsonData = pRes->getString("EventData").c_str();
		sscanf(pRes->getString("Amount").c_str(), "|%d|%d|%d|%d|%d|%d|",
			&QuestBot.m_aItemSearchValue[0], &QuestBot.m_aItemSearchValue[1], &QuestBot.m_aItemGivesValue[0], &QuestBot.m_aItemGivesValue[1], &QuestBot.m_aNeedMobValue[0], &QuestBot.m_aNeedMobValue[1]);
		QuestBot.m_HasAction = false;

		std::string DialogJsonStr = pRes->getString("DialogData").c_str();
		JsonTools::parseFromString(DialogJsonStr, [&](nlohmann::json& pJson)
		{
			for(auto& pItem : pJson)
			{
				const int Emote = GetIntegerEmoteValue("emote", pItem.value("emote", "").c_str());
				bool ActionStep = pItem.value("action_step", 0);

				if(ActionStep)
					QuestBot.m_HasAction = ActionStep;

				CDialog Dialogue;
				Dialogue.Init(QuestBot.m_BotID, pItem.value("text", ""), Emote, ActionStep);
				QuestBot.m_aDialogs.push_back(Dialogue);
			}
		});

		// initilize
		QuestBotInfo::ms_aQuestBot[MobID] = QuestBot;
		if(QuestID > 0)
		{
			CQuestDataInfo::ms_aDataQuests[QuestID].m_StepsQuestBot[MobID].m_Bot = QuestBotInfo::ms_aQuestBot[MobID];
		}
	}
}

// Initialization of NPC bots
void CBotCore::InitNPCBots(const char* pWhereLocalWorld)
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_bots_npc", pWhereLocalWorld);
	while(pRes->next())
	{
		const int MobID = pRes->getInt("ID");
		const int NumberOfNpc = pRes->getInt("Number");

		// load from database
		NpcBotInfo NpcBot;
		NpcBot.m_WorldID = pRes->getInt("WorldID");
		NpcBot.m_Static = pRes->getBoolean("Static");
		NpcBot.m_Position = vec2(pRes->getInt("PosX"), pRes->getInt("PosY") + (NpcBot.m_Static ? 1 : 0));
		NpcBot.m_Emote = pRes->getInt("Emote");
		NpcBot.m_BotID = pRes->getInt("BotID");
		NpcBot.m_Function = pRes->getInt("Function");
		NpcBot.m_GiveQuestID = pRes->getInt("GiveQuestID");
		if(NpcBot.m_GiveQuestID > 0)
			NpcBot.m_Function = FUNCTION_NPC_GIVE_QUEST;

		std::string DialogJsonStr = pRes->getString("DialogData").c_str();
		JsonTools::parseFromString(DialogJsonStr, [&](nlohmann::json& pJson)
		{
			CDialog Dialogue;
			for(auto& pItem : pJson)
			{
				const int Emote = GetIntegerEmoteValue("emote", pItem.value("emote", "").c_str());
				bool RequestAction = pItem.value("action_step", 0);

				CDialog Dialogue;
				Dialogue.Init(NpcBot.m_BotID, pItem.value("text", ""), Emote, RequestAction);
				NpcBot.m_aDialogs.push_back(Dialogue);
			}
		});

		// initilize
		NpcBotInfo::ms_aNpcBot[MobID] = NpcBot;

		// create bots
		for(int c = 0; c < NumberOfNpc; c++)
			GS()->CreateBot(TYPE_BOT_NPC, NpcBot.m_BotID, MobID);
	}
}

// Initialization of mobs bots
void CBotCore::InitMobsBots(const char* pWhereLocalWorld)
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_bots_mobs", pWhereLocalWorld);
	while(pRes->next())
	{
		const int MobID = pRes->getInt("ID");
		const int BotID = pRes->getInt("BotID");
		const int NumberOfMobs = pRes->getInt("Number");

		// load from database
		MobBotInfo MobBot;
		MobBot.m_WorldID = pRes->getInt("WorldID");
		MobBot.m_Position = vec2(pRes->getInt("PositionX"), pRes->getInt("PositionY"));
		MobBot.m_Power = pRes->getInt("Power");
		MobBot.m_Spread = pRes->getInt("Spread");
		MobBot.m_Boss = pRes->getBoolean("Boss");
		MobBot.m_Level = pRes->getInt("Level");
		MobBot.m_RespawnTick = pRes->getInt("Respawn");
		MobBot.m_BotID = BotID;

		str_copy(MobBot.m_aBehavior, pRes->getString("Behavior").c_str(), sizeof(MobBot.m_aBehavior));
		std::string BuffDebuff = pRes->getString("Effect").c_str();
		MobBot.InitBuffDebuff(4, 4, 3.0f, BuffDebuff);

		for(int i = 0; i < MAX_DROPPED_FROM_MOBS; i++)
		{
			char aBuf[32];
			str_format(aBuf, sizeof(aBuf), "it_drop_%d", i);
			MobBot.m_aDropItem[i] = pRes->getInt(aBuf);
		}
		sscanf(pRes->getString("it_drop_count").c_str(), "|%d|%d|%d|%d|%d|",
			&MobBot.m_aValueItem[0], &MobBot.m_aValueItem[1], &MobBot.m_aValueItem[2], &MobBot.m_aValueItem[3], &MobBot.m_aValueItem[4]);
		sscanf(pRes->getString("it_drop_chance").c_str(), "|%f|%f|%f|%f|%f|",
			&MobBot.m_aRandomItem[0], &MobBot.m_aRandomItem[1], &MobBot.m_aRandomItem[2], &MobBot.m_aRandomItem[3], &MobBot.m_aRandomItem[4]);

		// initilize
		MobBotInfo::ms_aMobBot[MobID] = MobBot;

		// create bots
		for(int c = 0; c < NumberOfMobs; c++)
			GS()->CreateBot(TYPE_BOT_MOB, BotID, MobID);
	}
}

// send a formatted message
void CBotCore::SendChatDialog(bool PlayerTalked, int BotType, int MobID, int ClientID, const char* pText)
{
	CNetMsg_Sv_Chat Msg;
	Msg.m_Team = -1;

	if(PlayerTalked)
	{
		Msg.m_ClientID = ClientID;	
	}
	else
	{	
		// check it's if there's a active bot
		int BotClientID = -1;
		for(int i = MAX_PLAYERS; i < MAX_CLIENTS; i++)
		{
			if(!GS()->m_apPlayers[i] || GS()->m_apPlayers[i]->GetBotType() != BotType || GS()->m_apPlayers[i]->GetBotMobID() != MobID)
				continue;
			BotClientID = i;
		}
		Msg.m_ClientID = BotClientID;
	}

	Msg.m_pMessage = pText;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void FormatDialog(char* aBuffer, int Size, const char* pTitle, std::pair < int, int > Page, const char* pTalked, const char* pDialogText)
{
	str_format(aBuffer, Size, "F4 (vote no) - continue dialog\n\n\n%s%s~ ( %d of %d ) %s:\n%s", pTitle, (pTitle[0] != '\0' ? "\n--------- \n\n" : ""), Page.first, Page.second, pTalked, pDialogText);
}

void CBotCore::DialogBotStepNPC(CPlayer* pPlayer, int MobID, int Progress, const char *pText)
{
	const int sizeDialogs = NpcBotInfo::ms_aNpcBot[MobID].m_aDialogs.size();
	if(!NpcBotInfo::IsNpcBotValid(MobID) || Progress >= sizeDialogs)
	{
		pPlayer->ClearTalking();
		return;
	}

	char reformTalkedText[512];
	const int BotID = NpcBotInfo::ms_aNpcBot[MobID].m_BotID;
	const int ClientID = pPlayer->GetCID();
	if(pText != nullptr)
	{
		pPlayer->FormatDialogText(BotID, pText);
		FormatDialog(reformTalkedText, sizeof(reformTalkedText), "\0", { 1, 1 }, NpcBotInfo::ms_aNpcBot[MobID].GetName(), pPlayer->GetDialogText());
		pPlayer->ClearDialogText();

		SendChatDialog(true, BotsTypes::TYPE_BOT_NPC, MobID, ClientID, pPlayer->GetDialogText());
		GS()->Motd(ClientID, reformTalkedText);
		return;
	}

	CDialog::VariantText* pVariant = NpcBotInfo::ms_aNpcBot[MobID].m_aDialogs[Progress].GetVariant();

	const char* TalkedNick;
	if(pVariant->m_Flag & TALKED_FLAG_SAYS_EIDOLON)
		TalkedNick = pPlayer->GetEidolon() ? DataBotInfo::ms_aDataBot[pPlayer->GetEidolon()->GetBotID()].m_aNameBot : "Eidolon";
	else if(pVariant->GetSaysName() == nullptr)
		TalkedNick = Server()->ClientName(ClientID);
	else
		TalkedNick = pVariant->GetSaysName();

	pPlayer->FormatDialogText(BotID, pVariant->m_Text.c_str());
	FormatDialog(reformTalkedText, sizeof(reformTalkedText), "\0", { (1 + Progress), sizeDialogs }, TalkedNick, pPlayer->GetDialogText());
	pPlayer->ClearDialogText();

	SendChatDialog(false, BotsTypes::TYPE_BOT_NPC, MobID, ClientID, pPlayer->GetDialogText());
	GS()->Motd(ClientID, reformTalkedText);
}

void CBotCore::DialogBotStepQuest(CPlayer* pPlayer, int MobID, int Progress, bool ExecutionStep)
{
	const int sizeDialogs = QuestBotInfo::ms_aQuestBot[MobID].m_aDialogs.size();
	if(!QuestBotInfo::IsQuestBotValid(MobID) || Progress >= sizeDialogs)
	{
		pPlayer->ClearTalking();
		return;
	}

	const int ClientID = pPlayer->GetCID();
	const int BotID = QuestBotInfo::ms_aQuestBot[MobID].m_BotID;
	CDialog::VariantText* pVariant = QuestBotInfo::ms_aQuestBot[MobID].m_aDialogs[Progress].GetVariant();

	const char* TalkedNick = "\0";
	const int QuestID = QuestBotInfo::ms_aQuestBot[MobID].m_QuestID;
	if(pVariant->m_Flag & TALKED_FLAG_SAYS_EIDOLON)
		TalkedNick = pPlayer->GetEidolon() ? DataBotInfo::ms_aDataBot[pPlayer->GetEidolon()->GetBotID()].m_aNameBot : "Eidolon";
	else if(pVariant->GetSaysName() == nullptr)
		TalkedNick = Server()->ClientName(ClientID);
	else
		TalkedNick = pVariant->GetSaysName();

	char aReformatText[512];
	pPlayer->FormatDialogText(BotID, pVariant->m_Text.c_str());
	FormatDialog(aReformatText, sizeof(aReformatText), GS()->GetQuestInfo(QuestID).GetName(), { (1 + Progress), sizeDialogs }, TalkedNick, pPlayer->GetDialogText());
	pPlayer->ClearDialogText();

	if(ExecutionStep)
		GS()->Mmo()->Quest()->QuestShowRequired(pPlayer, QuestBotInfo::ms_aQuestBot[MobID], aReformatText);
	else
		GS()->Motd(ClientID, aReformatText);
}

int CBotCore::GetQuestNPC(int MobID)
{
	if (!NpcBotInfo::IsNpcBotValid(MobID))
		return -1;

	return NpcBotInfo::ms_aNpcBot[MobID].m_GiveQuestID;
}


const char* CBotCore::GetMeaninglessDialog()
{
	const char* pTalking[3] =
	{
		"<player>, do you have any questions? I'm sorry, can't help you.",
		"What a beautiful <time>. I don't have anything for you <player>.",
		"<player> are you interested something? I'm sorry, don't want to talk right now."
	};
	return pTalking[random_int()%3];
}

bool CBotCore::ShowGuideDropByWorld(int WorldID, CPlayer* pPlayer)
{
	bool Found = false;
	const int ClientID = pPlayer->GetCID();
	const float ExtraChance = clamp((float)pPlayer->GetAttributeSize(AttributeIdentifier::LuckyDropItem, true) / 100.0f, 0.01f, 10.0f);
	
	char aBuf[128];
	for(const auto& [ID, MobData] : MobBotInfo::ms_aMobBot)
	{
		if (WorldID == MobData.m_WorldID)
		{
			bool HasDropItem = false;
			const int HideID = (NUM_TAB_MENU + ID);
			const vec2 Pos = MobData.m_Position / 32.0f;
			GS()->AVH(ClientID, HideID, "Mob {STR} [x{INT} y{INT}]", MobData.GetName(), (int)Pos.x, (int)Pos.y);

			for(int i = 0; i < MAX_DROPPED_FROM_MOBS; i++)
			{
				if(MobData.m_aDropItem[i] <= 0 || MobData.m_aValueItem[i] <= 0)
					continue;

				const float Chance = MobData.m_aRandomItem[i];
				CItemDescription* pDropItemInfo = GS()->GetItemInfo(MobData.m_aDropItem[i]);
				str_format(aBuf, sizeof(aBuf), "x%d - chance to loot %0.2f%%(+%0.2f%%)", MobData.m_aValueItem[i], Chance, ExtraChance);
				GS()->AVM(ClientID, "null", NOPE, HideID, "{STR}{STR}", pDropItemInfo->GetName(), aBuf);
				HasDropItem = true;
			}

			Found = true;

			if(!HasDropItem)
				GS()->AVM(ClientID, "null", NOPE, HideID, "The mob has no items!");
		}
	}
	return Found;
}

// add a new bot
void CBotCore::ConAddCharacterBot(int ClientID, const char* pCharacter)
{
	CPlayer* pPlayer = GS()->GetPlayer(ClientID);
	if(!pPlayer)
		return;

	nlohmann::json JsonTeeInfo;
	JsonTeeInfo["skin"] = pPlayer->Acc().m_TeeInfos.m_aSkinName;
	JsonTeeInfo["custom_color"] = pPlayer->Acc().m_TeeInfos.m_UseCustomColor;
	JsonTeeInfo["color_body"] = pPlayer->Acc().m_TeeInfos.m_ColorBody;
	JsonTeeInfo["color_feet"] = pPlayer->Acc().m_TeeInfos.m_ColorFeet;

	// check the nick
	CSqlString<16> cNick = CSqlString<16>(pCharacter);
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_bots_info", "WHERE Name = '%s'", cNick.cstr());
	if(pRes->next())
	{
		// if the nickname is not in the database
		const int ID = pRes->getInt("ID");
		Database->Execute<DB::UPDATE>("tw_bots_info", "JsonTeeInfo = '%s' WHERE ID = '%d'", JsonTeeInfo.dump().c_str(), ID);
		GS()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "parseskin", "Updated character bot!");
		return;
	}

	// add a new bot
	Database->Execute<DB::INSERT>("tw_bots_info", "(Name, JsonTeeInfo) VALUES ('%s', '%s')", cNick.cstr(), JsonTeeInfo.dump().c_str());
	GS()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "parseskin", "Added new character bot!");
}