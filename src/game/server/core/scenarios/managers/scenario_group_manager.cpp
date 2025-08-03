#include "scenario_group_manager.h"

void CScenarioGroupManager::UpdateScenarios()
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
	return (it != m_vScenarios.end() && it->second && it->second->IsRunning());
}