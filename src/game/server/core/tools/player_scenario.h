#ifndef GAME_SERVER_CORE_TOOLS_PLAYER_SCENARIO_H
#define GAME_SERVER_CORE_TOOLS_PLAYER_SCENARIO_H

// forward
class CGS;
class IServer;
class CPlayer;
class CCharacter;
class ScenarioBase;
using ScenarioAction = std::function<void(const ScenarioBase*)>;
using ScenarioCondition = std::function<bool(const ScenarioBase*)>;

enum class ConditionPriority
{
	CONDITION_AND_TIMER,
	CONDITION_OR_TIMER,
};

// scenario
class ScenarioBase
{
	friend class PlayerScenarioManager;

	struct Step
	{
		ScenarioAction FuncActive {};
		ScenarioAction FuncOnStart {};
		ScenarioAction FuncOnEnd {};
		ScenarioCondition FuncCheckCondition {};
		ConditionPriority Priority { ConditionPriority::CONDITION_AND_TIMER };
		int DelayTick {};

		explicit Step(int delayTick) : DelayTick(delayTick) {}
	};

	int m_ClientID {};
	std::vector<Step> m_vSteps {};
	size_t m_CurrentStepIndex {};
	int m_LastStepTimeTick {};
	bool m_Running {};

protected:
	virtual void SetupScenario() {}

public:
	ScenarioBase() = default;
	virtual ~ScenarioBase();

	virtual void OnRegisterEventListener() {}
	virtual void OnUnregisterEventListener() {}

	CGS* GS() const;
	IServer* Server() const;
	int GetClientID() const { return m_ClientID; }
	CPlayer* GetPlayer() const;
	CCharacter* GetCharacter() const;

	ScenarioBase& AddStep(int delayTick = -1)
	{
		m_vSteps.emplace_back(delayTick);
		return *this;
	}

	ScenarioBase& WhenStarted(const ScenarioAction& pfnOnStart)
	{
		if(!m_vSteps.empty())
		{
			m_vSteps.back().FuncOnStart = pfnOnStart;
		}
		return *this;
	}

	ScenarioBase& WhenActive(const ScenarioAction& pfnActive)
	{
		if(!m_vSteps.empty())
		{
			m_vSteps.back().FuncActive = pfnActive;
		}
		return *this;
	}

	ScenarioBase& WhenFinished(const ScenarioAction& pfnOnEnd)
	{
		if(!m_vSteps.empty())
		{
			m_vSteps.back().FuncOnEnd = pfnOnEnd;
		}
		return *this;
	}

	ScenarioBase& CheckCondition(ConditionPriority priority, const ScenarioCondition& pfnCheckCondition)
	{
		if(!m_vSteps.empty())
		{
			auto& step = m_vSteps.back();
			step.Priority = priority;
			step.FuncCheckCondition = pfnCheckCondition;
		}
		return *this;
	}

private:
	void Start();
	void Stop();
	void Tick();
	bool IsFinished() const { return m_CurrentStepIndex >= m_vSteps.size(); }
	bool CanExecuteStep(const Step& step, long elapsedTick) const;
	void ExecuteCurrentStep();
};

// scenario manager
class PlayerScenarioManager
{
	int m_ClientID {};
	std::map<int, ScenarioBase*> m_Scenarios {};

public:
	PlayerScenarioManager() = default;
	~PlayerScenarioManager()
	{
		StopAll();
	}

	void Init(int ClientID)
	{
		m_ClientID = ClientID;
	}

	void Start(int ScenarioID)
	{
		if(const auto it = m_Scenarios.find(ScenarioID); it != m_Scenarios.end())
			it->second->Start();
	}

	void Add(int ScenarioID, ScenarioBase* pScenario)
	{
		dbg_assert(ScenarioID >= 0, "attempting to start a scenario without an indexer");
		pScenario->m_ClientID = m_ClientID;
		m_Scenarios[ScenarioID] = pScenario;
	}

	bool IsRunning(int ScenarioID) const
	{
		const auto it = m_Scenarios.find(ScenarioID);
		return it != m_Scenarios.end() && it->second->m_Running;
	}

	void Tick()
	{
		for(auto& [id, pScenario] : m_Scenarios)
		{
			pScenario->Tick();
		}
	}

	void StopAll()
	{
		for(auto& [id, pScenario] : m_Scenarios)
		{
			pScenario->Stop();
			delete pScenario;
		}
		m_Scenarios.clear();
	}

	void Stop(int index)
	{
		if(const auto it = m_Scenarios.find(index); it != m_Scenarios.end())
		{
			it->second->Stop();
			delete it->second;
			m_Scenarios.erase(it);
		}
	}
};

#endif
