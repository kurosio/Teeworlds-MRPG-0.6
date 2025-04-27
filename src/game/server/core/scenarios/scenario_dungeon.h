#ifndef GAME_SERVER_CORE_SCENARIOS_SCENARIO_DUNGEON_H
#define GAME_SERVER_CORE_SCENARIOS_SCENARIO_DUNGEON_H

#include "entities/personal_door.h"

#include <game/server/core/tools/scenario_base.h>
#include <game/server/core/tools/event_listener.h>

class CEntityDropItem;
class CEntityGroup;
class CEntityPersonalDoor;

class CDungeonScenario : public GroupScenarioBase, public IEventListener
{
	nlohmann::json m_JsonData {};

public:
	explicit CDungeonScenario(const nlohmann::json& jsonData);
	~CDungeonScenario() override;

protected:
	void OnSetupScenario() override;
	void ProcessStep(const nlohmann::json& step);

private:
	void AddMessageStep(int delay, const std::string& broadcastMsg, const std::string& chatMsg);
};

#endif
