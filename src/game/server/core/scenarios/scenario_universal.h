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
	void StepMessage(int delay, const std::string& broadcastMsg, const std::string& chatMsg);
	void StepEmote(int emoteType, int emoticonType);
	void StepFixedCam(int delay, const vec2& pos);
	void StepTeleport(const vec2& pos);

	void SendChat(const std::string& text) const;
	void SendBroadcast(const std::string& text) const;
};

#endif
