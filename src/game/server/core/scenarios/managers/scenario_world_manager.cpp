#include "scenario_world_manager.h"
#include <game/server/player.h>

bool CScenarioWorldManager::IsClientInScenarioWorld(int ClientID) const
{
	if(!m_pGS || !m_pScenario)
		return false;

	auto* pPlayer = m_pGS->GetPlayer(ClientID);
	return pPlayer && pPlayer->GetCurrentWorldID() == m_pScenario->GetWorldID();
}

void CScenarioWorldManager::PrunePlayersOutsideScenarioWorld()
{
	if(!m_pScenario)
		return;

	std::erase_if(m_PendingStart.m_JoinedPlayers, [this](int ClientID) { return !IsClientInScenarioWorld(ClientID); });
	std::erase_if(m_PendingStart.m_DeclinedPlayers, [this](int ClientID) { return !IsClientInScenarioWorld(ClientID); });

	const auto Participants = m_pScenario->GetParticipants();
	for(const int ClientID : Participants)
	{
		if(!IsClientInScenarioWorld(ClientID))
			m_pScenario->RemoveParticipant(ClientID);
	}
}

void CScenarioWorldManager::UpdateScenarios()
{
	PrunePlayersOutsideScenarioWorld();

	// is timeout pending
	if(m_PendingStart.m_Active && time_get() >= m_PendingStart.m_StartAt)
		TryFinalizePendingStart();

	// update scenario
	if(m_pScenario && !m_PendingStart.m_Active)
	{
		// time limit
		if(m_ScenarioDeadlineTick > 0 && time_get() >= m_ScenarioDeadlineTick)
		{
			for(const int CID : m_pScenario->GetParticipants())
			{
				if(auto* pPlayer = m_pGS->GetPlayer(CID, true))
					pPlayer->KillCharacter();
			}

			m_pGS->ChatWorld(m_pScenario->GetWorldID(), "World scenario", "Finished: time is over.");
			m_pScenario->Stop();
		}

		// update
		m_pScenario->Tick();
		if(!m_pScenario->IsRunning())
		{
			m_pScenario.reset();
			m_ScenarioDeadlineTick = 0;
		}
	}
}

void CScenarioWorldManager::RemoveClient(int ClientID)
{
	// remove from pending
	if(m_PendingStart.m_Active)
	{
		m_PendingStart.m_JoinedPlayers.erase(ClientID);
		m_PendingStart.m_DeclinedPlayers.erase(ClientID);
	}

	// remove from scenario
	if(m_pScenario)
		m_pScenario->RemoveParticipant(ClientID);
}

std::shared_ptr<WorldScenarioBase> CScenarioWorldManager::GetActiveScenarioByPlayer(int ClientID) const
{
	return m_pScenario && m_pScenario->IsRunning() && m_pScenario->GetParticipants().contains(ClientID) && IsClientInScenarioWorld(ClientID) ? m_pScenario : nullptr;
}

void CScenarioWorldManager::TryFinalizePendingStart()
{
	// is already ending
	if(!m_PendingStart.m_Active)
		return;

	m_PendingStart.m_Active = false;

	// check valid scenario
	if(!m_pScenario)
		return;

	PrunePlayersOutsideScenarioWorld();

	// add joined players to scenario participant
	int ParticipantsAdded = 0;
	for(const int joinedClientID : m_PendingStart.m_JoinedPlayers)
	{
		if(m_pScenario->AddParticipant(joinedClientID))
			++ParticipantsAdded;
	}

	// cancel is empty
	if(m_pScenario->GetParticipants().empty())
	{
		m_pGS->ChatWorld(m_pScenario->GetWorldID(), "World scenario", "Cancelled: no participants joined.");
		m_pScenario.reset();
		m_ScenarioDeadlineTick = 0;
		return;
	}

	// start world scenario
	m_pScenario->Start();
	if(!m_pScenario->IsRunning())
	{
		m_pScenario.reset();
		m_ScenarioDeadlineTick = 0;
		return;
	}

	m_pGS->ChatWorld(m_pScenario->GetWorldID(), "World scenario", "Started: {} participant(s).", ParticipantsAdded);
}

void CScenarioWorldManager::AttachVoteForPlayer(int ClientID)
{
	// is already ending
	if(!m_PendingStart.m_Active)
		return;

	// check valid time for attach & valid
	if(!m_pScenario || time_get() >= m_PendingStart.m_StartAt)
		return;

	// check valid client world ID
	auto* pPlayer = m_pGS->GetPlayer(ClientID);
	if(!pPlayer || pPlayer->GetCurrentWorldID() != m_pScenario->GetWorldID())
		return;

	// register callback options vote
	const int remainingSeconds = std::max(1, (int)((m_PendingStart.m_StartAt - time_get()) / time_freq()));
	auto* pOption = CVoteOptional::Create(ClientID, remainingSeconds, "Join the scenario.");
	pOption->RegisterCallback([this](CPlayer* pVotePlayer, bool isJoined)
	{
		if(!m_PendingStart.m_Active || !pVotePlayer)
			return;

		const int playerCID = pVotePlayer->GetCID();
		m_PendingStart.m_JoinedPlayers.erase(playerCID);
		m_PendingStart.m_DeclinedPlayers.erase(playerCID);

		if(!m_pScenario || pVotePlayer->GetCurrentWorldID() != m_pScenario->GetWorldID())
			return;

		if(isJoined)
			m_PendingStart.m_JoinedPlayers.insert(playerCID);
		else
			m_PendingStart.m_DeclinedPlayers.insert(playerCID);
	});

	pOption->RegisterCloseCondition([this](CPlayer* pVotePlayer)
	{
		if(!m_PendingStart.m_Active || !pVotePlayer)
			return true;

		if(time_get() >= m_PendingStart.m_StartAt)
		{
			TryFinalizePendingStart();
			return true;
		}

		return !m_pScenario || pVotePlayer->GetCurrentWorldID() != m_pScenario->GetWorldID();
	});
}
