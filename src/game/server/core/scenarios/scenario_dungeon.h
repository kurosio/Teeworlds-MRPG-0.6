#ifndef GAME_SERVER_CORE_SCENARIOS_SCENARIO_DUNGEON_H
#define GAME_SERVER_CORE_SCENARIOS_SCENARIO_DUNGEON_H

#include <game/server/core/tools/scenario_base.h>
#include <game/server/core/tools/event_listener.h>

#include <game/server/core/entities/logic/base_door.h>
#include "entities/dungeon/object_destroy.h"

class CEntityGroup;

class CDungeonScenario : public GroupScenarioBase, public IEventListener
{
	struct GroupDoor
	{
		vec2 m_Pos {};
		std::unique_ptr<CEntityBaseDoor> m_EntPtr {};
	};

	nlohmann::json m_JsonData {};
	std::vector<std::unique_ptr<CEntityObjectDestroy>> m_vObjectDestroy {};
	std::map<std::string, GroupDoor> m_vpDoors {};

public:
	explicit CDungeonScenario(const nlohmann::json& jsonData);
	~CDungeonScenario() override;

protected:
	void OnSetupScenario() override;
	void ProcessStep(const nlohmann::json& step);

private:
	void AddMessageStep(int delay, const std::string& broadcastMsg, const std::string& chatMsg);
	void AddObjectsDestroy(const std::vector<vec2>& objects);
	void AddFixedCam(int delay, const vec2& pos);
};

#endif
