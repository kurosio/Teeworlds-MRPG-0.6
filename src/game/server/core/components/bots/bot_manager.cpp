/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "bot_manager.h"
#include "bot_db.h"

#include <game/server/gamecontext.h>
#include <game/server/core/components/quests/quest_manager.h>
#include <game/server/core/tools/db_async_context.h>

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
	void ParsePipedValues(const std::string& Raw, T(&aOut)[N])
	{
		static_assert(N == 5, "Current drop schema expects exactly 5 slots");
		if (Raw.empty())
			return;

		if constexpr (std::is_same_v<T, int>)
		{
			[[maybe_unused]] const int Read = std::sscanf(Raw.c_str(), "|%d|%d|%d|%d|%d|",
				&aOut[0], &aOut[1], &aOut[2], &aOut[3], &aOut[4]);
		}
		else if constexpr (std::is_same_v<T, float>)
		{
			[[maybe_unused]] const int Read = std::sscanf(Raw.c_str(), "|%f|%f|%f|%f|%f|",
				&aOut[0], &aOut[1], &aOut[2], &aOut[3], &aOut[4]);
		}
	}
}

// -----------------------------------------------------------------------------
// Bot data preloading (skins, equipment, modules)
// -----------------------------------------------------------------------------
void CBotManager::OnPreInit()
{
	auto Bots = BotDb::ListInfo();
	while (Bots.Next())
	{
		const int BotID = Bots.GetInt("ID");

		DataBotInfo BotInfo{};
		str_copy(BotInfo.m_aNameBot, Bots.GetString("Name").c_str(), sizeof(BotInfo.m_aNameBot));

		// equipment slots
		auto& Slots = BotInfo.m_vEquippedSlot;
		Slots[ItemType::EquipHammer] = Bots.GetInt("SlotHammer");
		Slots[ItemType::EquipGun] = Bots.GetInt("SlotGun");
		Slots[ItemType::EquipShotgun] = Bots.GetInt("SlotShotgun");
		Slots[ItemType::EquipGrenade] = Bots.GetInt("SlotGrenade");
		Slots[ItemType::EquipLaser] = Bots.GetInt("SlotRifle");
		Slots[ItemType::EquipArmorTank] = Bots.GetInt("SlotArmor");
		Slots[ItemType::EquipPickaxe] = 0;
		Slots[ItemType::EquipRake] = 0;
		Slots[ItemType::EquipEidolon] = 0;

		// modules
		std::string EquippedModules = Bots.GetString("EquippedModules");
		if (!EquippedModules.empty())
			BotInfo.m_EquippedModules = std::move(EquippedModules);

		// tee appearance
		mystd::json::parse(Bots.GetString("JsonTeeInfo"), [&](nlohmann::json& Json)
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

void CBotManager::OnInitWorld(int WorldID)
{
	InitQuestBots(WorldID);
	InitNPCBots(WorldID);
	InitMobsBots(WorldID);
}

// -----------------------------------------------------------------------------
// Quest bots
// -----------------------------------------------------------------------------
void CBotManager::InitQuestBots(int WorldID)
{
	auto QuestBots = BotDb::ListQuestBots(WorldID);
	while (QuestBots.Next())
	{
		const int MobID = QuestBots.GetInt("ID");
		const int BotID = QuestBots.GetInt("BotID");
		const int QuestID = QuestBots.GetInt("QuestID");
		dbg_assert(QuestID > 0, "Quest bot has no valid quest structure");

		const vec2 RawPos(QuestBots.GetInt("PosX"), QuestBots.GetInt("PosY") + 1);
		const vec2 VerifiedPos = GS()->Collision()->VerifyPoint(
			BOT_SPAWN_INVALID_MASK, RawPos, "Quest NPC (ID: {})", MobID);

		QuestBotInfo QuestBot;
		QuestBot.m_ID = MobID;
		QuestBot.m_QuestID = QuestID;
		QuestBot.m_BotID = BotID;
		QuestBot.m_StepPos = QuestBots.GetInt("Step");
		QuestBot.m_Position = VerifiedPos;
		QuestBot.m_ScenarioJson = QuestBots.GetString("ScenarioData");
		QuestBot.m_AutoFinish = (QuestBots.GetString("AutoFinish") == "Partial");
		QuestBot.m_WorldID = QuestBots.GetInt("WorldID");

		// tasks
		QuestBot.InitTasksFromJSON(GS()->Collision(), QuestBots.GetString("TasksData"));

		// dialogues
		auto [HasAction, Steps] = ParseDialogues(BotID, QuestBots.GetString("DialogData"));
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
void CBotManager::InitNPCBots(int WorldID)
{
	auto NpcBots = BotDb::ListNpcBots(WorldID);
	while (NpcBots.Next())
	{
		const int  MobID = NpcBots.GetInt("ID");
		const int  BotID = NpcBots.GetInt("BotID");
		const bool Static = NpcBots.GetBoolean("Static");
		const int  QuestID = NpcBots.GetInt("GiveQuestID");

		const vec2 RawPos(NpcBots.GetInt("PosX"), NpcBots.GetInt("PosY"));
		const vec2 VerifiedPos = GS()->Collision()->VerifyPoint(
			BOT_SPAWN_INVALID_MASK, RawPos, "Default NPC (ID: {})", MobID);

		NpcBotInfo NpcBot;
		NpcBot.m_WorldID = NpcBots.GetInt("WorldID");
		NpcBot.m_Static = Static;
		NpcBot.m_Position = VerifiedPos + vec2(0.f, Static ? 1.f : 0.f);
		NpcBot.m_Emote = NpcBots.GetInt("Emote");
		NpcBot.m_BotID = BotID;
		NpcBot.m_Function = NpcBots.GetInt("Function");
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
		NpcBot.m_aDialogs = ParseDialogues(BotID, NpcBots.GetString("DialogData")).Steps;

		NpcBotInfo::ms_aNpcBot[MobID] = std::move(NpcBot);
		GS()->CreateBot(TYPE_BOT_NPC, BotID, MobID);
	}
}

// -----------------------------------------------------------------------------
// Mob bots
// -----------------------------------------------------------------------------
void CBotManager::InitMobsBots(int WorldID)
{
	auto MobBots = BotDb::ListMobBots(WorldID);
	while (MobBots.Next())
	{
		const int  MobID = MobBots.GetInt("ID");
		const int  BotID = MobBots.GetInt("BotID");
		const int  NumberOfMobs = MobBots.GetInt("Number");

		MobBotInfo MobBot;
		MobBot.m_BotID = BotID;
		MobBot.m_Position = vec2(MobBots.GetInt("PositionX"), MobBots.GetInt("PositionY"));
		MobBot.m_Power = MobBots.GetInt("Power");
		MobBot.m_Boss = MobBots.GetBoolean("Boss");
		MobBot.m_Level = MobBots.GetInt("Level");
		MobBot.m_RespawnTick = MobBots.GetInt("Respawn");
			MobBot.m_Radius = static_cast<float>(MobBots.GetInt("Radius"));
			MobBot.m_WorldID = MobBots.GetInt("WorldID");

		// active radius — radius of the field of view in which the mob is active.
		const float RawActiveRadius = static_cast<float>(MobBots.GetInt("ActiveRadius"));
		MobBot.m_ActiveRadius = RawActiveRadius > 1.f
			? RawActiveRadius
			: static_cast<float>(g_Config.m_SvMapDistanceActveBot);

	// wander radius — max distance from the spawn point; crossing it teleports
	// the mob back. 0 — the limit is off. It is independent of the active radius.
	const float RawWanderRadius = static_cast<float>(MobBots.GetInt("WanderRadius"));
	MobBot.m_WanderRadius = RawWanderRadius > 0.f ? RawWanderRadius : 0.f;

		// behavior sets
		MobBot.InitBehaviors(DBSet(MobBots.GetString("Behavior")));
		MobBot.InitDebuffs(5, 5, 5.0f, DBSet(MobBots.GetString("Debuffs")));

		// drop items
		for (int i = 0; i < MAX_DROPPED_FROM_MOBS; ++i)
		{
			char aColumn[32];
			str_format(aColumn, sizeof(aColumn), "it_drop_%d", i);
			MobBot.m_aDropItem[i] = MobBots.GetInt(aColumn);
		}
		
		ParsePipedValues(MobBots.GetString("it_drop_count"), MobBot.m_aValueItem);
		ParsePipedValues(MobBots.GetString("it_drop_chance"), MobBot.m_aRandomItem);

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

	// If a bot with this name already exists its appearance is refreshed, otherwise
	// a new record is created; the answer prints the console line.
	const std::string Name = pCharacter;
	auto pContext = DbAsync::MakeContext(ClientID);
	BotDb::FindByName(Name)
		.Then([pContext, Name, JsonDump](Db::CResultSet&& Existing)
	{
		auto* pConsole = pContext->GS()->Console();
		if(Existing.HasRows())
		{
			const int ID = Existing.GetInt("ID");
			BotDb::SaveTeeInfo(ID, JsonDump);
			pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "parseskin", "Updated character bot!");
			return;
		}

		BotDb::Create(Name, JsonDump);
		pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "parseskin", "Added new character bot!");
	});
}