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
	if(action == "message")
	{
		const int delay = step.value("delay", 0);
		const auto text = step.value("text", "");

		Message(delay, text);
	}
	// emote message
	else if(action == "emote_message")
	{
		const int delay = step.value("delay", 0);
		const int emoteType = step.value("emote_type", (int)EMOTE_NORMAL);
		const int emoticonType = step.value("emoticon_type", -1);
		const auto text = step.value("text", "");

		EmoteMessage(delay, emoteType, emoticonType, text);
	}
	// teleport
	else if(action == "teleport")
	{
		const int delay = step.value("delay", 0);
		const vec2 position = { step["position"].value("x", 0.0f), step["position"].value("y", 0.0f) };
		const auto text = step.value("text", "");

		Teleport(delay, position, text);
	}
	// fixed cam
	else if(action == "fixed_cam")
	{
		const int delay = step.value("delay", 0);
		const vec2 position = { step["position"].value("x", 0.0f), step["position"].value("y", 0.0f) };
		const auto startText = step.value("start_text", "");
		const auto endText = step.value("end_text", "");

		FixedCam(delay, position, startText, endText);
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

void CUniversalScenario::FixedCam(int delay, const vec2& pos, const std::string& startMsg, const std::string& endMsg)
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

void CUniversalScenario::Teleport(int delay, const vec2& pos, const std::string& text)
{
	auto& teleportStep = AddStep(delay);
	teleportStep.WhenStarted([this, pos, text](auto*)
	{
		GetCharacter()->ChangePosition(pos);
		SendBroadcast(text);
	});
}

void CUniversalScenario::Message(int delay, const std::string& text)
{
	auto& messageStep = AddStep(delay);
	messageStep.WhenStarted([this, text](auto*)
	{
		SendBroadcast(text);
	});
}

void CUniversalScenario::EmoteMessage(int delay, int emoteType, int emoticonType, const std::string& text)
{
	auto& emoteMessageStep = AddStep(delay);
	emoteMessageStep.WhenStarted([this, emoteType, emoticonType, text](auto*)
	{
		GetCharacter()->SetEmote(emoteType, 1, false);
		GS()->SendEmoticon(GetClientID(), emoticonType);
		SendBroadcast(text);
	});
}

void CUniversalScenario::SendBroadcast(const std::string& text) const
{
	GS()->Broadcast(GetClientID(), BroadcastPriority::VERY_IMPORTANT, 300, text.c_str());
}