#ifndef GAME_SERVER_CORE_SCENARIOS_SCENARIO_EIDOLON_H
#define GAME_SERVER_CORE_SCENARIOS_SCENARIO_EIDOLON_H

#include <scenarios/base/scenario_base.h>
#include <scenarios/base/scenario_base_player.h>

#include <game/server/core/tools/event_listener.h>

class CEntityGroup;

class CEidolonScenario : public PlayerScenarioBase, public IEventListener
{
	CPlayer* GetOwner() const;

public:
	CEidolonScenario() : PlayerScenarioBase(FLAG_REPEATABLE) {}

protected:
	bool OnStopConditions() override;
	void OnSetupScenario() override;

private:
	void SendRandomChatMessage(const std::vector<const char*>& messages) const;
};

#endif
