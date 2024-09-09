#ifndef GAME_SERVER_CORE_TOOLS_PLAYER_SCENARIO_H
#define GAME_SERVER_CORE_TOOLS_PLAYER_SCENARIO_H

// forward
class CGS;
class IServer;
class CPlayer;
class CCharacter;
class PlayerScenario;
using ScenarioAction = std::function<void(const PlayerScenario*)>;
using ScenarioCondition = std::function<bool(const PlayerScenario*)>;

enum class ConditionPriority
{
	CONDITION_AND_TIMER,
	CONDITION_OR_TIMER,
};

// scenario
class PlayerScenario
{
	friend class PlayerScenarioManager;

	struct Step
	{
		ScenarioAction FuncActive {};
		ScenarioAction FuncOnStart {};
		ScenarioAction FuncOnEnd {};
		ScenarioCondition FuncCheckCondition {};
		ConditionPriority Priority {ConditionPriority::CONDITION_AND_TIMER };
		int DelayTick {};

		explicit Step(int delayTick) : DelayTick(delayTick) {}
	};

	int m_ClientID {};
	std::vector<Step> m_vSteps {};
	size_t m_CurrentStepIndex {};
	int m_LastStepTimeTick{};
	bool m_Running{};

	bool CanExecuteStep(const Step& step, long elapsedTick) const;
	void ExecuteCurrentStep();

public:
	PlayerScenario() = default;

	CGS* GS() const;
	IServer* Server() const;
	int GetClientID() const { return m_ClientID; }
	CPlayer* GetPlayer() const;
	CCharacter* GetCharacter() const;

	PlayerScenario& Add(int delayTick = -1)
	{
		m_vSteps.emplace_back(delayTick);
		return *this;
	}

	PlayerScenario& WhenStarted(const ScenarioAction& pfnOnStart)
	{
		if(!m_vSteps.empty())
			m_vSteps.back().FuncOnStart = pfnOnStart;
		return *this;
	}

	PlayerScenario& WhenActive(const ScenarioAction& pfnActive)
	{
		if(!m_vSteps.empty())
			m_vSteps.back().FuncActive = pfnActive;
		return *this;
	}

	PlayerScenario& WhenEnded(const ScenarioAction& pfnOnEnd)
	{
		if(!m_vSteps.empty())
			m_vSteps.back().FuncOnEnd = pfnOnEnd;
		return *this;
	}

	PlayerScenario& CheckCondition(ConditionPriority priority, const ScenarioCondition& pfnCheckCondition)
	{
		if(!m_vSteps.empty())
		{
			m_vSteps.back().Priority = priority;
			m_vSteps.back().FuncCheckCondition = pfnCheckCondition;
		}
		return *this;
	}

private:
	void Start();
	void Stop();
	void Tick();

	bool IsFinished() const { return m_CurrentStepIndex >= m_vSteps.size(); }
};

// scenario manager
class PlayerScenarioManager
{
	int m_ClientID {};
	std::map<int, PlayerScenario> m_Scenarios {};

public:
	PlayerScenarioManager() = default;

	void Init(int ClientID)
	{
		m_ClientID = ClientID;
	}

	void Start(int ScenarioID)
	{
		if(const auto it = m_Scenarios.find(ScenarioID); it != m_Scenarios.end())
			it->second.Start();
	}

	void Add(int ScenarioID, PlayerScenario Scenario)
	{
		dbg_assert(ScenarioID >= 0, "attempting to start a scenario without an indexer");
		Scenario.m_ClientID = m_ClientID;
		m_Scenarios[ScenarioID] = std::move(Scenario);
	}

	bool IsRunning(int ScenarioID) const
	{
		const auto it = m_Scenarios.find(ScenarioID);
		return it != m_Scenarios.end() && it->second.m_Running;
	}

	void Tick()
	{
		for(auto& [id, scenario] : m_Scenarios)
		{
			scenario.Tick();
		}
	}

	void StopAll()
	{
		for(auto& [id, scenario] : m_Scenarios)
		{
			scenario.Stop();
		}
	}

	void Stop(int index)
	{
		if(const auto it = m_Scenarios.find(index); it != m_Scenarios.end())
		{
			it->second.Stop();
		}
	}
};

#endif
