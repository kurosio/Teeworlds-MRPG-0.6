#ifndef GAME_SERVER_CORE_SCENARIOS_SCENARIO_DUNGEON_H
#define GAME_SERVER_CORE_SCENARIOS_SCENARIO_DUNGEON_H

#include <game/server/core/tools/scenario_base.h>
#include <game/server/core/entities/logic/base_door.h>

class CDungeonScenario final : public GroupScenarioBase
{
	nlohmann::json m_JsonData {};

public:
	struct GroupDoor
	{
		vec2 m_Pos {};
		std::unique_ptr<CEntityBaseDoor> m_EntPtr {};
	};
	std::map<std::string, GroupDoor> m_vpDoors {};


	explicit CDungeonScenario(const nlohmann::json& jsonData);
	~CDungeonScenario() override = default;

protected:
	void OnSetupScenario() override;
	void ProcessStep(const nlohmann::json& step);
};

#endif