#ifndef GAME_SERVER_CORE_SCENARIOS_IMPL_SCENARIO_WORLD_H
#define GAME_SERVER_CORE_SCENARIOS_IMPL_SCENARIO_WORLD_H

#include <scenarios/base/scenario_base_world.h>
#include <game/server/core/tools/event_listener.h>

class CWorldScenario : public WorldScenarioBase, public IEventListener
{
public:
	struct RewardEntry
	{
		int m_ItemID {};
		int m_Value {};
		float m_Chance {};
	};

private:
	nlohmann::json m_JsonData {};
	ScopedEventListener m_EventListener {};
	std::vector<RewardEntry> m_vRewards {};

public:
	explicit CWorldScenario(const nlohmann::json& jsonData);
	void SetContextRewards(const std::vector<RewardEntry>& vRewards) { m_vRewards = vRewards; }
	const std::vector<RewardEntry>& GetContextRewards() const { return m_vRewards; }

protected:
	void OnSetupScenario() override;
	void ProcessStep(const nlohmann::json& step);
	void OnScenarioStart() override;
	void OnScenarioEnd() override;
	void OnCharacterDeath(CPlayer* pVictim, CPlayer* pKiller, int Weapon) override;
};

#endif
