#ifndef GAME_SERVER_CORE_TOOLS_PLAYER_SCENARIO_H
#define GAME_SERVER_CORE_TOOLS_PLAYER_SCENARIO_H

// forward
class CGS;
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
class PlayerScenario : public MultiworldIdentifiableData<std::map<int, std::vector<PlayerScenario>>>
{
	struct Step
	{
		ScenarioAction FuncActive {};
		ScenarioAction FuncOnStart {};
		ScenarioAction FuncOnEnd {};
		ScenarioCondition FuncCheckCondition {};
		ConditionPriority Priority {ConditionPriority::CONDITION_AND_TIMER };
		int DelayMs {};

		explicit Step(int delayMs) : DelayMs(delayMs) {}
	};

	int m_ClientID{};
	std::vector<Step> m_vSteps{};
	size_t m_CurrentStepIndex {};
	std::chrono::steady_clock::time_point m_LastStepTime{};
	bool m_Running{};

	bool CanExecuteStep(const Step& step, long elapsedMs) const;
	void ExecuteCurrentStep();

public:
	PlayerScenario() = default;

	CGS* GS() const;
	int GetClientID() const { return m_ClientID; }
	CPlayer* GetPlayer() const { return GS()->GetPlayer(m_ClientID, true); }
	CCharacter* GetCharacter() const { return GetPlayer()->GetCharacter(); }
	bool IsFinished() const { return m_CurrentStepIndex >= m_vSteps.size(); }

	PlayerScenario& Add(int delayMs = -1)
	{
		m_vSteps.emplace_back(delayMs);
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

	void Start(int ClientID);
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
