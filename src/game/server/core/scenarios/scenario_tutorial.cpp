#include "scenario_tutorial.h"

#include <game/server/gamecontext.h>

#include <game/server/core/entities/group/entitiy_group.h>
#include <game/server/entities/projectile.h>

CTutorialScenario::CTutorialScenario(const nlohmann::json& jsonData)
	: ScenarioBase(SCENARIO_TUTORIAL)
{
	m_JsonData = jsonData;
}

CTutorialScenario::~CTutorialScenario()
{
	m_vpPersonalDoors.clear();
	m_vpShootmarkers.clear();
}

bool CTutorialScenario::OnStopConditions()
{
	const CPlayer* pPlayer = GetPlayer();
	return !pPlayer || !pPlayer->GetCharacter();
}

void CTutorialScenario::OnSetupScenario()
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

void CTutorialScenario::ProcessStep(const nlohmann::json& step)
{
	// check valid action
	if(!step.contains("action") || !step["action"].is_string())
	{
		dbg_msg("scenario-tutorial", "Missing or invalid 'action' key in JSON");
		return;
	}

	std::string action = step["action"];

	// reset quest
	if(action == "reset_quest")
	{
		if(int questID = step.value("quest_id", -1); questID > 0)
		{
			GetPlayer()->GetQuest(questID)->Reset();
		}
	}
	// create new door
	else if(action == "new_door")
	{
		bool Follow = step.value("follow", false);
		std::string Key = step.value("key", "");
		m_vpPersonalDoors[Key].m_Pos =
		{
			step["position"].value("x", 0.0f),
			step["position"].value("y", 0.0f)
		};

		auto& newDoorStep = AddStep();
		newDoorStep.WhenStarted([Key, this](auto*)
		{
			vec2 Pos = m_vpPersonalDoors[Key].m_Pos;
			m_vpPersonalDoors[Key].m_EntPtr = std::make_unique<CEntityPersonalDoor>(&GS()->m_World, GetClientID(), Pos, vec2(0, -1));
		});

		if(Follow)
		{
			StepMessage(0, "The door's locked!");
			StepFixedCam(100, m_vpPersonalDoors[Key].m_Pos);
		}
	}
	// remove door
	else if(action == "remove_door")
	{
		bool Follow = step.value("follow", false);
		std::string Key = step.value("key", "");
		vec2 DoorPos = m_vpPersonalDoors[Key].m_Pos;

		auto& removeDoorStep = AddStep();
		removeDoorStep.WhenStarted([Follow, Key, this](auto*)
		{
			if(m_vpPersonalDoors.contains(Key))
			{
				m_vpPersonalDoors.erase(Key);
			}
		});

		if(Follow)
		{
			StepMessage(0, "The door's is oppened!");
			StepFixedCam(100, DoorPos);
		}
	}
	// message
	else if(action == "message")
	{
		int delay = step.value("delay", 0);
		std::string text = step.value("text", "");

		StepMessage(delay, text);
	}
	// emote message
	else if(action == "emote")
	{
		int emoteType = step.value("emote_type", (int)EMOTE_NORMAL);
		int emoticonType = step.value("emoticon_type", -1);

		StepEmote(emoteType, emoticonType);
	}
	// teleport
	else if(action == "teleport")
	{
		vec2 position = 
		{
			step["position"].value("x", 0.0f), 
			step["position"].value("y", 0.0f) 
		};

		StepTeleport(position);
	}
	// movement task
	else if(action == "movement_task")
	{
		int delay = step.value("delay", 0);
		vec2 position = 
		{
			step["position"].value("x", 0.0f), 
			step["position"].value("y", 0.0f) 
		};
		std::string text = step.value("text", "");
		std::string targetLookText = step.value("target_lock_text", "");
		bool targetLook = step.value("target_look", true);

		StepMovementTask(delay, position, targetLookText, text, targetLook);
	}
	// fixed cam
	else if(action == "fix_cam")
	{
		int delay = step.value("delay", 0);
		vec2 position = { step["position"].value("x", 0.0f), step["position"].value("y", 0.0f) };
		StepFixedCam(delay, position);
	}
	// freeze movements
	else if(action == "freeze_movements")
	{
		bool freezeState = step.value("state", false);
		StepMovingDisable(freezeState);
	}
	// check quest accepted
	else if(action == "check_quest_accepted")
	{
		if(int questID = step.value("quest_id", -1); questID > 0)
		{
			AddStep().CheckCondition(ConditionPriority::CONDITION_AND_TIMER, [this, questID](auto*) {
				return GetPlayer()->GetQuest(questID)->IsAccepted();
			});
		}
	}
	// check quest completed
	else if(action == "check_quest_completed")
	{
		if(int questID = step.value("quest_id", -1); questID > 0)
		{
			AddStep().CheckCondition(ConditionPriority::CONDITION_AND_TIMER, [this, questID](auto*) {
				return GetPlayer()->GetQuest(questID)->IsCompleted();
			});
		}
	}
	// shotmarks
	else if(action == "shootmarkers")
	{
		std::vector<std::pair<vec2, int>> markers;
		if(step.contains("markers") && step["markers"].is_array())
		{
			for(const auto& marker : step["markers"])
			{
				vec2 position = { marker["position"].value("x", 0.0f), marker["position"].value("y", 0.0f) };
				int health = marker.value("health", 1);
				markers.emplace_back(position, health);
			}
		}

		StepShootmarkers(markers);
	}
	else
	{
		dbg_msg("scenario-tutorial", "Unknown action: %s", action.c_str());
	}
}

void CTutorialScenario::StepMovementTask(int delay, const vec2& pos, const std::string& targetLookText, const std::string& text, bool targetLook)
{
	// is has lockView
	if(targetLook)
	{
		StepMovingDisable(true);

		auto& lockStep = AddStep(delay);
		lockStep.WhenStarted([this, pos](auto*)
		{
			m_MovementPos = pos;
		});
		lockStep.WhenActive([this, targetLookText](auto*)
		{
			if(Server()->Tick() % (Server()->TickSpeed() / 2) == 0)
			{
				GS()->CreateHammerHit(m_MovementPos, CmaskOne(GetClientID()));
			}

			GetPlayer()->LockedView().ViewLock(m_MovementPos, true);
			SendBroadcast(targetLookText);
		});

		StepMovingDisable(false);
	}

	// movements
	auto& moveStep = AddStep();
	moveStep.WhenStarted([this](auto*)
	{
		GetCharacter()->MovingDisable(false);
	});
	moveStep.WhenActive([this, text](auto*)
	{
		if(Server()->Tick() % (Server()->TickSpeed() / 2) == 0)
		{
			GS()->CreateHammerHit(m_MovementPos, CmaskOne(GetClientID()));
		}

		SendBroadcast(text);
	});
	moveStep.CheckCondition(ConditionPriority::CONDITION_AND_TIMER, [this](auto*)
	{
		return distance(GetCharacter()->GetPos(), m_MovementPos) < 32.f;
	});
}

void CTutorialScenario::StepFixedCam(int delay, const vec2& pos)
{
	auto& step = AddStep(delay);
	step.WhenActive([this, pos](auto*)
	{
		GetPlayer()->LockedView().ViewLock(pos, true);
	});
}

void CTutorialScenario::StepTeleport(const vec2& pos)
{
	auto& teleportStep = AddStep();
	teleportStep.WhenStarted([this, pos](auto*)
	{
		GetCharacter()->ChangePosition(pos);
	});
}

void CTutorialScenario::StepMovingDisable(bool State)
{
	auto& movingStep = AddStep();
	movingStep.WhenStarted([this, State](auto*)
	{
		GetCharacter()->MovingDisable(State);
	});
}


void CTutorialScenario::StepShootmarkers(const std::vector<std::pair<vec2, int>>& vShotmarkers)
{
	for(const auto& [position, health] : vShotmarkers)
	{
		CreateEntityShootmarkersTask(position, health);
		StepMessage(0, "You can shoot with the left mouse button.");
		StepFixedCam(100, position);
	}

	auto& stepShootmarkers = AddStep();
	stepShootmarkers.WhenActive([this](auto*)
	{
		SendBroadcast("Shoot the targets!");
	});
	stepShootmarkers.CheckCondition(ConditionPriority::CONDITION_AND_TIMER, [this](auto*)
	{
		return std::ranges::all_of(m_vpShootmarkers, [](const std::weak_ptr<CEntityGroup>& weakPtr)
		{
			return weakPtr.expired();
		});
	});
}

void CTutorialScenario::StepMessage(int delay, const std::string& text)
{
	auto& messageStep = AddStep(delay);
	messageStep.WhenStarted([this, text](auto*)
	{
		SendBroadcast(text);
	});
}

void CTutorialScenario::StepEmote(int emoteType, int emoticonType)
{
	auto& emoteStep = AddStep();
	emoteStep.WhenStarted([this, emoteType, emoticonType](auto*)
	{
		GetCharacter()->SetEmote(emoteType, 1, false);
		GS()->SendEmoticon(GetClientID(), emoticonType);
	});
}

void CTutorialScenario::CreateEntityShootmarkersTask(const vec2& pos, int health)
{
	// initialize group
	auto groupPtr = CEntityGroup::NewGroup(&GS()->m_World, CGameWorld::ENTTYPE_ACTION, GetClientID());
	groupPtr->SetConfig("health", health);

	// initialize effect
	const auto pEntity = groupPtr->CreatePickup(pos);
	pEntity->RegisterEvent(CBaseEntity::EventTick, [](CBaseEntity* pBase)
	{
		// health
		int& Health = pBase->GetGroup()->GetRefConfig("health", 0);
		if(Health <= 0)
		{
			pBase->GS()->CreateCyrcleExplosion(6, 64.f, pBase->GetPos(), -1, WEAPON_WORLD, 0);
			pBase->MarkForDestroy();
			return;
		}

		// destroy projectiles
		for(auto* pProj = (CProjectile*)pBase->GameWorld()->FindFirst(CGameWorld::ENTTYPE_PROJECTILE); pProj; pProj = (CProjectile*)pProj->TypeNext())
		{
			if(distance(pBase->GetPos(), pProj->GetCurrentPos()) < 48.f)
			{
				Health -= 1;
				pBase->GS()->CreateDamage(pBase->GetPos(), pBase->GetClientID(), 1, false, random_angle(), CmaskOne(pBase->GetClientID()));
				pProj->MarkForDestroy();
			}
		}
	});
	m_vpShootmarkers.emplace_back(groupPtr);
}

void CTutorialScenario::SendBroadcast(const std::string& text) const
{
	GS()->Broadcast(GetClientID(), BroadcastPriority::VeryImportant, 300, text.c_str());
}