#include "scenario_dungeon.h"

#include <game/server/gamecontext.h>

CDungeonScenario::CDungeonScenario(const nlohmann::json& jsonData)
	: GroupScenarioBase()
{
	m_JsonData = jsonData;
}

CDungeonScenario::~CDungeonScenario()
{
}

void CDungeonScenario::OnSetupScenario()
{
	if(!m_JsonData.is_object() || !m_JsonData.contains("steps"))
	{
		dbg_msg("scenario-tutorial", "Invalid JSON format or missing 'steps'");
		return;
	}

	// initialize steps
	for(const auto& step : m_JsonData["steps"])
		ProcessStep(step);
}

void CDungeonScenario::ProcessStep(const nlohmann::json& step)
{
	// check valid action
	if(!step.contains("action") || !step["action"].is_string())
	{
		dbg_msg("scenario-tutorial", "Missing or invalid 'action' key in JSON");
		return;
	}

	std::string action = step["action"];

	// message
	if(action == "message")
	{
		int delay = step.value("delay", 0);
		std::string chatMsg = step.value("chat", "");
		std::string broadcastMsg = step.value("broadcast", "");
		std::string fullMsg = step.value("full", "");

		if(!fullMsg.empty())
			AddMessageStep(delay, fullMsg, fullMsg);
		else
			AddMessageStep(delay, broadcastMsg, chatMsg);
	}

	// fixed cam
	else if(action == "fix_cam")
	{
		int delay = step.value("delay", 0);
		vec2 position = { step["position"].value("x", 0.0f), step["position"].value("y", 0.0f) };
		AddFixedCam(delay, position);
	}

	// create door
	else if(action == "new_door")
	{
		std::string Key = step.value("key", "");
		bool Follow = step.value("follow", false);
		m_vpDoors[Key].m_Pos = step["position"];

		auto& newDoorStep = AddStep();
		newDoorStep.WhenStarted([Follow, Key, this]()
		{
			vec2 Pos = m_vpDoors[Key].m_Pos;
			m_vpDoors[Key].m_EntPtr = std::make_unique<CEntityBaseDoor>(&GS()->m_World, CGameWorld::ENTTYPE_DEFAULT_DOOR, Pos, vec2(0, -1));

			if(Follow)
				AddFixedCam(100, Pos);
		});
	}

	// remove door
	else if(action == "remove_door")
	{
		std::string Key = step.value("key", "");
		bool Follow = step.value("follow", false);
		vec2 DoorPos = m_vpDoors[Key].m_Pos;

		auto& removeDoorStep = AddStep();
		removeDoorStep.WhenStarted([Follow, DoorPos, Key, this]()
		{
			if(m_vpDoors.contains(Key))
				m_vpDoors.erase(Key);

			if(Follow)
				AddFixedCam(100, DoorPos);
		});
	}

	// object destroy
	else if(action == "object_destroy")
	{
		std::vector<vec2> objects;
		if(step.contains("objects") && step["objects"].is_array())
		{
			for(const auto& marker : step["objects"])
			{
				vec2 position = { marker["position"].value("x", 0.0f), marker["position"].value("y", 0.0f) };
				objects.emplace_back(position);
			}
		}

		AddObjectsDestroy(objects);
	}

	// defeat
	else if(action == "defeat_mobs")
	{
		// initialize variables
		struct DefeatState
		{
			std::vector<int> SpawnedBotIds;
			int KillsMade = 0;
		};

		auto pState = std::make_shared<DefeatState>();
		const auto& stepData = static_cast<const nlohmann::json&>(step);
		const std::string Mode = stepData.value("mode", "annihilation");
		const int KillTarget = stepData.value("kill_target", (int)NOPE);

		// add step
		auto& defeatMobs = AddStep();
		defeatMobs.WhenStarted([this, pState, stepData]()
		{
			const vec2 pos = stepData.value("position", vec2 {});
			const float radius = stepData.value("radius", 180.f);
			for(const auto& mobData : stepData.value("mobs", nlohmann::json::array()))
			{
				// check valid default mod data
				const int mobID = mobData.value("mob_id", (int)NOPE);
				if(!MobBotInfo::ms_aMobBot.contains(mobID))
				{
					dbg_msg("scenario-dungeon", "can't find mobId %d for (defeat task)", mobID);
					continue;
				}

				// initialize new mob data
				MobBotInfo mobInfo = MobBotInfo::ms_aMobBot[mobID];
				mobInfo.m_Level = mobData.value("level", 1);
				mobInfo.m_Power = mobData.value("power", 1);
				mobInfo.m_Position = pos;
				mobInfo.m_Radius = radius;
				mobInfo.m_WorldID = GS()->GetWorldID();
				for(int i = 0; i < mobData.value("count", 1); ++i)
				{
					if(auto* pPlayerBot = GS()->CreateBot(TYPE_BOT_MOB, mobInfo.m_BotID, mobID))
					{
						pPlayerBot->InitBotMobInfo(mobInfo);
						pPlayerBot->SetAllowedSpawn(true);
						pState->SpawnedBotIds.push_back(pPlayerBot->GetCID());
					}
				}
			}
		});

		// when active
		defeatMobs.WhenActive([this, pState, Mode, KillTarget]()
		{
			for(int BotCID : pState->SpawnedBotIds)
			{
				// mode wave
				if(Mode == "wave")
				{
					if(pState->KillsMade >= KillTarget)
					{
						GS()->DestroyPlayer(BotCID);
						continue;
					}

					auto* pPlayerBot = static_cast<CPlayerBot*>(GS()->GetPlayer(BotCID));
					if(pPlayerBot && !pPlayerBot->IsAllowedSpawn())
					{
						pState->KillsMade++;
						pPlayerBot->SetAllowedSpawn(true);
					}
				}
				// mode default
				else
				{
					auto* pPlayer = GS()->GetPlayer(BotCID);
					if(pPlayer && !pPlayer->GetCharacter())
						GS()->DestroyPlayer(BotCID);
				}
			}
		});

		// condition
		defeatMobs.CheckCondition(ConditionPriority::CONDITION_AND_TIMER, [this, Mode, pState]()
		{
			std::erase_if(pState->SpawnedBotIds, [this](int BotCID)
			{
				return GS()->GetPlayer(BotCID) == nullptr;
			});

			return pState->SpawnedBotIds.empty();
		});
	}
}

void CDungeonScenario::AddMessageStep(int delay, const std::string& broadcastMsg, const std::string& chatMsg)
{
	auto& messageStep = AddStep(delay);
	messageStep.WhenStarted([this, delay, chatMsg, broadcastMsg]()
	{
		if(!broadcastMsg.empty())
		{
			for(auto* pPlayer : GetPlayers())
				GS()->Broadcast(pPlayer->GetCID(), BroadcastPriority::VeryImportant, delay, broadcastMsg.c_str());
		}

		if(!chatMsg.empty())
		{
			for(auto* pPlayer : GetPlayers())
				GS()->Chat(pPlayer->GetCID(), chatMsg.c_str());
		}
	});
}

void CDungeonScenario::AddObjectsDestroy(const std::vector<vec2>& objects)
{
	auto& newStep = AddStep();

	newStep.WhenStarted([this, objects]
	{
		for(const auto& pos : objects)
		{
			auto newObject = std::make_unique<CEntityObjectDestroy>(&GS()->m_World, pos, 3);
			m_vObjectDestroy.push_back(std::move(newObject));
		}
	});

	newStep.CheckCondition(ConditionPriority::CONDITION_AND_TIMER, [this]
	{
		return std::ranges::all_of(m_vObjectDestroy, [](const auto& uniquePtr)
		{
			return uniquePtr && !uniquePtr->IsActive();
		});
	});

	newStep.WhenFinished([this]
	{
		m_vObjectDestroy.clear();
	});
}

void CDungeonScenario::AddFixedCam(int delay, const vec2& pos)
{
	auto& step = AddStep(delay);
	step.WhenActive([this, pos]()
	{
		for(auto* pPlayer : GetPlayers())
			pPlayer->LockedView().ViewLock(pos, true);
	});
}

