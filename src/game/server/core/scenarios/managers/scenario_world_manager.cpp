#include "scenario_world_manager.h"

void CScenarioWorldManager::UpdateScenarios()
{
	std::erase_if(m_vScenarios, [](const auto& item)
	{
		const auto& [id, pScenario] = item;
		if(pScenario)
		{
			pScenario->Tick();
			return !pScenario->IsRunning();
		}

		return true;
	});
}

void CScenarioWorldManager::RemoveClient(int ClientID)
{
	for(auto& [id, pScenario] : m_vScenarios)
	{
		if(pScenario)
			pScenario->RemoveParticipant(ClientID);
	}
}

void CScenarioWorldManager::HandleClientEnter(int ClientID)
{
	for(auto& [id, pScenario] : m_vScenarios)
	{
		if(pScenario)
			pScenario->AddParticipant(ClientID);
	}
}

bool CScenarioWorldManager::IsActive(int ScenarioID) const
{
	auto it = m_vScenarios.find(ScenarioID);
	return (it != m_vScenarios.end() && it->second && it->second->IsRunning());
}
