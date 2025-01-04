#ifndef GAME_SERVER_CORE_SCENARIOS_SCENARIO_TUTORIAL_H
#define GAME_SERVER_CORE_SCENARIOS_SCENARIO_TUTORIAL_H

#include "entities/personal_door.h"
#include <game/server/core/tools/event_listener.h>
#include <game/server/core/tools/scenario_manager.h>

class CEntityGroup;
class CEntityPersonalDoor;

class CTutorialScenario : public ScenarioBase, public IEventListener
{
	nlohmann::json m_JsonData {};

	struct PersonalDoor
	{
		vec2 m_Pos {};
		std::unique_ptr<CEntityPersonalDoor> m_EntPtr {};
	};
	std::map<std::string, PersonalDoor> m_vpPersonalDoors {};
	std::vector <std::weak_ptr<CEntityGroup>> m_vpShootmarkers {};
	vec2 m_MovementPos {};

public:
	CTutorialScenario(const nlohmann::json& jsonData);
	~CTutorialScenario();

protected:
	bool OnStopConditions() override;
	void OnSetupScenario() override;
	void ProcessStep(const nlohmann::json& step);

private:
	void StepMessage(int delay, const std::string& broadcastMsg, const std::string& chatMsg);
	void StepEmote(int emoteType, int emoticonType);
	void StepShootmarkers(const std::vector<std::pair<vec2, int>>& vShotmarkers);
	void StepFixedCam(int delay, const vec2& pos);
	void StepTeleport(const vec2& pos);
	void StepMovingDisable(bool State);
	void StepMovementTask(int delay, const vec2& pos, const std::string&, const std::string& text, bool lockView = true);

	void CreateEntityShootmarkersTask(const vec2& pos, int health);
	void SendBroadcast(const std::string& text) const;
};

#endif
