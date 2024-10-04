#ifndef GAME_SERVER_CORE_SCENARIOS_SCENARIO_UNIVERSAL_H
#define GAME_SERVER_CORE_SCENARIOS_SCENARIO_UNIVERSAL_H

#include <game/server/core/tools/event_listener.h>
#include <game/server/core/tools/scenario_manager.h>

class CEntityGroup;

class CUniversalScenario : public ScenarioBase, public IEventListener
{
	nlohmann::json m_JsonArrayData {};

public:
	CUniversalScenario(const nlohmann::json& jsonArrayData);

protected:
	bool OnStopConditions() override;
	void OnSetupScenario() override;
	void InitStep(const nlohmann::json& step);

private:
	void Message(int delay, const std::string& text);
	void EmoteMessage(int delay, int emoteType, int emoticonType, const std::string& text);
	void FixedCam(int delay, const vec2& pos, const std::string& startMsg, const std::string& endMsg);
	void Teleport(int delay, const vec2& pos, const std::string& text);
	void SendBroadcast(const std::string& text) const;
};

#endif
