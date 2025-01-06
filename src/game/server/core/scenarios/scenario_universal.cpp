#include "scenario_universal.h"

#include <game/server/gamecontext.h>

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
		dbg_msg("scenario-universal", "Missing or invalid 'action' key in JSON");
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
			StepMessage(delay, fullMsg, fullMsg);
		else
			StepMessage(delay, broadcastMsg, chatMsg);
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
	// fixed cam
	else if(action == "fix_cam")
	{
		int delay = step.value("delay", 0);
		vec2 position = { step["position"].value("x", 0.0f), step["position"].value("y", 0.0f) };
		StepFixedCam(delay, position);
	}
}

void CUniversalScenario::StepFixedCam(int delay, const vec2& pos)
{
	auto& step = AddStep(delay);
	step.WhenActive([this, pos](auto*)
	{
		GetPlayer()->LockedView().ViewLock(pos, true);
	});
}

void CUniversalScenario::StepTeleport(const vec2& pos)
{
	auto& teleportStep = AddStep();
	teleportStep.WhenStarted([this, pos](auto*)
	{
		GetCharacter()->ChangePosition(pos);
	});
}

void CUniversalScenario::StepMessage(int delay, const std::string& broadcastMsg, const std::string& chatMsg)
{
	auto& messageStep = AddStep(delay);
	messageStep.WhenStarted([this, delay, chatMsg, broadcastMsg](auto*)
	{
		if(!broadcastMsg.empty())
		{
			GS()->Broadcast(GetClientID(), BroadcastPriority::VeryImportant, delay, broadcastMsg.c_str());
		}

		if(!chatMsg.empty())
		{
			GS()->Chat(GetClientID(), chatMsg.c_str());
		}
	});
}

void CUniversalScenario::StepEmote(int emoteType, int emoticonType)
{
	auto& emoteStep = AddStep();
	emoteStep.WhenStarted([this, emoteType, emoticonType](auto*)
	{
		GetCharacter()->SetEmote(emoteType, 1, false);
		GS()->SendEmoticon(GetClientID(), emoticonType);
	});
}

void CUniversalScenario::SendChat(const std::string& text) const
{
	GS()->Chat(GetClientID(), text.c_str());
}

void CUniversalScenario::SendBroadcast(const std::string& text) const
{
	GS()->Broadcast(GetClientID(), BroadcastPriority::VeryImportant, 300, text.c_str());
}