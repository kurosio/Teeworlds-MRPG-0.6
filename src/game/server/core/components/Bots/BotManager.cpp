/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "BotManager.h"

#include <game/server/gamecontext.h>
#include <game/server/core/components/quests/quest_manager.h>

namespace
{
	// common collision mask used for validating spawn positions of bots.
	constexpr int BOT_SPAWN_INVALID_MASK = CCollision::COLFLAG_DEATH | CCollision::COLFLAG_SOLID | CCollision::COLFLAG_NOHOOK | CCollision::COLFLAG_WATER;
	constexpr const char* BOT_INVALID_POS_FMT = "{} bot: ID:{} invalid spawn position (death/solid/unhook).";

	/**
	 * Dialogue payload description (JSON schema):
	 *   "side":              "author" | "left" | "right"
	 *   "text":              string
	 *   "action":            bool
	 *   "left_speaker_id":   bot id | player | empty
	 *   "right_speaker_id":  bot id | player | empty
	 */
	struct DialogsBundle
	{
		bool HasAction{ false };
		std::vector<CDialogStep> Steps;
	};

	// creates a default fallback dialogue when the bot has no configured dialogues.
	CDialogStep MakeFallbackDialogue(int DataBotID)
	{
		nlohmann::json Json;
		Json["text"] = "<player>, do you have any questions? I'm sorry, can't help you.";
		Json["action"] = false;
		Json["side"] = "right";
		Json["left_speaker_id"] = DataBotID;
		Json["right_speaker_id"] = 0;

		CDialogStep Dialogue;
		Dialogue.Init(DataBotID, Json);
		return Dialogue;
	}

	DialogsBundle ParseDialogues(int DataBotID, const std::string& JsonData)
	{
		DialogsBundle Bundle;

		mystd::json::parse(JsonData, [&](nlohmann::json& Json)
			{
				Bundle.Steps.reserve(Json.size());
				for (auto& Item : Json)
				{
					CDialogStep Step;
					Step.Init(DataBotID, Item);

					if (Step.IsRequestAction())
						Bundle.HasAction = true;

					Bundle.Steps.emplace_back(std::move(Step));
				}
			});

		if (Bundle.Steps.empty())
			Bundle.Steps.emplace_back(MakeFallbackDialogue(DataBotID));

		return Bundle;
	}

	// parses "|v1|v2|v3|v4|v5|" style strings into a fixed-size array.
	template <typename T, size_t N>
	void ParsePipedValues(const std::string& Raw, const char* pFormat, T(&aOut)[N])
	{
		static_assert(N == 5, "Current drop schema expects exactly 5 slots");
		sscanf(Raw.c_str(), pFormat,
			&aOut[0], &aOut[1], &aOut[2], &aOut[3], &aOut[4]);
	}
}

// -----------------------------------------------------------------------------
// Bot data preloading (skins, equipment, modules)
// -----------------------------------------------------------------------------
void CBotManager::OnPreInit()
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_bots_info");
	while (pRes->next())
	{
		const int BotID = pRes->getInt("ID");

		DataBotInfo BotInfo{};
		str_copy(BotInfo.m_aNameBot, pRes->getString("Name").c_str(), sizeof(BotInfo.m_aNameBot));

		// equipment slots
		auto& Slots = BotInfo.m_vEquippedSlot;
		Slots[ItemType::EquipHammer] = pRes->getInt("SlotHammer");
		Slots[ItemType::EquipGun] = pRes->getInt("SlotGun");
		Slots[ItemType::EquipShotgun] = pRes->getInt("SlotShotgun");
		Slots[ItemType::EquipGrenade] = pRes->getInt("SlotGrenade");
		Slots[ItemType::EquipLaser] = pRes->getInt("SlotRifle");
		Slots[ItemType::EquipArmorTank] = pRes->getInt("SlotArmor");
		Slots[ItemType::EquipPickaxe] = 0;
		Slots[ItemType::EquipRake] = 0;
		Slots[ItemType::EquipEidolon] = 0;

		// modules
		std::string EquippedModules = pRes->getString("EquippedModules");
		if (!EquippedModules.empty())
			BotInfo.m_EquippedModules = std::move(EquippedModules);

		// tee appearance
		mystd::json::parse(pRes->getString("JsonTeeInfo"), [&](nlohmann::json& Json)
			{
				auto& Tee = BotInfo.m_TeeInfos;
				str_copy(Tee.m_aSkinName, Json.value("skin", "default").c_str(), sizeof(Tee.m_aSkinName));
				Tee.m_UseCustomColor = Json.value("custom_color", 0);
				Tee.m_ColorBody = Json.value("color_body", -1);
				Tee.m_ColorFeet = Json.value("color_feet", -1);
			});

		std::memset(BotInfo.m_aActiveByQuest, false, MAX_PLAYERS);
		DataBotInfo::ms_aDataBot[BotID] = std::move(BotInfo);
	}
}

void CBotManager::OnInitWorld(const std::string& SqlQueryWhereWorld)
{
	const char* pWhere = SqlQueryWhereWorld.c_str();
	InitQuestBots(pWhere);
	InitNPCBots(pWhere);
	InitMobsBots(pWhere);
}

// -----------------------------------------------------------------------------
// Quest bots
// -----------------------------------------------------------------------------
void CBotManager::InitQuestBots(const char* pWhereLocalWorld)
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_bots_quest", pWhereLocalWorld);
	while (pRes->next())
	{
		const int MobID = pRes->getInt("ID");
		const int BotID = pRes->getInt("BotID");
		const int QuestID = pRes->getInt("QuestID");
		dbg_assert(QuestID > 0, "Quest bot has no valid quest structure");

		const vec2 RawPos(pRes->getInt("PosX"), pRes->getInt("PosY") + 1);
		const vec2 VerifiedPos = GS()->Collision()->VerifyPoint(
			BOT_SPAWN_INVALID_MASK, RawPos, "Quest NPC (ID: {})", MobID);

		QuestBotInfo QuestBot;
		QuestBot.m_ID = MobID;
		QuestBot.m_QuestID = QuestID;
		QuestBot.m_BotID = BotID;
		QuestBot.m_StepPos = pRes->getInt("Step");
		QuestBot.m_Position = VerifiedPos;
		QuestBot.m_ScenarioJson = pRes->getString("ScenarioData");
		QuestBot.m_AutoFinish = (pRes->getString("AutoFinish") == "Partial");
		QuestBot.m_WorldID = pRes->getInt("WorldID");

		// tasks
		QuestBot.InitTasksFromJSON(GS()->Collision(), pRes->getString("TasksData"));

		// dialogues
		auto [HasAction, Steps] = ParseDialogues(BotID, pRes->getString("DialogData"));
		QuestBot.m_HasAction = HasAction;
		QuestBot.m_aDialogs = std::move(Steps);

		// register the objective in the quest
		auto* pQuestInfo = GS()->GetQuestInfo(QuestID);
		dbg_assert(pQuestInfo != nullptr, "QuestID is not valid");

		CQuestStepBase Base;
		Base.m_Bot = QuestBot;
		pQuestInfo->m_vObjectives[QuestBot.m_StepPos].push_back(std::move(Base));

		QuestBotInfo::ms_aQuestBot[MobID] = std::move(QuestBot);
	}
}

// -----------------------------------------------------------------------------
// NPC bots
// -----------------------------------------------------------------------------
void CBotManager::InitNPCBots(const char* pWhereLocalWorld)
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_bots_npc", pWhereLocalWorld);
	while (pRes->next())
	{
		const int  MobID = pRes->getInt("ID");
		const int  BotID = pRes->getInt("BotID");
		const bool Static = pRes->getBoolean("Static");
		const int  QuestID = pRes->getInt("GiveQuestID");

		const vec2 RawPos(pRes->getInt("PosX"), pRes->getInt("PosY"));
		const vec2 VerifiedPos = GS()->Collision()->VerifyPoint(
			BOT_SPAWN_INVALID_MASK, RawPos, "Default NPC (ID: {})", MobID);

		NpcBotInfo NpcBot;
		NpcBot.m_WorldID = pRes->getInt("WorldID");
		NpcBot.m_Static = Static;
		NpcBot.m_Position = VerifiedPos + vec2(0.f, Static ? 1.f : 0.f);
		NpcBot.m_Emote = pRes->getInt("Emote");
		NpcBot.m_BotID = BotID;
		NpcBot.m_Function = pRes->getInt("Function");
		NpcBot.m_GiveQuestID = QuestID;

		// auto-mark as quest giver
		if (QuestID > 0)
		{
			auto* pQuestInfo = GS()->GetQuestInfo(QuestID);
			dbg_assert(pQuestInfo != nullptr, "QuestID is not valid");
			pQuestInfo->AddFlag(QUEST_FLAG_GRANTED_FROM_NPC);
			NpcBot.m_Function = FUNCTION_NPC_GIVE_QUEST;
		}

		// dialogues (NPCs do not use the HasAction flag)
		NpcBot.m_aDialogs = ParseDialogues(BotID, pRes->getString("DialogData")).Steps;

		NpcBotInfo::ms_aNpcBot[MobID] = std::move(NpcBot);
		GS()->CreateBot(TYPE_BOT_NPC, BotID, MobID);
	}
}

// -----------------------------------------------------------------------------
// Mob bots
// -----------------------------------------------------------------------------
void CBotManager::InitMobsBots(const char* pWhereLocalWorld)
{
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_bots_mobs", pWhereLocalWorld);
	while (pRes->next())
	{
		const int  MobID = pRes->getInt("ID");
		const int  BotID = pRes->getInt("BotID");
		const int  NumberOfMobs = pRes->getInt("Number");

		MobBotInfo MobBot;
		MobBot.m_BotID = BotID;
		MobBot.m_Position = vec2(pRes->getInt("PositionX"), pRes->getInt("PositionY"));
		MobBot.m_Power = pRes->getInt("Power");
		MobBot.m_Boss = pRes->getBoolean("Boss");
		MobBot.m_Level = pRes->getInt("Level");
		MobBot.m_RespawnTick = pRes->getInt("Respawn");
		MobBot.m_Radius = static_cast<float>(pRes->getInt("Radius"));
		MobBot.m_WorldID = pRes->getInt("WorldID");

		// use configured active radius, or fall back to the map default when unset.
		const float RawActiveRadius = static_cast<float>(pRes->getInt("ActiveRadius"));
		MobBot.m_ActiveRadius = RawActiveRadius > 1.f
			? RawActiveRadius
			: static_cast<float>(g_Config.m_SvMapDistanceActveBot);

		// behavior sets
		MobBot.InitBehaviors(DBSet(pRes->getString("Behavior")));
		MobBot.InitDebuffs(5, 5, 5.0f, DBSet(pRes->getString("Debuffs")));

		// drop items
		for (int i = 0; i < MAX_DROPPED_FROM_MOBS; ++i)
		{
			char aColumn[32];
			str_format(aColumn, sizeof(aColumn), "it_drop_%d", i);
			MobBot.m_aDropItem[i] = pRes->getInt(aColumn);
		}
		ParsePipedValues(pRes->getString("it_drop_count"), "|%d|%d|%d|%d|%d|", MobBot.m_aValueItem);
		ParsePipedValues(pRes->getString("it_drop_chance"), "|%f|%f|%f|%f|%f|", MobBot.m_aRandomItem);

		MobBotInfo::ms_aMobBot[MobID] = MobBot;

		// spawn actual bot instances
		for (int i = 0; i < NumberOfMobs; ++i)
		{
			if (auto* pPlayerBot = GS()->CreateBot(TYPE_BOT_MOB, BotID, MobID))
				pPlayerBot->InitBotMobInfo(MobBot);
		}
	}
}

// -----------------------------------------------------------------------------
// Public helpers
// -----------------------------------------------------------------------------
int CBotManager::GetQuestNPC(int MobID)
{
	if (!NpcBotInfo::IsValid(MobID))
		return -1;

	return NpcBotInfo::ms_aNpcBot[MobID].m_GiveQuestID;
}

// -----------------------------------------------------------------------------
// Console: add / update a character bot from the given player's appearance
// -----------------------------------------------------------------------------
void CBotManager::ConAddCharacterBot(int ClientID, const char* pCharacter)
{
	CPlayer* pPlayer = GS()->GetPlayer(ClientID);
	if (!pPlayer)
		return;

	const auto& Tee = pPlayer->Account()->m_TeeInfos;

	nlohmann::json JsonTeeInfo;
	JsonTeeInfo["skin"] = Tee.m_aSkinName;
	JsonTeeInfo["custom_color"] = Tee.m_UseCustomColor;
	JsonTeeInfo["color_body"] = Tee.m_ColorBody;
	JsonTeeInfo["color_feet"] = Tee.m_ColorFeet;

	const std::string JsonDump = JsonTeeInfo.dump();
	const CSqlString<16> Nick(pCharacter);

	// if a bot with this name already exists, refresh its appearance.
	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_bots_info", "WHERE Name = '{}'", Nick.cstr());
	if (pRes->next())
	{
		const int ID = pRes->getInt("ID");
		Database->Execute<DB::UPDATE>("tw_bots_info",
			"JsonTeeInfo = '{}' WHERE ID = '{}'", JsonDump.c_str(), ID);
		GS()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "parseskin", "Updated character bot!");
		return;
	}

	// otherwise create a new record.
	Database->Execute<DB::INSERT>("tw_bots_info",
		"(Name, JsonTeeInfo) VALUES ('{}', '{}')", Nick.cstr(), JsonDump.c_str());
	GS()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "parseskin", "Added new character bot!");
}