#ifndef GAME_SERVER_CORE_SCENARIOS_SCENARIO_TUTORIAL_H
#define GAME_SERVER_CORE_SCENARIOS_SCENARIO_TUTORIAL_H

#include <game/server/core/tools/event_listener.h>
#include <game/server/core/tools/scenario_manager.h>

class CEntityGroup;

class CTutorialScenario : public ScenarioBase, public IEventListener
{
	nlohmann::json m_JsonData {};
	std::vector <std::weak_ptr<CEntityGroup>> m_vpShootmarkers {};
	vec2 m_MovementPos {};

public:
	CTutorialScenario(const nlohmann::json& jsonData);

protected:
	bool OnStopConditions() override;
	void OnSetupScenario() override;
	void ProcessStep(const nlohmann::json& step);

private:
	void Message(int delay, const std::string& text);
	void EmoteMessage(int delay, int emoteType, int emoticonType, const std::string& text);
	void Shootmarkers(const std::vector<std::pair<vec2, int>>& vShotmarkers, const std::string& text);
	void FixedCam(int delay, const vec2& pos, const std::string& startMsg, const std::string& endMsg);
	void Teleport(int delay, const vec2& pos, const std::string& text);
	void MovingDisable(bool State);
	void MovementTask(int delay, const vec2& pos, const std::string&, const std::string& text, bool lockView = true);

	bool IsShootingComplete() const;

	void CreateEntityShootmarkersTask(const vec2& pos, int health);
	void SendBroadcast(const std::string& text) const;
};

#endif
