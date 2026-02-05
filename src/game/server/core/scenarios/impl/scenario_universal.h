#ifndef GAME_SERVER_CORE_SCENARIOS_SCENARIO_TUTORIAL_H
#define GAME_SERVER_CORE_SCENARIOS_SCENARIO_TUTORIAL_H

#include <scenarios/base/scenario_base.h>
#include <scenarios/base/scenario_base_player.h>
#include <scenarios/entities/personal_door.h>

#include <game/server/core/tools/event_listener.h>

class CEntityDropItem;
class CEntityGroup;
class CEntityPersonalDoor;

class CUniversalScenario : public PlayerScenarioBase, public IEventListener
{
	nlohmann::json m_JsonData {};

	struct PersonalDoor
	{
		vec2 m_Pos {};
		std::unique_ptr<CEntityPersonalDoor> m_EntPtr {};
	};
	std::map<std::string, PersonalDoor> m_vpPersonalDoors {};
	std::vector<std::weak_ptr<CEntityGroup>> m_vpShootmarkers {};

public:
	CUniversalScenario(const nlohmann::json& jsonData);
	~CUniversalScenario();

	void CreatePersonalDoor(const std::string& key, const vec2& pos);
	void RemovePersonalDoor(const std::string& key);
	void ResetShootmarkers();
	bool IsShootmarkersDestroyed() const;
	void CreateShootmarker(const vec2& pos, int health);

protected:
	bool OnStopConditions() override;
	void OnSetupScenario() override;
	void ProcessStep(const nlohmann::json& step);
};

#endif
