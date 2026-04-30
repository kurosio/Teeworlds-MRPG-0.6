#include "scenario_world_manager.h"
#include <game/server/player.h>

void CScenarioWorldManager::UpdateScenarios()
{
	std::vector<int> vPendingToFinalize;
	for(const auto& [scenarioID, pendingStart] : m_vPendingStarts)
	{
		if(time_get() >= pendingStart.m_StartAt)
			vPendingToFinalize.push_back(scenarioID);
	}

	for(const int scenarioID : vPendingToFinalize)
		TryFinalizePendingStart(scenarioID);

	std::erase_if(m_vScenarios, [this](const auto& item)
	{
		const auto& [id, pScenario] = item;
		if(m_vPendingStarts.contains(id))
			return false;

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
	for(auto& [scenarioID, pendingStart] : m_vPendingStarts)
	{
		pendingStart.m_JoinedPlayers.erase(ClientID);
		pendingStart.m_DeclinedPlayers.erase(ClientID);
	}

	for(auto& [id, pScenario] : m_vScenarios)
	{
		if(pScenario)
			pScenario->RemoveParticipant(ClientID);
	}
}

bool CScenarioWorldManager::IsActive(int ScenarioID) const
{
	auto it = m_vScenarios.find(ScenarioID);
	return (it != m_vScenarios.end() && it->second && it->second->IsRunning());
}

void CScenarioWorldManager::TryFinalizePendingStart(int ScenarioID)
{
	auto pendingIt = m_vPendingStarts.find(ScenarioID);
	if(pendingIt == m_vPendingStarts.end())
		return;

	PendingScenarioStart pendingStart = std::move(pendingIt->second);
	m_vPendingStarts.erase(pendingIt);

	if(!pendingStart.m_pScenario)
	{
		m_vScenarios.erase(ScenarioID);
		return;
	}

	for(const int joinedClientID : pendingStart.m_JoinedPlayers)
		pendingStart.m_pScenario->AddParticipant(joinedClientID);

	if(pendingStart.m_pScenario->GetParticipants().empty())
	{
		m_pGS->ChatWorld(pendingStart.m_pScenario->GetWorldID(), "World scenario", "Cancelled: no participants joined.");
		m_vScenarios.erase(ScenarioID);
		return;
	}

	pendingStart.m_pScenario->Start();
	if(!pendingStart.m_pScenario->IsRunning())
	{
		m_vScenarios.erase(ScenarioID);
		return;
	}

	m_pGS->ChatWorld(pendingStart.m_pScenario->GetWorldID(), "World scenario", "Started: {} participant(s).", (int)pendingStart.m_JoinedPlayers.size());
}

void CScenarioWorldManager::AttachVoteForPlayer(int ScenarioID, int ClientID)
{
	auto pendingIt = m_vPendingStarts.find(ScenarioID);
	if(pendingIt == m_vPendingStarts.end())
		return;

	PendingScenarioStart& pendingStart = pendingIt->second;
	if(!pendingStart.m_pScenario || time_get() >= pendingStart.m_StartAt)
		return;

	auto* pPlayer = m_pGS->GetPlayer(ClientID);
	if(!pPlayer || pPlayer->GetCurrentWorldID() != pendingStart.m_pScenario->GetWorldID())
		return;

	const int remainingSeconds = std::max(1, (int)((pendingStart.m_StartAt - time_get()) / time_freq()));
	auto* pOption = CVoteOptional::Create(ClientID, remainingSeconds, "Enter world scenario.");

	pOption->RegisterCallback([this, ScenarioID](CPlayer* pVotePlayer, bool isJoined)
	{
		auto pendingItInner = m_vPendingStarts.find(ScenarioID);
		if(pendingItInner == m_vPendingStarts.end() || !pVotePlayer)
			return;

		PendingScenarioStart& pendingStartInner = pendingItInner->second;
		const int playerCID = pVotePlayer->GetCID();
		pendingStartInner.m_JoinedPlayers.erase(playerCID);
		pendingStartInner.m_DeclinedPlayers.erase(playerCID);

		if(isJoined)
			pendingStartInner.m_JoinedPlayers.insert(playerCID);
		else
			pendingStartInner.m_DeclinedPlayers.insert(playerCID);
	});

	pOption->RegisterCloseCondition([this, ScenarioID](CPlayer* pVotePlayer)
	{
		auto pendingItInner = m_vPendingStarts.find(ScenarioID);
		if(pendingItInner == m_vPendingStarts.end() || !pVotePlayer)
			return true;

		if(time_get() >= pendingItInner->second.m_StartAt)
		{
			TryFinalizePendingStart(ScenarioID);
			return true;
		}

		const auto* pScenario = pendingItInner->second.m_pScenario.get();
		return !pScenario || pVotePlayer->GetCurrentWorldID() != pScenario->GetWorldID();
	});
}
