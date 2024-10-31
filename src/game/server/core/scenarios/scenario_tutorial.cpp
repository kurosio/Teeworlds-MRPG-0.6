#include "scenario_tutorial.h"

#include <game/server/gamecontext.h>

#include <game/server/core/entities/group/entitiy_group.h>
#include <game/server/entities/projectile.h>

CTutorialScenario::CTutorialScenario(const nlohmann::json& jsonData)
	: ScenarioBase(SCENARIO_TUTORIAL)
{
	m_JsonData = jsonData;
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
	// message
	else if(action == "message")
	{
		int delay = step.value("delay", 0);
		std::string text = step.value("text", "");

		Message(delay, text);
	}
	// emote message
	else if(action == "emote_message")
	{
		int delay = step.value("delay", 0);
		int emoteType = step.value("emote_type", (int)EMOTE_NORMAL);
		int emoticonType = step.value("emoticon_type", -1);
		std::string text = step.value("text", "");

		EmoteMessage(delay, emoteType, emoticonType, text);
	}
	// teleport
	else if(action == "teleport")
	{
		int delay = step.value("delay", 0);
		vec2 position = { step["position"].value("x", 0.0f), step["position"].value("y", 0.0f) };
		std::string text = step.value("text", "");

		Teleport(delay, position, text);
	}
	// movement task
	else if(action == "movement_task")
	{
		int delay = step.value("delay", 0);
		vec2 position = { step["position"].value("x", 0.0f), step["position"].value("y", 0.0f) };
		std::string lockViewText = step.value("lock_text", "");
		std::string text = step.value("text", "");
		bool loockView = step.value("lock_view", true);

		MovementTask(delay, position, lockViewText, text, loockView);
	}
	// fixed cam
	else if(action == "fixed_cam")
	{
		int delay = step.value("delay", 0);
		vec2 position = { step["position"].value("x", 0.0f), step["position"].value("y", 0.0f) };
		std::string startText = step.value("start_text", "");
		std::string endText = step.value("end_text", "");
		FixedCam(delay, position, startText, endText);
	}
	// freeze movements
	else if(action == "freeze_movements")
	{
		bool freezeState = step.value("state", false);
		MovingDisable(freezeState);
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

		std::string text = step.value("text", "");
		Shootmarkers(markers, text);
	}
	else
	{
		dbg_msg("scenario-tutorial", "Unknown action: %s", action.c_str());
	}
}

void CTutorialScenario::MovementTask(int delay, const vec2& pos, const std::string& lockViewText, const std::string& text, bool lockView)
{
	// is has lockView
	if(lockView)
	{
		MovingDisable(true);

		auto& lockStep = AddStep(delay);
		lockStep.WhenStarted([this, pos](auto*)
		{
			m_MovementPos = pos;
		});
		lockStep.WhenActive([this, lockViewText](auto*)
		{
			if(Server()->Tick() % (Server()->TickSpeed() / 2) == 0)
			{
				GS()->CreateHammerHit(m_MovementPos, CmaskOne(GetClientID()));
			}

			GetPlayer()->LockedView().ViewLock(m_MovementPos, true);
			SendBroadcast(lockViewText);
		});

		MovingDisable(false);
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

void CTutorialScenario::FixedCam(int delay, const vec2& pos, const std::string& startMsg, const std::string& endMsg)
{
	auto& step = AddStep(delay);
	step.WhenActive([this, pos, startMsg](auto*)
	{
		GetPlayer()->LockedView().ViewLock(pos, true);
		SendBroadcast(startMsg);
	});

	if(!endMsg.empty())
	{
		step.WhenFinished([this, endMsg](auto*)
		{
			SendBroadcast(endMsg);
		});
	}
}

void CTutorialScenario::Teleport(int delay, const vec2& pos, const std::string& text)
{
	auto& teleportStep = AddStep(delay);
	teleportStep.WhenStarted([this, pos, text](auto*)
	{
		GetCharacter()->ChangePosition(pos);
		SendBroadcast(text);
	});
}

void CTutorialScenario::MovingDisable(bool State)
{
	auto& movingStep = AddStep();
	movingStep.WhenStarted([this, State](auto*)
	{
		GetCharacter()->MovingDisable(State);
	});
}


void CTutorialScenario::Shootmarkers(const std::vector<std::pair<vec2, int>>& vShotmarkers, const std::string& text)
{
	for(const auto& [position, health] : vShotmarkers)
	{
		CreateEntityShootmarkersTask(position, health);
		FixedCam(100, position, "You can shoot with the left mouse button.", "");
	}

	auto& stepShootmarkers = AddStep();
	stepShootmarkers.WhenActive([this, text](auto*)
	{
		SendBroadcast(text);
	});
	stepShootmarkers.CheckCondition(ConditionPriority::CONDITION_AND_TIMER, [this](auto*)
	{
		return IsShootingComplete();
	});
}


void CTutorialScenario::Message(int delay, const std::string& text)
{
	auto& messageStep = AddStep(delay);
	messageStep.WhenStarted([this, text](auto*)
	{
		SendBroadcast(text);
	});
}

void CTutorialScenario::EmoteMessage(int delay, int emoteType, int emoticonType, const std::string& text)
{
	auto& emoteMessageStep = AddStep(delay);
	emoteMessageStep.WhenStarted([this, emoteType, emoticonType, text](auto*)
	{
		GetCharacter()->SetEmote(emoteType, 1, false);
		GS()->SendEmoticon(GetClientID(), emoticonType);
		SendBroadcast(text);
	});
}

bool CTutorialScenario::IsShootingComplete() const
{
	return std::ranges::all_of(m_vpShootmarkers, [](const std::weak_ptr<CEntityGroup>& weakPtr)
	{
		return weakPtr.expired();
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
				Health -= 1; // TODO fix
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