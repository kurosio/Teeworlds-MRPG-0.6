#ifndef GAME_SERVER_CORE_SCENARIOS_IMPL_SCENARIO_WORLD_H
#define GAME_SERVER_CORE_SCENARIOS_IMPL_SCENARIO_WORLD_H

#include <scenarios/base/scenario_base_world.h>
#include <game/server/core/tools/event_listener.h>

class CWorldScenario : public WorldScenarioBase, public IEventListener
{
	nlohmann::json m_JsonData {};
	ScopedEventListener m_EventListener {};

public:
	explicit CWorldScenario(const nlohmann::json& jsonData);

protected:
	void OnSetupScenario() override;
	void ProcessStep(const nlohmann::json& step);
	void OnScenarioStart() override;
	void OnScenarioEnd() override;
	void OnCharacterDeath(CPlayer* pVictim, CPlayer* pKiller, int Weapon) override;
};

#endif
