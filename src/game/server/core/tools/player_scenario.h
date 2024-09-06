#ifndef GAME_SERVER_CORE_TOOLS_PLAYER_SCENARIO_H
#define GAME_SERVER_CORE_TOOLS_PLAYER_SCENARIO_H

// forward
class CGS;
class CPlayer;
class CCharacter;
class PlayerScenario;
using ScenarioAction = std::function<void(const PlayerScenario*)>;
using ScenarioCondition = std::function<bool(const PlayerScenario*)>;

// scenario
class PlayerScenario : public MultiworldIdentifiableData<std::map<int, std::vector<PlayerScenario>>>
{
	struct Step
	{
		ScenarioAction FuncAction {};
		ScenarioCondition FuncCondition {};
		int DelayMs {};

		Step(ScenarioAction pfnAction, int delayMs, ScenarioCondition pfnCondition)
			: FuncAction(std::move(pfnAction)), FuncCondition(std::move(pfnCondition)), DelayMs(delayMs) {}
	};

	int m_ClientID{};
	std::vector<Step> m_vSteps{};
	size_t m_CurrentStepIndex {};
	std::chrono::steady_clock::time_point m_LastStepTime{};
	bool m_Running{};

	bool CanExecuteStep(const Step& step, long elapsedMs) const;
	void ExecuteCurrentStep();

public:
	explicit PlayerScenario(int clientID) : m_ClientID(clientID) {}

	CGS* GS() const;
	int GetClientID() const { return m_ClientID; }
	CPlayer* GetPlayer() const { return GS()->GetPlayer(m_ClientID, true); }
	CCharacter* GetCharacter() const { return GetPlayer()->GetCharacter(); }
	bool IsFinished() const { return m_CurrentStepIndex >= m_vSteps.size(); }

	void AddStep(const ScenarioAction& pfnAction, int delayMs)
	{
		m_vSteps.emplace_back(pfnAction, delayMs, nullptr);
	}

	void AddStep(const ScenarioAction& pfnAction, const ScenarioCondition& pfnCondition)
	{
		m_vSteps.emplace_back(pfnAction, -1, pfnCondition);
	}

	void Start();
	void Stop();
	void Tick();

	static void Tick(int ClientID)
	{
		if(const auto it = m_pData.find(ClientID); it != m_pData.end())
		{
			std::erase_if(it->second, [](PlayerScenario& scenario)
			{
				scenario.Tick();
				return scenario.IsFinished();
			});

			if(it->second.empty())
				m_pData.erase(it);
		}
	}
};

#endif
