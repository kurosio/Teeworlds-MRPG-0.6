#ifndef GAME_SERVER_CORE_TOOLS_SCENARIO_PLAYER_MANAGER_H
#define GAME_SERVER_CORE_TOOLS_SCENARIO_PLAYER_MANAGER_H

#include "scenario_base.h"

class CGS;

// hashing function for fast iterate unordered map with pair <client, scenario>
struct PairIntHash
{
	std::size_t operator()(const std::pair<int, int>& p) const noexcept
	{
		auto hash1 = std::hash<int> {}(p.first);
		auto hash2 = std::hash<int> {}(p.second);
		return hash1 ^ (hash2 + 0x9e3779b9 + (hash1 << 6) + (hash1 >> 2));
	}
};

class CScenarioPlayerManager
{
	using ScenarioKey = std::pair<int, int>;
	using ScenarioMap = std::unordered_map<ScenarioKey, std::unique_ptr<ScenarioBase>, PairIntHash>;
	using ClientScenarioMap = std::unordered_map<int, std::unordered_set<int>>;

	CGS* m_pGS {};
	int m_NextScenarioID = 1;
	ScenarioMap m_vScenarios {};
	ClientScenarioMap m_ClientScenarios {};

public:
	explicit CScenarioPlayerManager(CGS* pGS) : m_pGS(pGS) { };
	~CScenarioPlayerManager() = default;

	template<typename T, typename... Args>
	int RegisterScenario(int ClientID, Args&&... args)
	{
		static_assert(std::is_base_of_v<PlayerScenarioBase, T>, "T must derive from PlayerScenarioBase for CScenarioPlayerManager");

		// initialize variables
		int scenarioID = m_NextScenarioID++;
		auto pScenario = std::make_unique<T>(std::forward<Args>(args)...);

		// initialize scenario
		pScenario->m_pGS = m_pGS;
		pScenario->m_ClientID = ClientID;
		pScenario->m_ScenarioID = scenarioID;
		pScenario->Start();

		// check is succesful running
		if(!pScenario->IsRunning())
			return -1;

		// check succesful incerting
		const ScenarioKey key = { ClientID, scenarioID };
		auto [it, inserted] = m_vScenarios.emplace(key, std::move(pScenario));
		if(!inserted)
			return -1;

		m_ClientScenarios[ClientID].insert(scenarioID);
		return scenarioID;
	}

	void UpdateClientScenarios(int ClientID);

	void Stop(int ClientID, int ScenarioID) { RemoveScenarioInternal(ClientID, ScenarioID); }
	void StopAll(int ClientID);

	template<typename T = ScenarioBase>
	T* GetScenario(int ClientID, int ScenarioID)
	{
		static_assert(std::is_base_of_v<ScenarioBase, T>, "T must derive from ScenarioBase");

		auto it = m_vScenarios.find({ ClientID, ScenarioID });
		if(it != m_vScenarios.end() && it->second)
			return dynamic_cast<T*>(it->second.get());

		return nullptr;
	}

	PlayerScenarioBase* GetPlayerScenario(int ClientID, int ScenarioID);
	bool IsActive(int ClientID, int ScenarioID) const noexcept;
	bool HasActiveScenarios(int ClientID) const noexcept;
	std::vector<int> GetActiveScenarios(int ClientID) const;
	size_t GetScenarioCount(int ClientID) const noexcept;
	size_t GetTotalScenarioCount() const noexcept { return m_vScenarios.size(); }
	void RemoveClient(int ClientID) { StopAll(ClientID); }

private:
	bool RemoveScenarioInternal(int ClientID, int ScenarioID);
};

#endif // GAME_SERVER_CORE_TOOLS_SCENARIO_PLAYER_MANAGER_H