#ifndef GAME_SERVER_CORE_TOOLS_SCENARIO_GROUP_MANAGER_H
#define GAME_SERVER_CORE_TOOLS_SCENARIO_GROUP_MANAGER_H

#include "scenario_base.h"

class CGS;

class CScenarioGroupManager
{
private:
	using ScenarioMap = std::unordered_map<int, std::shared_ptr<GroupScenarioBase>>;

	CGS* m_pGS {};
	int m_NextScenarioID = 1;
	ScenarioMap m_vScenarios {};

public:
	explicit CScenarioGroupManager(CGS* pGS) : m_pGS(pGS) { };
	~CScenarioGroupManager() = default;

	template<typename T, typename... Args>
	int RegisterScenario(int ClientID, Args&&... args)
	{
		static_assert(std::is_base_of_v<GroupScenarioBase, T>, "T must derive from GroupScenarioBase for CScenarioGroupManager");

		int scenarioID = m_NextScenarioID++;
		auto pScenario = std::make_shared<T>(m_pGS, std::forward<Args>(args)...);
		pScenario->m_ScenarioID = scenarioID;
		pScenario->m_pGS = m_pGS;

		if(!pScenario->AddParticipant(ClientID))
			return -1;

		pScenario->Start();

		if(!pScenario->IsRunning())
			return -1;

		auto [it, inserted] = m_vScenarios.emplace(scenarioID, pScenario);
		if(!inserted)
			return -1;

		return scenarioID;
	}

	void UpdateScenarios();
	void RemoveClient(int ClientID);

	template<typename T = GroupScenarioBase>
	std::shared_ptr<T> GetScenario(int ScenarioID)
	{
		static_assert(std::is_base_of_v<GroupScenarioBase, T>, "T must derive from GroupScenarioBase");

		auto it = m_vScenarios.find(ScenarioID);
		if(it != m_vScenarios.end())
			return std::dynamic_pointer_cast<T>(it->second);

		return nullptr;
	}

	bool IsActive(int ScenarioID) const;
	size_t GetScenarioCount() const noexcept { return m_vScenarios.size(); }

private:
	void RemoveScenarioInternal(int scenarioID);
};

#endif // GAME_SERVER_CORE_TOOLS_SCENARIO_GROUP_MANAGER_H