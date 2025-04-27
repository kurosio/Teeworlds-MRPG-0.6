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