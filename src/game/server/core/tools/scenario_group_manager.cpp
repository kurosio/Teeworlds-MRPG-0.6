#include "scenario_group_manager.h"

void CScenarioGroupManager::UpdateScenarios()
{
	// update and collect for erase
	std::vector<int> scenariosToRemove;
	for(auto const& [id, pScenario] : m_vScenarios)
	{
		if(pScenario)
		{
			pScenario->Tick();

			if(!pScenario->IsRunning())
				scenariosToRemove.push_back(id);
		}
		else
			scenariosToRemove.push_back(id);
	}

	// remove from scenarios
	for(int scenarioID : scenariosToRemove)
		RemoveScenarioInternal(scenarioID);
}

void CScenarioGroupManager::RemoveClient(int ClientID)
{
	for(auto& [id, pScenario] : m_vScenarios)
	{
		if(pScenario)
			pScenario->RemoveParticipant(ClientID);
	}
}

bool CScenarioGroupManager::IsActive(int ScenarioID) const
{
	auto it = m_vScenarios.find(ScenarioID);
	if(it != m_vScenarios.end() && it->second)
		return it->second->IsRunning();

	return false;
}

void CScenarioGroupManager::RemoveScenarioInternal(int scenarioID)
{
	auto scenarioIt = m_vScenarios.find(scenarioID);
	if(scenarioIt == m_vScenarios.end())
		return;

	std::shared_ptr<GroupScenarioBase> pScenario = scenarioIt->second;

	if(pScenario && pScenario->IsRunning())
		pScenario->Stop();

	m_vScenarios.erase(scenarioIt);
}