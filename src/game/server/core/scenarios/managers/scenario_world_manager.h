#ifndef GAME_SERVER_CORE_TOOLS_SCENARIO_WORLD_MANAGER_H
#define GAME_SERVER_CORE_TOOLS_SCENARIO_WORLD_MANAGER_H

#include <scenarios/base/scenario_base_world.h>
#include <game/server/gamecontext.h>

#include <unordered_map>

class CGS;

class CScenarioWorldManager
{
	using ScenarioMap = std::unordered_map<int, std::shared_ptr<WorldScenarioBase>>;

	CGS* m_pGS {};
	int m_NextScenarioID = 1;
	ScenarioMap m_vScenarios {};

public:
	explicit CScenarioWorldManager(CGS* pGS) : m_pGS(pGS) { };
	~CScenarioWorldManager() = default;

	template<typename T, typename... Args>
	int RegisterScenario(int WorldID, Args&&... args) requires std::derived_from<T, WorldScenarioBase>
	{
		const int scenarioID = m_NextScenarioID++;
		auto pScenario = std::make_shared<T>(std::forward<Args>(args)...);

		pScenario->m_pGS = m_pGS;
		pScenario->m_WorldID = WorldID;
		pScenario->m_ScenarioID = scenarioID;

		for(int ClientID = 0; ClientID < MAX_CLIENTS; ++ClientID)
		{
			if(!m_pGS->Server()->ClientIngame(ClientID))
				continue;

			pScenario->AddParticipant(ClientID);
		}

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
	void HandleClientEnter(int ClientID);

	template<typename T = WorldScenarioBase>
	std::shared_ptr<T> GetScenario(int ScenarioID) requires std::derived_from<T, WorldScenarioBase>
	{
		auto it = m_vScenarios.find(ScenarioID);
		if(it != m_vScenarios.end())
			return std::dynamic_pointer_cast<T>(it->second);
		return nullptr;
	}

	bool IsActive(int ScenarioID) const;
};

#endif
