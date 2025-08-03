#include "scenario_player_manager.h"

void CScenarioPlayerManager::UpdateClientScenarios(int ClientID)
{
	auto clientIt = m_ClientScenarios.find(ClientID);
	if(clientIt == m_ClientScenarios.end())
		return;

	auto& scenarioIDs = clientIt->second;
	std::erase_if(scenarioIDs, [&](int scenarioID)
	{
		const ScenarioKey key = { ClientID, scenarioID };
		auto scenarioIt = m_vScenarios.find(key);
		bool shouldRemove = false;

		if(scenarioIt == m_vScenarios.end() || !scenarioIt->second)
		{
			shouldRemove = true;
		}
		else
		{
			ScenarioBase* ptr = scenarioIt->second.get();
			ptr->Tick();
			if(!ptr->IsRunning())
			{
				m_vScenarios.erase(scenarioIt);
				shouldRemove = true;
			}
		}

		return shouldRemove;
	});

	if(scenarioIDs.empty())
		m_ClientScenarios.erase(clientIt);
}

void CScenarioPlayerManager::StopAll(int ClientID)
{
	auto clientIt = m_ClientScenarios.find(ClientID);
	if(clientIt == m_ClientScenarios.end())
		return;

	std::vector<int> scenarioIDsToRemove(clientIt->second.begin(), clientIt->second.end());
	for(int scenarioID : scenarioIDsToRemove)
		RemoveScenarioInternal(ClientID, scenarioID);
}

bool CScenarioPlayerManager::IsActive(int ClientID, int ScenarioID) const noexcept
{
	auto it = m_vScenarios.find({ ClientID, ScenarioID });
	return it != m_vScenarios.end() && it->second && it->second->IsRunning();
}

bool CScenarioPlayerManager::HasActiveScenarios(int ClientID) const noexcept
{
	auto it = m_ClientScenarios.find(ClientID);
	return it != m_ClientScenarios.end() && !it->second.empty();
}

bool CScenarioPlayerManager::RemoveScenarioInternal(int ClientID, int ScenarioID)
{
	const ScenarioKey key = { ClientID, ScenarioID };
	if(m_vScenarios.erase(key) == 0)
		return false;

	auto clientIt = m_ClientScenarios.find(ClientID);
	if(clientIt != m_ClientScenarios.end())
	{
		auto& scenarioIDs = clientIt->second;
		scenarioIDs.erase(ScenarioID);
		if(scenarioIDs.empty())
			m_ClientScenarios.erase(clientIt);
		return true;
	}
	return false;
}