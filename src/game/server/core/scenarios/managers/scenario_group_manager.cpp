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

void CScenarioGroupManager::StopScenario(int ScenarioID)
{
	auto it = m_vScenarios.find(ScenarioID);
	if(it == m_vScenarios.end() || !it->second)
		return;

	it->second->Stop();
}

bool CScenarioGroupManager::IsActive(int ScenarioID) const
{
	auto it = m_vScenarios.find(ScenarioID);
	return (it != m_vScenarios.end() && it->second && it->second->IsRunning());
}