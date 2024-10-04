#include "scenario_universal.h"

#include <game/server/gamecontext.h>

#include <game/server/entities/projectile.h>

CUniversalScenario::CUniversalScenario(const nlohmann::json& jsonArrayData)
	: ScenarioBase(SCENARIO_UNIVERSAL)
{
	m_JsonArrayData = jsonArrayData;
}

bool CUniversalScenario::OnStopConditions()
{
	const CPlayer* pPlayer = GetPlayer();
	return !pPlayer || !pPlayer->GetCharacter();
}

void CUniversalScenario::OnSetupScenario()
{
	if(!m_JsonArrayData.is_object() || !m_JsonArrayData.contains("steps"))
	{
		dbg_msg("scenario-universal", "Invalid JSON format or missing 'steps'");
		return;
	}

	// initialize steps
	for(const auto& step : m_JsonArrayData["steps"])
	{
		InitStep(step);
	}
}

void CUniversalScenario::InitStep(const nlohmann::json& step)
{
	if(!step.contains("action") || !step["action"].is_string())
	{
		dbg_msg("scenario-tutorial", "Missing or invalid 'action' key in JSON");
		return;
	}

	std::string action = step["action"];

	// message
	if(action == "game_message")
	{
		const int delay = step.value("delay", 0);
		const auto chatText = step.value("chat", "\0");
		const auto broadcastText = step.value("broadcast", "\0");
		const int emoteType = step.value("emote", -1);
		const int emoticonType = step.value("emoticon", -1);

		Message(delay, chatText, broadcastText, emoteType, emoticonType);
	}
	// teleport
	else if(action == "teleport")
	{
		const int delay = step.value("delay", 0);
		const vec2 position = { step["position"].value("x", 0.0f), step["position"].value("y", 0.0f) };

		Teleport(delay, position);
	}
	// fixed cam
	else if(action == "fixed_cam")
	{
		const int delay = step.value("delay", 0);
		const vec2 position = { step["position"].value("x", 0.0f), step["position"].value("y", 0.0f) };
		const auto whenActiveBroadcastText = step.value("text", "");

		FixedCam(delay, position, whenActiveBroadcastText);
	}
	// check quest accepted
	else if(action == "check_quest_accepted")
	{
		const int questID = step.value("quest_id", -1);

		if(questID > 0)
		{
			AddStep().CheckCondition(ConditionPriority::CONDITION_AND_TIMER, [this, questID](auto*) 
			{
				return GetPlayer()->GetQuest(questID)->IsAccepted();
			});
		}
	}
	// check quest completed
	else if(action == "check_quest_completed")
	{
		const int questID = step.value("quest_id", -1);

		if(questID > 0)
		{
			AddStep().CheckCondition(ConditionPriority::CONDITION_AND_TIMER, [this, questID](auto*) 
			{
				return GetPlayer()->GetQuest(questID)->IsCompleted();
			});
		}
	}
	// reset quest complete
	else if(action == "reset_quest")
	{
		const int questID = step.value("quest_id", -1);

		if(questID > 0)
		{
			AddStep().WhenFinished([questID](auto* pBase)
			{
				pBase->GetPlayer()->GetQuest(questID)->Reset();
			});
		}
	}
	else
	{
		dbg_msg("scenario-tutorial", "Unknown action: %s", action.c_str());
	}
}

void CUniversalScenario::FixedCam(int delay, const vec2& pos, const std::string& whenActiveBroadcast)
{
	auto& step = AddStep(delay);
	step.WhenActive([this, pos, whenActiveBroadcast](auto*)
	{
		GetPlayer()->LockedView().ViewLock(pos, true);
		SendBroadcast(whenActiveBroadcast);
	});
}

void CUniversalScenario::Teleport(int delay, const vec2& pos)
{
	auto& teleportStep = AddStep(delay);
	teleportStep.WhenStarted([this, pos](auto*)
	{
		GetCharacter()->ChangePosition(pos);
	});
}

void CUniversalScenario::Message(int delay, const std::string& chatText, const std::string& broadcastText, int emote, int emoticon)
{
	auto& messageStep = AddStep(delay);
	messageStep.WhenStarted([this, chatText, broadcastText, emote, emoticon](auto*)
	{
		if(!chatText.empty())
		{
			SendChat(chatText);
		}

		if(!broadcastText.empty())
		{
			SendBroadcast(broadcastText);
		}

		if(emote != -1)
		{
			GetCharacter()->SetEmote(emote, 1, false);
		}

		if(emoticon != -1)
		{
			GS()->SendEmoticon(GetClientID(), emoticon);
		}
	});
}

void CUniversalScenario::SendChat(const std::string& text) const
{
	GS()->Chat(GetClientID(), text.c_str());
}

void CUniversalScenario::SendBroadcast(const std::string& text) const
{
	GS()->Broadcast(GetClientID(), BroadcastPriority::VERY_IMPORTANT, 300, text.c_str());
}