#ifndef GAME_SERVER_CORE_TOOLS_SCENARIO_GROUP_MANAGER_H
#define GAME_SERVER_CORE_TOOLS_SCENARIO_GROUP_MANAGER_H

#include <scenarios/base/scenario_base.h>
#include <scenarios/base/scenario_base_group.h>

#include <unordered_map>
#include <vector>

class CGS;

class CScenarioGroupManager
{
private:
	using ScenarioMap = std::unordered_map<int, std::shared_ptr<GroupScenarioBase>>;

	CGS* m_pGS {};
	int m_NextScenarioID = 1;
	ScenarioMap m_vScenarios {};

	int StartAndStoreScenario(const std::shared_ptr<GroupScenarioBase>& pScenario)
	{
		pScenario->Start();

		if(!pScenario->IsRunning())
			return -1;

		const int ScenarioID = m_NextScenarioID;
		auto [it, inserted] = m_vScenarios.emplace(ScenarioID, pScenario);
		if(!inserted)
			return -1;

		pScenario->m_ScenarioID = ScenarioID;
		++m_NextScenarioID;
		return ScenarioID;
	}

public:
	explicit CScenarioGroupManager(CGS* pGS) : m_pGS(pGS) { };
	~CScenarioGroupManager() = default;

	template<typename T, typename... Args>
	int RegisterScenario(int ClientID, Args&&... args) requires std::derived_from<T, GroupScenarioBase>
	{
		auto pScenario = std::make_shared<T>(std::forward<Args>(args)...);

		pScenario->m_pGS = m_pGS;

		if(ClientID > -1 && ClientID < MAX_PLAYERS)
		{
			if(!pScenario->AddParticipant(ClientID))
				return -1;
		}

		return StartAndStoreScenario(pScenario);
	}

	// Registers a group scenario with every participant already in place. A scenario runs the
	// start actions of its first step while it registers, so participants added only afterwards
	// stay invisible to those components - their intro messages, titles, camera locks and mob
	// scaling would all be resolved against an empty player list.
	template<typename T, typename... Args>
	int RegisterScenario(const std::vector<int>& vClientIDs, Args&&... args) requires std::derived_from<T, GroupScenarioBase>
	{
		auto pScenario = std::make_shared<T>(std::forward<Args>(args)...);

		pScenario->m_pGS = m_pGS;

		for(const int ClientID : vClientIDs)
		{
			if(ClientID > -1 && ClientID < MAX_PLAYERS)
				pScenario->AddParticipant(ClientID);
		}

		return StartAndStoreScenario(pScenario);
	}

	void UpdateScenarios();
	void RemoveClient(int ClientID);
	void StopScenario(int ScenarioID);

	template<typename T = GroupScenarioBase>
	std::shared_ptr<T> GetScenario(int ScenarioID) requires std::derived_from<T, GroupScenarioBase>
	{
		auto it = m_vScenarios.find(ScenarioID);
		if(it != m_vScenarios.end())
			return std::dynamic_pointer_cast<T>(it->second);
		return nullptr;
	}

	bool IsActive(int ScenarioID) const;

	std::shared_ptr<GroupScenarioBase> GetActiveScenarioByPlayer(int ClientID) const
	{
		for(const auto& [id, pScenario] : m_vScenarios)
		{
			if(pScenario && pScenario->IsRunning() && pScenario->GetParticipants().contains(ClientID))
				return pScenario;
		}

		return nullptr;
	}
};

#endif // GAME_SERVER_CORE_TOOLS_SCENARIO_GROUP_MANAGER_H