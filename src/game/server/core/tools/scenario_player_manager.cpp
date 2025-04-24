#include "scenario_player_manager.h"

void CScenarioPlayerManager::UpdateClientScenarios(int ClientID)
{
	auto clientIt = m_ClientScenarios.find(ClientID);
	if(clientIt == m_ClientScenarios.end())
		return;

	// update and collect for erasing
	std::vector<int> scenariosToRemove;
	const auto& clientScenarioIDs = clientIt->second;
	for(int scenarioID : clientScenarioIDs)
	{
		const ScenarioKey key = { ClientID, scenarioID };
		auto scenarioIt = m_vScenarios.find(key);
		bool shouldRemove = false;

		if(scenarioIt == m_vScenarios.end())
		{
			shouldRemove = true;
		}
		else
		{
			ScenarioBase* ptr = scenarioIt->second.get();
			if(!ptr)
			{
				shouldRemove = true;
			}
			else
			{
				ptr->Tick();

				if(!ptr->IsRunning())
					shouldRemove = true;
			}
		}

		if(shouldRemove)
			scenariosToRemove.push_back(scenarioID);
	}

	// remove from scenarios
	for(int scenarioID : scenariosToRemove)
		RemoveScenarioInternal(ClientID, scenarioID);
}

void CScenarioPlayerManager::StopAll(int ClientID)
{
	// check valid iterator
	auto clientIt = m_ClientScenarios.find(ClientID);
	if(clientIt == m_ClientScenarios.end())
		return;

	// stop all for client
	std::vector<int> scenarioIDsToRemove(clientIt->second.begin(), clientIt->second.end());
	for(int scenarioID : scenarioIDsToRemove)
		RemoveScenarioInternal(ClientID, scenarioID);
}

PlayerScenarioBase* CScenarioPlayerManager::GetPlayerScenario(int ClientID, int ScenarioID)
{
	return GetScenario<PlayerScenarioBase>(ClientID, ScenarioID);
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

std::vector<int> CScenarioPlayerManager::GetActiveScenarios(int ClientID) const
{
	// check valid iterator
	auto it = m_ClientScenarios.find(ClientID);
	if(it == m_ClientScenarios.end())
		return {};

	std::vector<int> result(it->second.begin(), it->second.end());
	return result;
}

size_t CScenarioPlayerManager::GetScenarioCount(int ClientID) const noexcept
{
	auto it = m_ClientScenarios.find(ClientID);
	if(it == m_ClientScenarios.end())
		return 0;

	return it->second.size();
}

bool CScenarioPlayerManager::RemoveScenarioInternal(int ClientID, int ScenarioID)
{
	// check valid key
	const ScenarioKey key = { ClientID, ScenarioID };
	auto scenarioIt = m_vScenarios.find(key);
	if(scenarioIt == m_vScenarios.end())
		return false;

	// erase scenario
	if(scenarioIt->second && scenarioIt->second->IsRunning())
		scenarioIt->second->Stop();

	m_vScenarios.erase(scenarioIt);

	// erase from client maps
	auto clientIt = m_ClientScenarios.find(ClientID);
	if(clientIt != m_ClientScenarios.end())
	{
		auto& scenarioIDs = clientIt->second;
		scenarioIDs.erase(ScenarioID);

		// erase client from clients maps
		if(scenarioIDs.empty())
			m_ClientScenarios.erase(clientIt);

		return true;
	}

	return false;
}
