#ifndef GAME_SERVER_CORE_SCENARIOS_SCENARIO_EIDOLON_H
#define GAME_SERVER_CORE_SCENARIOS_SCENARIO_EIDOLON_H

#include <game/server/core/tools/event_listener.h>
#include <game/server/core/tools/scenario_manager.h>

class CEntityGroup;

class CEidolonScenario : public ScenarioBase, public IEventListener
{
	CPlayer* GetOwner() const;

public:
	CEidolonScenario() : ScenarioBase(SCENARIO_EIDOLON, FLAG_REPEATABLE) {}

protected:
	bool OnStopConditions() override;
	void OnSetupScenario() override;

private:
	void SendRandomChatMessage(const std::vector<const char*>& messages) const;
};

#endif
