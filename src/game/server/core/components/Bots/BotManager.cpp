/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "BotManager.h"

#include <game/server/gamecontext.h>

#include <game/server/core/components/quests/quest_manager.h>

// structure
/* "side": (author, left, right)
 * "text": (some text)
 * "action": (true, false)
 * "left_speaker_id: (13 - bot id, player, empty)
 * "right_speaker_id: (14 - bot id, player, empty)
 */
// dialogue initilizer
typedef std::pair < bool, std::vector<CDialogStep> > DialogsInitilizerType;
static DialogsInitilizerType DialogsInitilizer(int DataBotID, const std::string& JsonDialogData)
{
	DialogsInitilizerType Value{false, {}};
	mystd::json::parse(JsonDialogData, [&](nlohmann::json& pJson)
	{
		for(auto& pItem : pJson)
		{
			CDialogStep Dialogue;
			Dialogue.Init(DataBotID, pItem);
			if(Dialogue.IsRequestAction())
				Value.first = true;
			Value.second.push_back(Dialogue);
		}
	});
	// useless dialogue
	if(Value.second.empty())
	{
		CDialogStep Dialogue;

		// json sturcture
		nlohmann::json JsonDialog;
		JsonDialog["text"] = "<player>, do you have any questions? I'm sorry, can't help you.";
		JsonDialog["action"] = false;
		JsonDialog["side"] = "right";
		JsonDialog["left_speaker_id"] = DataBotID;
		JsonDialog["right_speaker_id"] = 0;

		// initialize dialog
		Dialogue.Init(DataBotID, JsonDialog);
		Value.second.push_back(Dialogue);
	}
	return Value;
}

void CBotManager::OnPreInit()
{
	// init bot datas
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_bots_info");
	while(pRes->next())
	{
		const int BotID = pRes->getInt("ID");

		DataBotInfo BotInfo;
		str_copy(BotInfo.m_aNameBot, pRes->getString("Name").c_str(), sizeof(BotInfo.m_aNameBot));
		BotInfo.m_vEquippedSlot[ItemType::EquipHammer] = pRes->getInt("SlotHammer");
		BotInfo.m_vEquippedSlot[ItemType::EquipGun] = pRes->getInt("SlotGun");
		BotInfo.m_vEquippedSlot[ItemType::EquipShotgun] = pRes->getInt("SlotShotgun");
		BotInfo.m_vEquippedSlot[ItemType::EquipGrenade] = pRes->getInt("SlotGrenade");
		BotInfo.m_vEquippedSlot[ItemType::EquipLaser] = pRes->getInt("SlotRifle");
		BotInfo.m_vEquippedSlot[ItemType::EquipArmorTank] = pRes->getInt("SlotArmor");
		BotInfo.m_vEquippedSlot[ItemType::EquipPickaxe] = 0;
		BotInfo.m_vEquippedSlot[ItemType::EquipRake] = 0;
		BotInfo.m_vEquippedSlot[ItemType::EquipEidolon] = 0;

		std::string EquippedModules = pRes->getString("EquippedModules").c_str();
		if(!EquippedModules.empty())
			BotInfo.m_EquippedModules = EquippedModules;

		// load teeinfo
		std::string JsonString = pRes->getString("JsonTeeInfo").c_str();
		mystd::json::parse(JsonString, [&](nlohmann::json& pJson)
		{
			str_copy(BotInfo.m_TeeInfos.m_aSkinName, pJson.value("skin", "default").c_str(), sizeof(BotInfo.m_TeeInfos.m_aSkinName));
			BotInfo.m_TeeInfos.m_UseCustomColor = pJson.value("custom_color", 0);
			BotInfo.m_TeeInfos.m_ColorBody = pJson.value("color_body", -1);
			BotInfo.m_TeeInfos.m_ColorFeet = pJson.value("color_feet", -1);
		});

		memset(BotInfo.m_aActiveByQuest, false, MAX_PLAYERS);
		DataBotInfo::ms_aDataBot[BotID] = BotInfo;
	}
}

void CBotManager::OnInitWorld(const std::string& SqlQueryWhereWorld)
{
	InitQuestBots(SqlQueryWhereWorld.c_str());
	InitNPCBots(SqlQueryWhereWorld.c_str());
	InitMobsBots(SqlQueryWhereWorld.c_str());
}

bool CBotManager::OnClientMessage(int MsgID, void* pRawMsg, int ClientID)
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
		const auto BotID = pRes->getInt("BotID");
		const int QuestID = pRes->getInt("QuestID");
		const auto AutoFinsihMode = pRes->getString("AutoFinish") == "Partial";
		const auto Step = pRes->getInt("Step");
		const auto Pos = vec2(pRes->getInt("PosX"), pRes->getInt("PosY") + 1);
		const auto VerifyPos = GS()->Collision()->VerifyPoint(CCollision::COLFLAG_DEATH | CCollision::COLFLAG_SOLID,
			Pos, "QuestNPC: Mob(ID:{}) - invalid (death, solid) position.", MobID);
		const auto ScenarioData = pRes->getString("ScenarioData");
		const auto TaskData = pRes->getString("TasksData");
		const auto DialogData = pRes->getString("DialogData");
		const auto WorldID = pRes->getInt("WorldID");
		dbg_assert(QuestID > 0, "Some quest bot's does not have quest structure");

		// load from database
		QuestBotInfo QuestBot;
		QuestBot.m_ID = MobID;
		QuestBot.m_QuestID = QuestID;
		QuestBot.m_BotID = BotID;
		QuestBot.m_StepPos = Step;
		QuestBot.m_Position = VerifyPos;
		QuestBot.m_ScenarioJson = ScenarioData;
		QuestBot.m_AutoFinish = AutoFinsihMode;
		QuestBot.m_WorldID = WorldID;

		// tasks initilized
		QuestBot.InitTasksFromJSON(GS()->Collision(), TaskData);

		// dialog initilizer
		auto [hasAction, vDialogs] = DialogsInitilizer(QuestBot.m_BotID, DialogData);
		QuestBot.m_HasAction = hasAction;
		QuestBot.m_aDialogs = vDialogs;

		// initialize quest steps
		CQuestStepBase Base;
		Base.m_Bot = QuestBot;
		dbg_assert(GS()->GetQuestInfo(QuestID) != nullptr, "QuestID is not valid");
		auto* pQuestInfo = GS()->GetQuestInfo(QuestID);
		pQuestInfo->m_vObjectives[QuestBot.m_StepPos].push_back(Base);

		// initilize
		QuestBotInfo::ms_aQuestBot[MobID] = std::move(QuestBot);
	}
}

void CBotManager::InitNPCBots(const char* pWhereLocalWorld)
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_bots_npc", pWhereLocalWorld);
	while(pRes->next())
	{
		const int MobID = pRes->getInt("ID");
		const auto BotID = pRes->getInt("BotID");
		const auto Static = pRes->getBoolean("Static");
		const auto Pos = vec2(pRes->getInt("PosX"), pRes->getInt("PosY"));
		const auto VerifyPos = GS()->Collision()->VerifyPoint(CCollision::COLFLAG_DEATH | CCollision::COLFLAG_SOLID,
			Pos, "NPC: Mob(ID:{}) - invalid (death, solid) position.", MobID);
		const auto WorldID = pRes->getInt("WorldID");
		const auto Emote = pRes->getInt("Emote");
		const auto Function = pRes->getInt("Function");
		const auto Quest = pRes->getInt("GiveQuestID");

		// load from database
		NpcBotInfo NpcBot;
		NpcBot.m_WorldID = WorldID;
		NpcBot.m_Static = Static;
		NpcBot.m_Position = VerifyPos + vec2(0.f, Static ? 1.f : 0.f);
		NpcBot.m_Emote = Emote;
		NpcBot.m_BotID = BotID;
		NpcBot.m_Function = Function;
		NpcBot.m_GiveQuestID = Quest;

		if(Quest > 0)
		{
			dbg_assert(GS()->GetQuestInfo(NpcBot.m_GiveQuestID) != nullptr, "QuestID is not valid");
			auto* pQuestInfo = GS()->GetQuestInfo(NpcBot.m_GiveQuestID);
			pQuestInfo->AddFlag(QUEST_FLAG_GRANTED_FROM_NPC);
			NpcBot.m_Function = FUNCTION_NPC_GIVE_QUEST;
		}

		// dialog initilizer
		const auto DialogData = pRes->getString("DialogData");
		NpcBot.m_aDialogs = DialogsInitilizer(NpcBot.m_BotID, DialogData).second;

		// initilize
		NpcBotInfo::ms_aNpcBot[MobID] = NpcBot;
		GS()->CreateBot(TYPE_BOT_NPC, NpcBot.m_BotID, MobID);
	}
}

void CBotManager::InitMobsBots(const char* pWhereLocalWorld)
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_bots_mobs", pWhereLocalWorld);
	while(pRes->next())
	{
		// initialize variables
		const auto MobID = pRes->getInt("ID");
		const auto BotID = pRes->getInt("BotID");
		const auto NumberOfMobs = pRes->getInt("Number");
		const auto Position = vec2(pRes->getInt("PositionX"), pRes->getInt("PositionY"));
		const auto Power = pRes->getInt("Power");
		const auto IsBoss = pRes->getBoolean("Boss");
		const auto Level = pRes->getInt("Level");
		const auto RespawnTick = pRes->getInt("Respawn");
		const auto Radius = (float)pRes->getInt("Radius");
		const auto Behavior = pRes->getString("Behavior");
		const auto WorldID = pRes->getInt("WorldID");

		// create new structure
		MobBotInfo MobBot;
		MobBot.m_BotID = BotID;
		MobBot.m_Position = Position;
		MobBot.m_Power = Power;
		MobBot.m_Boss = IsBoss;
		MobBot.m_Level = Level;
		MobBot.m_RespawnTick = RespawnTick;
		MobBot.m_Radius = Radius;
		MobBot.m_WorldID = WorldID;

		// initialize behaviors
		auto BehaviorSet = DBSet(Behavior);
		MobBot.InitBehaviors(BehaviorSet);

		// initialize debuffs
		auto DebuffSet = DBSet(pRes->getString("Debuffs"));
		MobBot.InitDebuffs(5, 5, 5.0f, DebuffSet);

		// initialize drop items
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
		{
			if(auto* pPlayerBot = GS()->CreateBot(TYPE_BOT_MOB, BotID, MobID))
				pPlayerBot->InitBotMobInfo(MobBot);
		}
	}
}

int CBotManager::GetQuestNPC(int MobID)
{
	if (!NpcBotInfo::IsValid(MobID))
		return -1;

	return NpcBotInfo::ms_aNpcBot[MobID].m_GiveQuestID;
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
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_bots_info", "WHERE Name = '{}'", cNick.cstr());
	if(pRes->next())
	{
		// if the nickname is not in the database
		const int ID = pRes->getInt("ID");
		Database->Execute<DB::UPDATE>("tw_bots_info", "JsonTeeInfo = '{}' WHERE ID = '{}'", JsonTeeInfo.dump().c_str(), ID);
		GS()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "parseskin", "Updated character bot!");
		return;
	}

	// add a new bot
	Database->Execute<DB::INSERT>("tw_bots_info", "(Name, JsonTeeInfo) VALUES ('{}', '{}')", cNick.cstr(), JsonTeeInfo.dump().c_str());
	GS()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "parseskin", "Added new character bot!");
}