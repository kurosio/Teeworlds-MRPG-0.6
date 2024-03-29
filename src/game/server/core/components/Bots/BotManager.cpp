/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "BotManager.h"

#include <game/server/gamecontext.h>

#include <game/server/core/components/Quests/QuestManager.h>

// dialogue initilizer
typedef std::pair < bool, std::vector<CDialogElem> > DialogsInitilizerType;
static DialogsInitilizerType DialogsInitilizer(int DataBotID, const std::string& JsonDialogData)
{
	DialogsInitilizerType Value{false, {}};
	Tools::Json::parseFromString(JsonDialogData, [&](nlohmann::json& pJson)
	{
		for(auto& pItem : pJson)
		{
			bool Action = pItem.value("action_step", 0);
			if(!Value.first && Action)
				Value.first = true;

			CDialogElem Dialogue;
			Dialogue.Init(DataBotID, pItem.value("text", ""), Action);
			Value.second.push_back(Dialogue);
		}
	});

	// useless dialogue
	if(Value.second.empty())
	{
		CDialogElem Dialogue;
		Dialogue.Init(DataBotID, "<player>, do you have any questions? I'm sorry, can't help you.", false);
		Value.second.push_back(Dialogue);
	}

	return Value;
}

void CBotManager::OnInit()
{
	// init bot datas
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

		std::string EquippedModules = pRes->getString("EquippedModules").c_str();
		if(!EquippedModules.empty())
			BotInfo.m_EquippedModules = EquippedModules;

		// load teeinfo
		std::string JsonString = pRes->getString("JsonTeeInfo").c_str();
		Tools::Json::parseFromString(JsonString, [&](nlohmann::json& pJson)
		{
			str_copy(BotInfo.m_TeeInfos.m_aSkinName, pJson.value("skin", "default").c_str(), sizeof(BotInfo.m_TeeInfos.m_aSkinName));
			BotInfo.m_TeeInfos.m_UseCustomColor = pJson.value("custom_color", 0);
			BotInfo.m_TeeInfos.m_ColorBody = pJson.value("color_body", -1);
			BotInfo.m_TeeInfos.m_ColorFeet = pJson.value("color_feet", -1);
		});

		memset(BotInfo.m_aVisibleActive, false, MAX_PLAYERS);
		DataBotInfo::ms_aDataBot[BotID] = BotInfo;
	}
}

void CBotManager::OnInitWorld(const char* pWhereLocalWorld)
{
	InitQuestBots(pWhereLocalWorld);
	InitNPCBots(pWhereLocalWorld);
	InitMobsBots(pWhereLocalWorld);
}

bool CBotManager::OnMessage(int MsgID, void* pRawMsg, int ClientID)
{
	return false;
}

// Initialization of Quest bots
void CBotManager::InitQuestBots(const char* pWhereLocalWorld)
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_bots_quest", pWhereLocalWorld);
	while(pRes->next())
	{
		const int MobID = pRes->getInt("ID");
		const int QuestID = pRes->getInt("QuestID");
		dbg_assert(QuestID > 0, "Some quest bot's does not have quest structure");

		// load from database
		QuestBotInfo QuestBot;
		QuestBot.m_ID = MobID;
		QuestBot.m_QuestID = QuestID;
		QuestBot.m_BotID = pRes->getInt("BotID");
		QuestBot.m_StepPos = pRes->getInt("Step");
		QuestBot.m_WorldID = pRes->getInt("WorldID");
		QuestBot.m_Position = vec2(pRes->getInt("PosX"), pRes->getInt("PosY") + 1);
		QuestBot.m_EventJsonData = pRes->getString("EventData").c_str();

		// tasks initilized
		std::string TasksData = pRes->getString("TasksData").c_str();
		QuestBot.InitTasksFromJSON(TasksData);

		// dialog initilizer
		std::string DialogJsonStr = pRes->getString("DialogData").c_str();
		auto [lAction, aDialogs] = DialogsInitilizer(QuestBot.m_BotID, DialogJsonStr);
		QuestBot.m_HasAction = lAction;
		QuestBot.m_aDialogs = aDialogs;

		// initilize quest steps
		CQuestStepBase Base;
		Base.m_Bot = QuestBot;
		GS()->GetQuestInfo(QuestID)->m_vSteps[QuestBot.m_StepPos].push_back(Base);

		// initilize
		QuestBotInfo::ms_aQuestBot[MobID] = std::move(QuestBot);
		dbg_assert(GS()->GetQuestInfo(QuestID) != nullptr, "QuestID is not valid");
	}
}

// Initialization of NPC bots
void CBotManager::InitNPCBots(const char* pWhereLocalWorld)
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_bots_npc", pWhereLocalWorld);
	while(pRes->next())
	{
		const int MobID = pRes->getInt("ID");

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
		{
			NpcBot.m_Function = FUNCTION_NPC_GIVE_QUEST;
		}

		// dialog initilizer
		std::string DialogJsonStr = pRes->getString("DialogData").c_str();
		NpcBot.m_aDialogs = DialogsInitilizer(NpcBot.m_BotID, DialogJsonStr).second;

		// initilize
		NpcBotInfo::ms_aNpcBot[MobID] = NpcBot;
		GS()->CreateBot(TYPE_BOT_NPC, NpcBot.m_BotID, MobID);
	}
}

// Initialization of Mobs bots
void CBotManager::InitMobsBots(const char* pWhereLocalWorld)
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
		MobBot.m_BehaviorSets = pRes->getString("Behavior").c_str();
		std::string BuffDebuff = pRes->getString("Effect").c_str();
		MobBot.InitDebuffs(4, 4, 3.0f, BuffDebuff);

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

int CBotManager::GetQuestNPC(int MobID)
{
	if (!NpcBotInfo::IsValid(MobID))
		return -1;

	return NpcBotInfo::ms_aNpcBot[MobID].m_GiveQuestID;
}

bool CBotManager::InsertItemsDetailVotes(CPlayer* pPlayer, int WorldID) const
{
	bool Found = false;
	const int ClientID = pPlayer->GetCID();
	const float ExtraChance = clamp(static_cast<float>(pPlayer->GetAttributeSize(AttributeIdentifier::LuckyDropItem)) / 100.0f, 0.01f, 10.0f);

	for(const auto& [ID, Mob] : MobBotInfo::ms_aMobBot)
	{
		if(WorldID != Mob.m_WorldID)
			continue;

		const vec2 Pos = Mob.m_Position / 32.0f;
		CVoteWrapper VMob(ClientID, VWF_UNIQUE | VWF_STYLE_SIMPLE, "Mob {STR}", Mob.GetName());
		VMob.MarkList().Add("Location:");
		{
			VMob.BeginDepth();
			VMob.Add(Instance::Localize(ClientID, Instance::Server()->GetWorldName(WorldID)));
			VMob.Add("x{INT} y{INT}", (int)Pos.x, (int)Pos.y);
			VMob.EndDepth();
		}
		VMob.AddLine();
		VMob.MarkList().Add("Description:");
		{
			VMob.BeginDepth();
			VMob.Add("Level: {INT}", Mob.m_Level);
			VMob.Add("Power: {INT}", Mob.m_Power);
			VMob.Add("Boss: {STR}", Mob.m_Boss ? "Yes" : "No");
			VMob.Add("Respawn: {INT} sec", Mob.m_RespawnTick);
			VMob.EndDepth();
		}
		VMob.AddLine();
		VMob.MarkList().Add("Dropped:");
		{
			VMob.BeginDepth();
			bool HasDropItem = false;
			for(int i = 0; i < MAX_DROPPED_FROM_MOBS; i++)
			{
				if(Mob.m_aDropItem[i] <= 0 || Mob.m_aValueItem[i] <= 0)
					continue;

				const float Chance = Mob.m_aRandomItem[i];
				CItemDescription* pDropItemInfo = GS()->GetItemInfo(Mob.m_aDropItem[i]);

				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), "x%d - chance to loot %0.2f%%(+%0.2f%%)", Mob.m_aValueItem[i], Chance, ExtraChance);
				VMob.Add("{STR}{STR}", pDropItemInfo->GetName(), aBuf);
				HasDropItem = true;
			}
			if(!HasDropItem)
			{
				VMob.Add("The mob has no items!");
			}
			VMob.EndDepth();
		}
		VMob.AddLine();
		Found = true;
	}

	return Found;
}

// add a new bot
void CBotManager::ConAddCharacterBot(int ClientID, const char* pCharacter)
{
	CPlayer* pPlayer = GS()->GetPlayer(ClientID);
	if(!pPlayer)
		return;

	nlohmann::json JsonTeeInfo;
	JsonTeeInfo["skin"] = pPlayer->Account()->m_TeeInfos.m_aSkinName;
	JsonTeeInfo["custom_color"] = pPlayer->Account()->m_TeeInfos.m_UseCustomColor;
	JsonTeeInfo["color_body"] = pPlayer->Account()->m_TeeInfos.m_ColorBody;
	JsonTeeInfo["color_feet"] = pPlayer->Account()->m_TeeInfos.m_ColorFeet;

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