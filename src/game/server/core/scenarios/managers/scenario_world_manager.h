#ifndef GAME_SERVER_CORE_TOOLS_SCENARIO_WORLD_MANAGER_H
#define GAME_SERVER_CORE_TOOLS_SCENARIO_WORLD_MANAGER_H

#include <scenarios/base/scenario_base_world.h>
#include <game/server/core/tools/vote_optional.h>
#include <game/server/gamecontext.h>

class CGS;

class CScenarioWorldManager
{
	struct PendingScenarioStart
	{
		bool m_Active = false;
		bool m_Single = false;
		time_t m_StartAt {};
		std::set<int> m_JoinedPlayers {};
		std::set<int> m_DeclinedPlayers {};
	};

	CGS* m_pGS {};
	std::shared_ptr<WorldScenarioBase> m_pScenario {};
	PendingScenarioStart m_PendingStart {};
	int64_t m_ScenarioDeadlineTick { 0 };

	bool IsClientInScenarioWorld(int ClientID) const;
	void PrunePlayersOutsideScenarioWorld();

public:
	explicit CScenarioWorldManager(CGS* pGS) : m_pGS(pGS) { };
	~CScenarioWorldManager() = default;

	template<typename T, typename... Args>
	int RegisterScenario(int StarterCID, int WorldID, bool Single, int DurationSeconds, Args&&... args) requires std::derived_from<T, WorldScenarioBase>
	{
		if(m_pScenario)
			return -1;

		if(WorldID < 0 || WorldID != m_pGS->GetWorldID() || !m_pGS->IsPlayerInWorld(StarterCID, WorldID))
			return -1;

		// initialize
		m_pScenario = std::make_shared<T>(std::forward<Args>(args)...);
		m_pScenario->m_pGS = m_pGS;
		m_pScenario->m_WorldID = WorldID;
		m_PendingStart = {};
		if(DurationSeconds > 0)
			m_ScenarioDeadlineTick = time_get() + time_freq() * DurationSeconds;
		else
			m_ScenarioDeadlineTick = 0;

		// singe world event
		if(Single)
		{
			if(m_pScenario->AddParticipant(StarterCID))
			{
				m_pScenario->Start();
				if(m_pScenario->IsRunning())
					return 1;
			}

			m_pScenario.reset();
			m_ScenarioDeadlineTick = 0;
			return -1;
		}

		// group world event
		m_PendingStart.m_Active = true;
		m_PendingStart.m_StartAt = time_get() + time_freq() * WORLD_SCENARIO_JOIN_VOTE_SEC;
		for(int ClientID = 0; ClientID < MAX_CLIENTS; ++ClientID)
		{
			if(m_pGS->Server()->ClientIngame(ClientID))
				AttachVoteForPlayer(ClientID);
		}

		return 1;
	}

	void UpdateScenarios();
	void RemoveClient(int ClientID);
	std::shared_ptr<WorldScenarioBase> GetActiveScenario() const { return m_pScenario; }
	std::shared_ptr<WorldScenarioBase> GetActiveScenarioByPlayer(int ClientID) const;

private:
	void TryFinalizePendingStart();
	void AttachVoteForPlayer(int ClientID);
};

#endif
