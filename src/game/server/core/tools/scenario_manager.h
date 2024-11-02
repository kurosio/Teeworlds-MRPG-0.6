#ifndef GAME_SERVER_CORE_TOOLS_PLAYER_SCENARIO_H
#define GAME_SERVER_CORE_TOOLS_PLAYER_SCENARIO_H

// forward
class CGS;
class IServer;
class CPlayer;
class CCharacter;
class ScenarioBase;
class CEventListenerManager;
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
	friend class ScenarioManager;

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
	int m_ScenarioID {};
	std::vector<Step> m_vSteps {};
	size_t m_CurrentStepIndex {};
	int m_LastStepTimeTick {};
	bool m_Running {};
	int m_Flags {};

protected:
	virtual void OnRegisterEventListener(CEventListenerManager* pListener) {}
	virtual void OnUnregisterEventListener(CEventListenerManager* pListener) {}
	virtual void OnSetupScenario() {}
	virtual bool OnStopConditions() { return true; }

public:
	enum
	{
		FLAG_NONE       = 0,
		FLAG_REPEATABLE = 1 << 0,
	};

	ScenarioBase(int ScenarioID, int Flags = 0) : m_ScenarioID(ScenarioID), m_Flags(Flags) {}
	virtual ~ScenarioBase();

	bool IsRepeatable() const { return m_Flags & FLAG_REPEATABLE; }

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

protected:
	int GetNumSteps() const { return (int)m_vSteps.size(); }
	void Stop();

private:
	void Start();
	void Tick();
	bool IsFinished() const { return m_CurrentStepIndex >= m_vSteps.size(); }
	bool CanExecuteStep(const Step& step, long elapsedTick) const;
	void ExecuteCurrentStep();
};

// scenario manager
class ScenarioManager
{
	int m_ClientID {};
	std::vector<std::unique_ptr<ScenarioBase>> m_Scenarios {};

public:
	ScenarioManager() = default;
	~ScenarioManager()
	{
		StopAll();
	}

	void Init(int ClientID)
	{
		m_ClientID = ClientID;
	}

	void Start(std::unique_ptr<ScenarioBase> pScenario)
	{
		Stop(pScenario->m_ScenarioID);

		pScenario->m_ClientID = m_ClientID;
		pScenario->Start();
		m_Scenarios.emplace_back(std::move(pScenario));
	}

	void StopAll()
	{
		for(auto& pScenario : m_Scenarios)
		{
			if(pScenario)
			{
				pScenario->Stop();
			}
		}
		m_Scenarios.clear();
	}

	void Stop(int ScenarioID)
	{
		const auto it = std::ranges::find_if(m_Scenarios, [ScenarioID](const auto& pScenario) { return pScenario->m_ScenarioID == ScenarioID; });
		if(it != m_Scenarios.end())
		{
			(*it)->Stop();
			m_Scenarios.erase(it);
		}
	}

	void Tick() const
	{
		for(auto& pScenario : m_Scenarios)
		{
			if(pScenario && pScenario->m_Running)
			{
				pScenario->Tick();
			}
		}
	}

	void PostTick()
	{
		std::erase_if(m_Scenarios, [](const std::unique_ptr<ScenarioBase>& pScenario)
		{
			if(pScenario->IsFinished())
			{
				pScenario->Stop();
				return true;
			}
			return false;
		});
	}

	bool IsActive(int ScenarioID)
	{
		const auto it = std::ranges::find_if(m_Scenarios, [ScenarioID](const auto& pScenario) { return pScenario->m_ScenarioID == ScenarioID; });
		return it != m_Scenarios.end();
	}
};

#endif
