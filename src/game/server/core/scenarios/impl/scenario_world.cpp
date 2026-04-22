#include "scenario_world.h"

CWorldScenario::CWorldScenario(const nlohmann::json& jsonData)
	: WorldScenarioBase()
{
	m_JsonData = jsonData;
}

void CWorldScenario::OnSetupScenario()
{
	if(!m_JsonData.is_object() || !m_JsonData.contains("steps") || !m_JsonData["steps"].is_array())
		return;

	const auto& steps = m_JsonData["steps"];
	if(!steps.empty())
		m_StartStepId = steps[0].value("id", "");

	for(const auto& step : steps)
		ProcessStep(step);
}

void CWorldScenario::ProcessStep(const nlohmann::json& stepJson)
{
	if(!stepJson.is_object())
		return;

	StepId id = stepJson.value("id", "");
	if(id.empty() || !stepJson.contains("components") || !stepJson["components"].is_array())
		return;

	auto& newStep = AddStep(id, stepJson.value("msg_info", ""), stepJson.value("delay", -1));
	SetupStep(newStep, stepJson);
}
