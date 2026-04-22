#ifndef GAME_SERVER_CORE_SCENARIOS_IMPL_SCENARIO_WORLD_H
#define GAME_SERVER_CORE_SCENARIOS_IMPL_SCENARIO_WORLD_H

#include <scenarios/base/scenario_base_world.h>

class CWorldScenario : public WorldScenarioBase
{
	nlohmann::json m_JsonData {};

public:
	explicit CWorldScenario(const nlohmann::json& jsonData);

protected:
	void OnSetupScenario() override;
	void ProcessStep(const nlohmann::json& step);
};

#endif
